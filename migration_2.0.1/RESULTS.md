# CSP4CMSIS 2.0.1 update: results

**Date:** 2026-10-04. **Board:** NUCLEO-G474RE (ST-LINK-V3, VCP = LPUART1, 115200) **with the L3G4200D
connected** (SPI2 PB12–PB15, DRDY/INT2 on PB0). **Tools:** STM32CubeIDE 2.1.0 (GNU Tools for STM32
14.3.1, headless build), STM32CubeMX 6.17.0, FW_G4 V1.6.3 (FreeRTOS 10.3.1), STM32CubeProgrammer (flash;
SWD reads and writes in hotplug mode, no reset). **Library:** CSP4CMSIS v2.0.1, unmodified.
**Baseline:** `BASELINE.md` (taken before the sensor was connected) and the with-sensor runs below.
**Decisions** (Phase A): trigger channel KeepNewest, capacity 1; no overrun counter (textbook: as
simple as possible); `EnableINT1` renamed `EnableDRDY`; verification on hardware with the sensor.
Scripts: `run.sh` (board run, SWD readings), `measure.py`, `overrun_patch.py` and `overrun_read.sh`
(overrun test).

## Commits (branch csp4cmsis-2.0.1, from main `f7bc5cf`, which was up to date with origin)

| Commit | Change |
|---|---|
| Baseline | `BASELINE.md`, logs, scripts |
| Make the project regeneration-safe | SPI2 (8-bit, mode 3, prescaler 32) and EXTI0 in the `.ioc` (CubeMX now generates `EXTI0_IRQHandler()` and the NVIC setup, priority 5); heap (15 360, unchanged) and newlib reentrancy in the `.ioc`; `csp_app_main_init()` into `USER CODE RTOS_THREADS`; CubeMX restores `defaultTask` |
| CubeMX: static defaultTask; configASSERT | as in The_Process |
| Gyro driver | `L3G4200D_EnableDRDY()`, `GYRO_DRDY_PIN` (data-ready is on DRDY/INT2); `READ_BIT` -> `SPI_READ` (clashed with the CMSIS macro: the one build warning) |
| CSP4CMSIS 2.0.1 | library + LICENSE + VERSION; include path, defines, GNU++17 in Debug and Release; trigger channel `SamplingBufferedChannel<trigger_t, 1, KeepNewest>` written via `isrWriter()`; CMSIS-RTOS2 bootstrap, `SleepFor()`, priorities; MainApp stack 1.5 KB, FreeRTOS heap 1 KB from the measurements |
| Untrack build output | `Debug/` makefiles and the `.launch` file (absolute paths) |
| Untrack language.settings.xml | as in The_Process |
| LICENSE | **new** (the repository had none): MIT, "Copyright (c) 2026 Oliver Faust", as the other chapters |
| README | interrupt design, versions, build steps, measured memory |
| Overrun test scripts | two phases |

No firmware migration: the project was already on FW_G4 V1.6.3 / CubeMX 6.17.0.

## Board results (sensor connected)

| | Old library (Debug) | 2.0.1 Debug | 2.0.1 Release |
|---|---|---|---|
| Build | Debug: 1 warning; Release: 15 errors | 0 errors, 0 warnings | 0 errors, 0 warnings |
| text / data / bss (B) | 51 104 / 132 / 25 108 | 53 616 / 132 / 14 496 | 31 252 / 112 / 14 408 |
| UART at rest (20 s) | `--- Launching CSP Static Network (Zero-Heap) ---` | **identical** | **identical** |
| UART while shaking (120 s, by hand) | 27 × (`>>> SHAKE DETECTED! <<<`, `Shake ended.`), alternating | 16 × the same pair, alternating | — |
| Data-ready rate | ≈ 935 interrupts in the 8 s window (100 Hz) | same | — |
| Stacks at rest: L3g4200d / ShakeDetect / UI (1 KB each) | 432 / 408 / 220 B | 540 / 512 / 320 B (UI 484 B after shaking) | 428 / 388 / 212 B |
| MainApp stack | 2 KB from the heap (not measured) | 572 B of 1.5 KB (static) | 308 B of 1.5 KB |
| defaultTask stack (1 KB) | none (deleted by hand) | 144 B (152 B after shaking) | 104 B |
| Priorities processes / MainApp / defaultTask / EXTI0 | 2 / 3 / — / 15 | 8 / 16 / 24 / 5 | 8 / 16 / 24 / 5 |
| FreeRTOS heap | 5 allocations, 2 frees | **0 allocations** (heap 1 KB) | **0 allocations** |
| newlib `_sbrk` | 1032 B | 1032 B | 1032 B |

