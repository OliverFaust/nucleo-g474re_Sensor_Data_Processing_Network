# CSP4CMSIS Shake Detection
### NUCLEO-G474RE + L3G4200D Gyroscope

This project demonstrates interrupt-driven shake detection on the STM32 NUCLEO-G474RE using the L3G4200D gyroscope. It is a port of the original [NUCLEO-F401RE version](https://github.com/OliverFaust/CSP4CMSIS-shake-detection-NUCLEO-F401RE-) by Oliver Faust, adapted for the G4 series (different SPI instance, clock tree, and interrupt line).

It uses:

- STM32CubeIDE
- FreeRTOS with the CMSIS-RTOS v2 API (STM32CubeMX `CMSIS_V2` interface)
- CSP4CMSIS 2.0.1 (Communicating Sequential Processes), in `lib/csp4cmsis/` (unmodified; see `lib/csp4cmsis/VERSION`)
- Interrupt-triggered SPI acquisition (the sensor's data-ready interrupt triggers each SPI read)
- Real-time signal processing

The system detects when the sensor is shaken and prints a message to the serial terminal
(LPUART1, the ST-LINK virtual COM port, 115200 baud).

Tested with:

| Tool | Version |
|---|---|
| STM32CubeIDE | 2.1.0 (GNU Tools for STM32 14.3.rel1) |
| STM32CubeMX (only to regenerate code) | 6.17.0 |
| STM32Cube FW_G4 | V1.6.3 (FreeRTOS 10.3.1) |
| CSP4CMSIS | 2.0.1 |

---

# 1. Hardware Used

- STM32 NUCLEO-G474RE
- L3G4200D 3-axis digital gyroscope breakout
- USB connection for power + serial output

---

# 2. Pin Connections

This project uses **SPI2** on the NUCLEO-G474RE.

| Gyro Pin   | STM32 Pin          | Description                  |
|------------|---------------------|-------------------------------|
| VDD/VDD_IO | 3.3V                | Power                        |
| GND        | GND                 | Ground                       |
| SCL/SPC    | PB13                | SPI2 SCK                     |
| SDI/SDA    | PB15                | SPI2 MOSI                    |
| SDO        | PB14                | SPI2 MISO                    |
| CS         | PB12                | Chip Select (software controlled) |
| DRDY/INT2  | PB0 (CN7 pin 34)    | Data Ready Interrupt (EXTI0) |

Important:
- SPI is configured in master mode, Mode 3 (CPOL=1, CPHA=1), 8-bit frames, ~5.3 MHz (SYSCLK 170 MHz / APB1 ÷1 / prescaler 32), keeping clear margin under the sensor's 10 MHz SPI limit.
- Chip Select (CS) is controlled in software via GPIO, not hardware NSS.
- All of this is set in the STM32CubeMX project (`CSP4CMSIS_NUCLEO-G474RE_Shake_Detection_L3G4200D.ioc`), so CubeMX generates it.
- **The Data-Ready signal must be wired to the gyro's DRDY/INT2 pin, not its INT1 pin.** On the L3G4200D, `CTRL_REG3`'s `I2_DRDY` bit only routes Data-Ready to DRDY/INT2 — the INT1 pin is driven by a separate, unused programmable threshold interrupt generator. Wiring DRDY to INT1 instead will build and run without error but silently never produce an interrupt.

---

# 3. System Architecture

This project uses a **CSP (Communicating Sequential Processes)** architecture.

The processing chain is:
```text
Interrupt (DRDY/INT2) -> L3g4200d Process (Sensor Reader) -> ShakeDetect Process (Signal Processing) -> UI Process (printf output)
```

Each block runs as an independent CSP process.

---

# 4. Interrupt Handling

The gyro asserts DRDY/INT2 when new data is ready, wired to PB0 / EXTI0 (rising edge). EXTI0 is
enabled in the STM32CubeMX NVIC settings with "Uses FreeRTOS functions", priority 5 (numerically
>= `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` = 5, as required for an interrupt that calls into
CSP4CMSIS); CubeMX generates `EXTI0_IRQHandler()`, which calls the HAL, which calls

```cpp
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
```

The callback does NOT perform SPI communication. It only sends a trigger event into a CSP channel:

```cpp
static SamplingBufferedChannel<trigger_t, 1, BufferPolicy::KeepNewest> g_trigger_chan;
static IsrChanout<trigger_t> g_trigger_isr = g_trigger_chan.isrWriter();
...
g_trigger_isr.putFromISR(trigger_t{});
```

This keeps the interrupt short and safe.

**Why not a rendezvous channel?** A rendezvous needs both partners to be ready at the same time, and
an interrupt cannot wait. CSP4CMSIS therefore gives rendezvous channels no interrupt write path; an
interrupt writes into a buffered channel, through its ISR writer end (`isrWriter()`).

**Why capacity 1 and KeepNewest?** The trigger carries no data: it only says "a new sample is
waiting in the sensor", and the sensor keeps only its latest sample. If triggers arrive while the
L3g4200d process is still busy, they merge into one, and the next read gets the latest sample. A
queue of triggers would only read the same registers several times. The write never blocks and
never fails.

**Why this matters:** the data-ready line stays high until the sample is read, and the interrupt
fires on its rising edge. A trigger that got lost would leave the line high for ever: no new edge,
no new trigger, the whole pipeline stopped, without any message. With the buffered trigger channel
a trigger is never lost: if L3g4200d is busy, it finds the trigger waiting when it is ready again.

# 5. L3g4200d Process (Sensor Layer)

This process:
1. Waits for interrupt trigger
1. Reads X, Y, Z angular velocity using SPI
1. Sends the data into the next CSP channel
```cpp
trigger_reader >> t;
L3G4200D_ReadDPS(&gyro, &msg.x, &msg.y, &msg.z);
out << msg;
```
This ensures:
- No polling
- No blocking in ISR
- Clean separation of hardware and processing

Reading the output registers each cycle also clears the sensor's Data-Ready flag, which is what allows DRDY to go low and then re-trigger on the next sample — the interrupt is self-sustaining as long as this read happens every cycle.

# 6. Shake Detection Algorithm

Shake detection is based on short-term angular energy.

The algorithm works as follows:

## Step 1 – Compute Magnitude

We calculate total angular velocity:
```text
mag = sqrt(x² + y² + z²)
```
This gives total rotational motion.

## Step 2 – Remove Slow Drift

We maintain a running mean:
```text  
mean += alpha * (mag - mean)
hp = mag - mean
```
This acts like a high-pass filter.

Slow movement is removed.
Rapid movement remains.

## Step 3 – Energy Calculation

We accumulate squared high-pass values:
```text
energy += hp²
```
After a short time window (about 100 ms):
```text
avg_energy = energy / window_size
```
## Step 4 – Hysteresis Detection

Two thresholds are used:
- threshold_on
- threshold_off

If energy rises above threshold_on → SHAKE detected
If energy falls below threshold_off → Shake ended

This prevents rapid toggling.

# 7. UI Layer

The UI process prints:
```text
>>> SHAKE DETECTED! <<<
```
and
```text
Shake ended.
```
It only prints when the shake state changes.

# 8. Why CSP?

CSP provides:
- Clear process separation
- Deterministic communication
- No shared global data
- No FreeRTOS heap allocation (static network, static threads): see Memory below

This is good practice for real-time embedded systems.

## Memory

Measured on the board with the L3G4200D connected (Debug and Release):

- **FreeRTOS heap: not used.** `pvPortMalloc()` is never called (0 allocations). The three processes,
  `MainApp`, CubeMX's `defaultTask`, and FreeRTOS's idle and timer tasks all have static stacks and
  control blocks; the buffered trigger channel's semaphores are static too
  (`CSP4CMSIS_STATIC_ALLOCATION`), and the rendezvous channels need no RTOS objects. The FreeRTOS heap
  (`configTOTAL_HEAP_SIZE`) is therefore set to only 1 KB.
- **C library heap: 1 KB.** newlib's `printf()` allocates its `stdout` buffer with `malloc()` on first
  use (1032 B from `_sbrk()`). This is the only dynamic allocation.
- **Stacks used** (Debug, after shaking; Release at rest in brackets): `L3g4200d` 540 B (428 B),
  `ShakeDetect` 512 B (388 B), `UI` 484 B (212 B), each of 1 KB; `MainApp` 572 B (308 B) of 1.5 KB;
  `defaultTask` 152 B (104 B) of 1 KB.

# 9. Learning Outcomes
This project demonstrates:
- External interrupt handling
- SPI communication
- Interrupt-to-task synchronization
- Real-time signal processing
- State machine design
- CSP-based architecture
- Hysteresis thresholding
- Bringing up a sensor's interrupt line correctly: distinguishing a sensor's data-ready output from its programmable threshold interrupt, and confirming the MCU's ISR chain (NVIC → IRQHandler → HAL callback) is actually complete, not just enabled

# 10. How To Build

1. Clone this repository (not inside your STM32CubeIDE workspace directory).
1. In STM32CubeIDE: `File → Import → Existing Projects into Workspace`, select the cloned directory.
1. Build (configuration `Debug` or `Release`).
1. Flash to NUCLEO-G474RE
1. Open serial terminal (115200 baud), e.g. `minicom -D /dev/ttyACM0 -b 115200 -o`
1. Shake the sensor

The CSP4CMSIS settings are already in the project (G++ compiler, Debug and Release): include path
`../lib/csp4cmsis/inc`, and the defines `CSP4CMSIS_RTOS2_BACKEND_FREERTOS`,
`CSP4CMSIS_MAX_SYSCALL_INTERRUPT_PRIORITY=5`, `CSP4CMSIS_STATIC_ALLOCATION` and
`CSP4CMSIS_DEVICE_HEADER="stm32g4xx.h"` (explained in the
[CSP4CMSIS STM32CubeIDE guide](https://github.com/OliverFaust/CSP4CMSIS/blob/main/Documentation/CSP4CMSIS_STM32CubeIDE.md)).

The `.ioc` can be opened and regenerated (GENERATE CODE) without losing anything: SPI2, EXTI0 and
the FreeRTOS settings are stored in it, and the application's code in `main.c` and
`FreeRTOSConfig.h` sits between `USER CODE BEGIN`/`END` markers.

If the sensor is not connected or not answering, the console shows
`L3G4200D Fault: WHO_AM_I failed after 100 attempts.` and `HAL-ERROR during init`.

# 11. Example Output
```text
--- Launching CSP Static Network (Zero-Heap) ---
>>> SHAKE DETECTED! <<<
Shake ended.
>>> SHAKE DETECTED! <<<
```

# 12. Possible Extensions
- Multi-level shake intensity
- CSV streaming for plotting
- Fixed-point implementation
- LED indicator instead of printf
- Machine learning classification
- UART output mutex to prevent interleaved printf output across CSP processes

# License

MIT License – see the `LICENSE` file. CSP4CMSIS: MIT License, `lib/csp4cmsis/LICENSE`.

# Author
Dr Dr Oliver Faust
