# CSP4CMSIS Shake Detection
### NUCLEO-G474RE + L3G4200D Gyroscope

This project demonstrates interrupt-driven shake detection on the STM32 NUCLEO-G474RE using the L3G4200D gyroscope. It is a port of the original [NUCLEO-F401RE version](https://github.com/OliverFaust/CSP4CMSIS-shake-detection-NUCLEO-F401RE-) by Oliver Faust, adapted for the G4 series (different SPI instance, clock tree, and interrupt line).

It uses:

- STM32CubeIDE
- FreeRTOS (CMSIS-RTOS)
- CSP4CMSIS (Communicating Sequential Processes)
- Interrupt-driven SPI acquisition
- Real-time signal processing

The system detects when the sensor is shaken and prints a message to the serial terminal.

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

The gyro asserts DRDY/INT2 when new data is ready, wired to PB0 / EXTI0.

The interrupt handler:

```cpp
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
```

does NOT perform SPI communication.

It only sends a trigger event into a CSP channel:
This keeps the interrupt short and safe.

> **Note:** `HAL_GPIO_EXTI_Callback` is only ever reached if `EXTI0_IRQHandler()` exists in `stm32g4xx_it.c` and calls `HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0)`. STM32CubeMX does not always auto-generate an `EXTIx_IRQHandler` for every enabled line — double check it's present, since a missing handler falls through to the startup file's default (empty, infinite-loop) handler, which silently hangs the whole system on the very first interrupt with no fault raised.

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
- Zero heap (static network mode)

This is good practice for real-time embedded systems.

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

1. Open project in STM32CubeIDE
1. Build
1. Flash to NUCLEO-G474RE
1. Open serial terminal (115200 baud), e.g. `minicom -D /dev/ttyACM0 -b 115200 -o`
1. Shake the board

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

# Author
Dr Dr Oliver Faust