The shake counts differ because the shaking (by hand) differs between runs; both versions detect
shakes and alternate correctly between the two messages. The stack figures without the sensor
(`BASELINE.md`) differ because the init then takes the WHO_AM_I error path.

## Overrun test (instrumented scratch builds, not committed: `overrun_patch.py`; sensor connected)

Phase 1: 10 software interrupts right after `Run()` created the network, before L3g4200d waits
(the lost-completion case). Phase 2: 2 s later, 10 back to back while L3g4200d waits but cannot run
yet (MainApp holds the CPU), counted over 1 ms. Real data-ready interrupts (100 Hz) arrive as well.

| | Phase 1: write failures / first wait | Phase 2: interrupts / write failures / triggers taken | Failures in 8 s |
|---|---|---|---|
| Old library, runs 1–3 | **10** / 4, 8, 4 ms (until the next real data-ready edge) | 10, 10, 11 / **9** / 1, 1, 2 | 19 (only the injected ones) |
| 2.0.1, runs 1–3 | **0** / 0 ms (a trigger was waiting) | 10, 11, 10 / **0** / 1, 2, 1 | 0 |

(An 11th interrupt and a second trigger in phase 2 is a real data-ready interrupt inside the 1 ms
window. The 8-s totals of triggers and samples differ by a few because SWD reads them one after the
other while they keep counting.)

- **Old library:** every interrupt that arrives while L3g4200d is not waiting is lost, silently. In
  phase 1 the pipeline does not stall only because L3g4200d's read before its loop re-arms the
  sensor; without the sensor (no further edges) the first wait lasted until the next injected
  interrupt, 1776 ms later (`results/nosensor_old_uart.txt`, pre-check).
- **2.0.1:** no interrupt write fails; a trigger that arrives while L3g4200d is not waiting is kept;
  a burst merges into **one** trigger.
