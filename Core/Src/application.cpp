#include "application.h"
#include "l3g4200d.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"  // StaticTask_t
#include "csp/csp4cmsis.h"
#include <cstdio>
#include <cmath>
extern "C" {
#include "main.h"
}

// PB0 (EXTI0, rising edge), wired to the gyro's DRDY/INT2 pin.
#define GYRO_DRDY_PIN GPIO_PIN_0
// SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI), initialised by main.c; chip select PB12.
extern SPI_HandleTypeDef hspi2;

using namespace csp;

// Written by the data-ready interrupt: "a new sample is waiting". KeepNewest, capacity 1:
// triggers that arrive while L3g4200d is busy merge into one, and none is lost, so the
// data-ready line (low only after a read) cannot get stuck high.
static BufferedChannel<bool, 1, BufferPolicy::KeepNewest> g_trigger_chan;

struct Message {
    float x, y, z;
};

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GYRO_DRDY_PIN) {
        g_trigger_chan.isrWriter().putFromISR(true);
    }
}

class L3g4200d : public CSProcessStatic<256> {
    Chanout<Message> out;

public:
    explicit L3g4200d(Chanout<Message> w) : out(w) {}

    void run() override {
        bool trigger;
        auto trigger_reader = g_trigger_chan.reader();
        SleepFor(Milliseconds(10));
        L3G4200D_t gyro;
        gyro.hspi = &hspi2;
        gyro.cs_port = GPIOB;
        gyro.cs_pin = GPIO_PIN_12;

        if (HAL_ERROR == L3G4200D_Init(&gyro, L3G4200D_SCALE_250DPS)) {
            printf("HAL-ERROR during init\r\n");
        }
        if (HAL_ERROR == L3G4200D_EnableDRDY(&gyro)) {
            printf("HAL-ERROR during DRDY enable\r\n");
            return;
        }

        Message msg;
        L3G4200D_ReadDPS(&gyro, &msg.x, &msg.y, &msg.z);  // clears a data-ready already pending
        while (true) {
            trigger_reader >> trigger;
            L3G4200D_ReadDPS(&gyro, &msg.x, &msg.y, &msg.z);
            out << msg;
        }
    }
};

class ShakeDetect : public CSProcessStatic<256> {
    Chanin<Message> in;
    Chanout<bool> out;  // true = shake started, false = shake ended

public:
    ShakeDetect(Chanin<Message> r, Chanout<bool> w) : in(r), out(w) {}

    void run() override {
        const float alpha = 0.02f;          // mean filter speed
        const int window_size = 10;         // ~100 ms at 100 Hz
        const float threshold_on  = 3000.0f;
        const float threshold_off = 1500.0f;

        float mean = 0.0f;
        float energy = 0.0f;
        int count = 0;
        bool shaking = false;
        Message msg;

        while (true) {
            in >> msg;

            float mag = sqrtf(msg.x * msg.x + msg.y * msg.y + msg.z * msg.z);
            mean += alpha * (mag - mean);   // high-pass: subtract the running mean
            float hp = mag - mean;
            energy += hp * hp;
            count++;

            if (count >= window_size) {
                float avg_energy = energy / count;
                if (!shaking && avg_energy > threshold_on) {        // hysteresis
                    shaking = true;
                    out << true;
                } else if (shaking && avg_energy < threshold_off) {
                    shaking = false;
                    out << false;
                }
                energy = 0.0f;
                count = 0;
            }
        }
    }
};

class UI : public CSProcessStatic<256> {
    Chanin<bool> in;

public:
    explicit UI(Chanin<bool> r) : in(r) {}

    // The only process that prints while the network runs (L3g4200d prints only before
    // its first sample).
    void run() override {
        bool shaking;
        while (true) {
            in >> shaking;
            if (shaking) {
                printf(">>> SHAKE DETECTED! <<<\r\n");
            } else {
                printf("Shake ended.\r\n");
            }
        }
    }
};

// MainApp runs above the network, so the processes first run after MainApp has printed
// its banner and exited.
static constexpr osPriority_t MAIN_APP_PRIORITY = osPriorityBelowNormal;
static constexpr osPriority_t NETWORK_PRIORITY  = osPriorityLow;

// Static stack (384 words = 1.5 KB) and control block: no heap.
alignas(8) static uint32_t mainAppStack[384];
static StaticTask_t mainAppControlBlock;

void MainApp_Task(void* argument) {
    (void)argument;
    osDelay(10);
    printf("\r\n--- Launching CSP Static Network (Zero-Heap) ---\r\n");

    static Channel<Message> msg_chan;
    static Channel<bool> result_chan;

    static L3g4200d pL3g4200d(msg_chan.writer());
    static ShakeDetect pShakeDetect(msg_chan.reader(), result_chan.writer());
    static UI pUI(result_chan.reader());

    Run(InParallel(pL3g4200d, pShakeDetect, pUI), ExecutionMode::StaticNetwork, NETWORK_PRIORITY);
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
