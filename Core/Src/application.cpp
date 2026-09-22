#include "application.h"
#include "l3g4200d.h"
#include "csp/csp4cmsis.h"
#include <cstdio>
#include <cmath>
extern "C" {
#include "main.h"
#include "cmsis_os.h"
}

// --- Configuration ---
// PB0, configured GPIO_MODE_IT_RISING in main.c's MX_GPIO_Init(). (Was
// GPIO_PIN_1 -- corrected to match the pin main.c actually arms for EXTI.)
#define GYRO_INT1_PIN GPIO_PIN_0
// main.c initializes hspi2, not hspi3; the CS pin it configures
// (GPIOB/GPIO_PIN_12) matches what this driver expects below, so hspi2 is
// almost certainly the intended handle. Update this if your board really
// does use SPI3 and main.c is the one that's incomplete.
extern SPI_HandleTypeDef hspi2;

using namespace csp;

struct trigger_t {};
static Channel<trigger_t> g_trigger_chan;

struct Message {
	float x,y,z;
};

struct Result {
	float result;
};

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GYRO_INT1_PIN) {
        // putFromISR() performs the yield internally, on the caller's
        // behalf, before it returns -- no separate xHigherPriorityTaskWoken
        // handling or portYIELD_FROM_ISR() call is needed here.
        g_trigger_chan.writer().putFromISR(trigger_t{});
    }
}

class L3g4200d : public CSProcessStatic<256> {
private:
	Chanout<Message> out;
public:
	L3g4200d(Chanout<Message> w) : out(w) {}
	const char* name() const override { return "L3g4200d"; }

    void run() override {
    	trigger_t t;
    	auto trigger_reader = g_trigger_chan.reader();
        vTaskDelay(pdMS_TO_TICKS(10));
        L3G4200D_t gyro;
        gyro.hspi = &hspi2;
        gyro.cs_port = GPIOB;
        gyro.cs_pin = GPIO_PIN_12;

        if (HAL_ERROR == L3G4200D_Init(&gyro, L3G4200D_SCALE_250DPS)){
            printf("HAL-ERROR during init\r\n");
        }
        if (HAL_ERROR == L3G4200D_EnableINT1(&gyro)){
            printf("HAL-ERROR during INT1 enable\r\n");
            return;
        }

        Message msg;
        L3G4200D_ReadDPS(&gyro, &msg.x, &msg.y, &msg.z);
        while(true) {
            trigger_reader >> t;
            L3G4200D_ReadDPS(&gyro, &msg.x, &msg.y, &msg.z);
            out << msg;
        }
    }
};

class ShakeDetect : public CSProcessStatic<256> {
private:
    Chanin<Message> in;
    Chanout<Result> out;

public:
    ShakeDetect(Chanin<Message> r, Chanout<Result> w)
        : in(r), out(w) {}
    const char* name() const override { return "ShakeDetect"; }

    void run() override {
        const float alpha = 0.02f;          // mean filter speed
        const int window_size = 10;         // ~100ms if 100Hz
        const float threshold_on  = 3000.0f;
        const float threshold_off = 1500.0f;

        float mean = 0.0f;
        float energy = 0.0f;
        int count = 0;

        bool shake_state = false;

        Message msg;
        Result result;

        while (true) {

            in >> msg;

            // --- 1. Magnitude ---
            float mag = sqrtf(msg.x*msg.x +
                              msg.y*msg.y +
                              msg.z*msg.z);

            // --- 2. High-pass via running mean ---
            mean += alpha * (mag - mean);
            float hp = mag - mean;

            // --- 3. Accumulate energy ---
            energy += hp * hp;
            count++;

            if (count >= window_size) {

                float avg_energy = energy / count;

                // --- 4. Hysteresis detection ---
                if (!shake_state && avg_energy > threshold_on) {
                    shake_state = true;
                    result.result = 1.0f;
                    out << result;
                }
                else if (shake_state && avg_energy < threshold_off) {
                    shake_state = false;
                    result.result = 0.0f;
                    out << result;
                }

                energy = 0.0f;
                count = 0;
            }
        }
    }
};

class UI : public CSProcessStatic<256> {
private:
    Chanin<Result> in;

public:
    UI(Chanin<Result> r) : in(r) {}
    const char* name() const override { return "UI"; }

    void run() override {
        Result res;
        while (true) {
            in >> res;

            if (res.result > 0.5f) {
                printf(">>> SHAKE DETECTED! <<<\r\n");
            } else {
                printf("Shake ended.\r\n");
            }
        }
    }
};

void MainApp_Task(void* params) {
    vTaskDelay(pdMS_TO_TICKS(10));

    printf("\r\n--- Launching CSP Static Network (Zero-Heap) ---\r\n");

    static Channel<Message>  msg_chan;      // unbuffered – can be buffered if needed
    static Channel<Result> result_chan;

    static L3g4200d pL3g4200d(msg_chan.writer());
    static ShakeDetect pShakeDetect(msg_chan.reader(), result_chan.writer());
    static UI pUI(result_chan.reader());

    // Run parallel processes using static execution
    Run(
        InParallel(pL3g4200d, pShakeDetect, pUI),
        ExecutionMode::StaticNetwork
    );
    // Run() returns immediately in StaticNetwork mode; the task must
    // delete itself rather than fall off the end of the function.
    vTaskDelete(NULL);
}

void csp_app_main_init(void) {
	// MainApp_Task's own stack: it only builds three static objects and
	// calls Run() -- the gyro/SPI work all happens on L3g4200d's own
	// statically-allocated CSProcessStatic<256> stack, not this one. 512
	// words (2048 bytes) is ample headroom for printf/vsnprintf, which can
	// be surprisingly stack-hungry under newlib. This is also a
	// heap-backed allocation (xTaskCreate, not xTaskCreateStatic) -- see
	// configTOTAL_HEAP_SIZE in FreeRTOSConfig.h, which previously was too
	// small to satisfy even the old 2048-word request.
	BaseType_t status = xTaskCreate(MainApp_Task, "MainApp", 512, NULL, tskIDLE_PRIORITY + 3, NULL);
	if (status != pdPASS) {
	    printf("ERROR: MainApp_Task creation failed!\r\n");
	}
}