- **Correction of the Phase A prediction:** I predicted 2 triggers for phase 2 on 2.0.1 ("the first
  to the waiting reader, the rest merged"). Measured: 1. A buffered channel also buffers the first
  trigger instead of handing it to the waiting reader, so all 10 merge into one. Phase 1 was added
  because it is the case that separates the two versions.

## Regeneration and fresh clone

- **GENERATE CODE** (CubeMX 6.17.0) on the committed `.ioc`: no change in git; rebuilt Debug and
  Release ELFs byte-identical to those flashed above, so the board output is identical.
- **Fresh clone** to another path (without the formerly tracked `Debug/` makefiles), empty
  workspace, import, build (Debug and Release, 0 errors, 0 warnings): Release ELF byte-identical;
  Debug flash image identical (the ELF's debug information contains the build path);
  `language.settings.xml` was recreated by the import (ignored by git).

## What changes in the book chapter text (Sensor Data Processing Network)

**The interrupt and the trigger channel** (the core of the chapter):

1. `static Channel<trigger_t> g_trigger_chan;` becomes
   `static SamplingBufferedChannel<trigger_t, 1, BufferPolicy::KeepNewest> g_trigger_chan;` plus
   `static IsrChanout<trigger_t> g_trigger_isr = g_trigger_chan.isrWriter();`; the callback calls
   `g_trigger_isr.putFromISR(trigger_t{})`. The reader side (`trigger_reader >> t`) is unchanged.
2. **Why an interrupt cannot use a rendezvous channel:** a rendezvous needs both partners at once, an
   interrupt cannot wait; 2.0.1 has no ISR write path on rendezvous channels.
3. **Why KeepNewest, capacity 1, is right for this data:** the trigger carries no data; it means "a
   new sample is waiting in the sensor", and the sensor keeps only its latest sample. Merged triggers
   lose nothing a queue would recover; queued triggers would read the same registers again and feed
   duplicate samples into the filter (running mean, energy window). Measured: a burst of 10 becomes
   one read.
4. **The lost trigger (new section or box):** data-ready stays high until the sample is read, and the
   interrupt fires on the rising edge. Under the old library a trigger that arrived while L3g4200d was
   not waiting was dropped; then no new edge could come and the pipeline would stop for good,
   silently. Measured: 1.x lost 10 of 10 triggers that arrived before L3g4200d waited, 2.0.1 none.
   (In this example the window is small: the read before the loop and the fast loop make it rare.)
5. **No `portYIELD_FROM_ISR`** and no comment about it: the library's wakeup yields.
6. **Interrupt configuration:** EXTI0 is enabled in STM32CubeMX's NVIC table ("Uses FreeRTOS
   functions"), CubeMX generates `EXTI0_IRQHandler()` and sets priority 5 (= the threshold
   `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`); no hand-written handler or NVIC code. The old
   README note about a missing `EXTI0_IRQHandler` is obsolete.
7. **Sensor naming:** data-ready is routed to the DRDY/INT2 pin (`CTRL_REG3` bit 3, `I2_DRDY`):
   `L3G4200D_EnableDRDY()`, `GYRO_DRDY_PIN` (not INT1).
8. **SPI2:** configured in the `.ioc` (8-bit, mode 3, prescaler 32 = 5.3 MHz), not by editing
   `MX_SPI2_Init()`.

**Unchanged:** the rendezvous channels `msg_chan` (`Message`, 12 B) and `result_chan` (`Result`, 4 B)
(trivially copyable, Block, task-to-task), the three processes' logic, the shake-detection
algorithm, the console messages.

**As in the other chapters:**

9. `application.cpp`: `osThreadNew` with a static 1.5 KB stack (measured 572 B), `osDelay`,
   `osThreadExit()`, `SleepFor(Milliseconds(10).to_ticks())` in L3g4200d, includes `cmsis_os2.h` and
   `FreeRTOS.h`; named priorities `osPriorityLow` (processes, 8) < `osPriorityBelowNormal` (MainApp,
   16) < `osPriorityNormal` (defaultTask, 24), formerly 2 < 3 (no defaultTask); `Run(..., priority)`.
10. Stack units: `CSProcessStatic<256>` = 256 words = 1 KB; `osThreadAttr_t.stack_size` in bytes.
11. CubeMX settings: `USE_NEWLIB_REENTRANT` Enabled; heap 1024 B in the `.ioc`; `defaultTask` static
    (CubeMX re-creates it); printing `configASSERT`; FW_G4 V1.6.3, CubeMX 6.17.0, CubeIDE 2.1.0;
    `csp_app_main_init()` in `USER CODE RTOS_THREADS`.
12. Project setup: `lib/csp4cmsis/` = CSP4CMSIS 2.0.1 unmodified (`VERSION`, `LICENSE`); include path
    `../lib/csp4cmsis/inc`; four defines; GNU++17 in Debug and Release; no build output in the
    repository.
13. Memory: no FreeRTOS heap allocation (measured; heap 1 KB), newlib's `printf` takes 1 KB. The
    console banner still says "(Zero-Heap)"; decide whether to keep it.
14. "Interrupt-driven SPI acquisition" -> interrupt-triggered (the SPI transfer is polled).

Logs: `results/` (`hw_*`, `shake_*`, `hw_overrun_*`, `baseline_sensor_*`: with the sensor;
`baseline_*`, `overrun_old_*`, `nosensor_*`: before it was connected).
