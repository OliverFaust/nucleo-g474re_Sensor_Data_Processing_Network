# Baseline before the CSP4CMSIS 2.0.1 update

**Date:** 2026-10-04. **Commit:** `f7bc5cf` (main, up to date with origin). **Board:** NUCLEO-G474RE,
ST-LINK-V3 VCP (LPUART1) 115200, **no L3G4200D connected** (WHO_AM_I reads 0x00). Data-ready
interrupts were injected over SWD (EXTI `SWIER1` bit 0: the real EXTI0 / HAL callback path).

## State

| Item | Finding |
|---|---|
| Library | `lib/csp4cmsis/`, FreeRTOS-native pre-1.0 snapshot, tree `14cf22a` (commit `184f7fc`, 2026-09-22): The_Process's snapshot `17c36e6` plus ALT fixes (`alt_channel_sync.cpp` +43/-9, `alternative.cpp` +1). Source folder `lib/csp4cmsis/src`; include path `${workspace_loc:/${ProjName}/lib/csp4cmsis/inc}`, Debug only; no CSP4CMSIS defines |
| CubeMX | CMSIS_V2; already FW_G4 V1.6.3, CubeMX 6.17.0; heap 15 360 B **hand-edited** in `FreeRTOSConfig.h` (`.ioc`: default 3072); newlib reentrancy off (CubeMX refuses to generate); timer task priority 2; `defaultTask` in the `.ioc` (256 words) but **deleted by hand** from `main.c` |
| SPI2 | `MX_SPI2_Init()` **hand-edited** (8-bit, mode 3, prescaler 32 = 5.3 MHz, NSS pulse off); the `.ioc` still has prescaler 2 (85 Mbit/s) and defaults, and CubeMX refuses to generate code from it ("IP not ready for code generation: SPI2"). With the hand-edited values put into the `.ioc`, CubeMX generates the same initialisation |
| EXTI0 (gyro data-ready, PB0, rising edge) | no NVIC entry in the `.ioc`; `EXTI0_IRQHandler()` **hand-written** in `stm32g4xx_it.c` (regeneration deletes it: the first interrupt then hangs in the default handler); NVIC enabled in `USER CODE SysInit`, priority 15 (valid: >= 5) |
| Bootstrap | `csp_app_main_init()` outside USER CODE (regeneration deletes it) |
| Build | Debug: 0 errors, **1 warning** (`READ_BIT` redefined in `l3g4200d.c`, clashes with the CMSIS macro); Release: 15 errors (no include path, no GNU++17) |
| Tracked build output | `Debug/` makefiles and the `.launch` file are tracked although `.gitignore` excludes them; they contain absolute paths of this machine |

## Network

| Process | Does | Channels |
|---|---|---|
| ISR (`HAL_GPIO_EXTI_Callback`, PB0) | `g_trigger_chan.writer().putFromISR(trigger_t{})` | `Channel<trigger_t>` rendezvous (Block), 1 B |
| L3g4200d | init (WHO_AM_I, 100 Hz, 250 dps), route data-ready, one read; then: wait trigger, SPI read X/Y/Z (dps), send | in: trigger; out: `Channel<Message>` rendezvous, `Message{float x,y,z}` 12 B |
| ShakeDetect | magnitude, running-mean high-pass, energy over 10 samples, hysteresis 3000/1500; sends only on state change | in: Message; out: `Channel<Result>` rendezvous, `Result{float}` 4 B |
| UI | prints `>>> SHAKE DETECTED! <<<` / `Shake ended.` | in: Result |

No ALT. Priorities: network 2 (all three), MainApp 3, timer task 2, no defaultTask.

## Board results (Debug)

| | |
|---|---|
| ELF SHA-256 | `ecdd0fae4bbf78ad88ed38132944747c0b474bf8be0d445c9357b72dad6ed571` |
| text / data / bss | 51 104 / 132 / 25 108 B |
| UART (20 s) | `--- Launching CSP Static Network (Zero-Heap) ---`, `L3G4200D Fault: WHO_AM_I failed after 100 attempts. Last value: 0x00`, `HAL-ERROR during init` (no sensor; the process continues and waits for triggers) |
| Stacks (1 KB each) | L3g4200d 572 B, ShakeDetect 276 B, UI 220 B |
| Priorities | 2 / 2 / 2 |
| FreeRTOS heap (15 360 B) | 5 allocations, 2 frees, 15 088 B free, 12 928 B minimum ever free |
| newlib `_sbrk` | 1032 B |

## Overrun test (instrumented scratch build, not committed: `overrun_patch.py`)

MainApp, 2 s after start: 10 software interrupts on EXTI line 0 back to back, while it still holds the
CPU (priority 3 > network 2). Counters: ISR calls, failed ISR writes, triggers taken by L3g4200d,
samples taken by ShakeDetect.

| Run | Interrupts | ISR-write failures | Triggers taken | Samples taken |
|---|---|---|---|---|
| 1, 2, 3 | 10 | **9** (ignored) | 1 | 1 |

Every data-ready interrupt that arrives while L3g4200d is not waiting is lost, silently.
