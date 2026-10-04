#include "application.h"
#include "l3g4200d.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"  // StaticTask_t: the control block of a statically created thread
#include "csp/csp4cmsis.h"
#include <cstdio>
#include <cmath>
extern "C" {
#include "main.h"
}

// --- Configuration ---
// PB0 (EXTI0, rising edge), wired to the gyro's DRDY/INT2 pin.
#define GYRO_DRDY_PIN GPIO_PIN_0
// SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI), initialised by main.c; chip select PB12.
extern SPI_HandleTypeDef hspi2;

using namespace csp;

struct trigger_t {};

// --- Trigger channel: written by the data-ready interrupt ---
// An interrupt cannot use a rendezvous channel: it cannot wait for a partner. It writes into
// a buffered channel instead, through the channel's ISR writer end. Capacity 1 with the
// KeepNewest policy: the write never blocks and never fails. The trigger carries no data, it
// only says "a new sample is waiting in the sensor", and the sensor keeps only its latest
// sample: triggers that arrive while L3g4200d is busy merge into one, and the next read gets
// the latest sample. A trigger is never lost, so the data-ready line, which goes low only when
// the sample is read, cannot get stuck high.
static SamplingBufferedChannel<trigger_t, 1, BufferPolicy::KeepNewest> g_trigger_chan;
static IsrChanout<trigger_t> g_trigger_isr = g_trigger_chan.isrWriter();

struct Message {
	float x,y,z;
};

struct Result {
	float result;
};

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GYRO_DRDY_PIN) {
        g_trigger_isr.putFromISR(trigger_t{});  // never blocks; with KeepNewest always succeeds
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
        SleepFor(Milliseconds(10).to_ticks());
        L3G4200D_t gyro;
        gyro.hspi = &hspi2;
        gyro.cs_port = GPIOB;
        gyro.cs_pin = GPIO_PIN_12;

        if (HAL_ERROR == L3G4200D_Init(&gyro, L3G4200D_SCALE_250DPS)){
            printf("HAL-ERROR during init\r\n");
        }
        if (HAL_ERROR == L3G4200D_EnableDRDY(&gyro)){
            printf("HAL-ERROR during DRDY enable\r\n");
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

// Start order. MainApp runs at a higher priority than the network it launches, so
// Run(..., StaticNetwork) only creates the three process threads and returns: none of them
// can preempt MainApp, and they first run after MainApp has printed its banner and exited.
// All stay below CubeMX's defaultTask (osPriorityNormal).
static constexpr osPriority_t MAIN_APP_PRIORITY = osPriorityBelowNormal;
static constexpr osPriority_t NETWORK_PRIORITY  = osPriorityLow;

// MainApp's stack and control block are static: creating the thread takes no heap.
// CMSIS-RTOS2 counts the stack in bytes: 384 words = 1.5 KB. Measured on the NUCLEO-G474RE:
// MainApp uses 572 B (Debug, -O0) and 308 B (Release, -Os) of it.
alignas(8) static uint32_t mainAppStack[384];
static StaticTask_t mainAppControlBlock;

void MainApp_Task(void* argument) {
    (void)argument;
    osDelay(10);

    printf("\r\n--- Launching CSP Static Network (Zero-Heap) ---\r\n");

    static Channel<Message>  msg_chan;      // unbuffered – can be buffered if needed
    static Channel<Result> result_chan;

    static L3g4200d pL3g4200d(msg_chan.writer());
    static ShakeDetect pShakeDetect(msg_chan.reader(), result_chan.writer());
    static UI pUI(result_chan.reader());

    // Run parallel processes using static execution
    Run(
        InParallel(pL3g4200d, pShakeDetect, pUI),
        ExecutionMode::StaticNetwork,
        NETWORK_PRIORITY
    );
    // Run() returns immediately in StaticNetwork mode; the thread must
    // end itself rather than fall off the end of the function.
    osThreadExit();
}

void csp_app_main_init(void) {
    osThreadAttr_t attr = {};
    attr.name       = "MainApp";
    attr.stack_mem  = mainAppStack;
    attr.stack_size = sizeof(mainAppStack);
    attr.cb_mem     = &mainAppControlBlock;
    attr.cb_size    = sizeof(mainAppControlBlock);
    attr.priority   = MAIN_APP_PRIORITY;
    if (osThreadNew(MainApp_Task, NULL, &attr) == NULL) {
        printf("ERROR: MainApp_Task creation failed!\r\n");
    }
}
