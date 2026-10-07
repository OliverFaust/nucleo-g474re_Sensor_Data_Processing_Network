# CSP4CMSIS 3.0.0 update: results

**Date:** 2026-10-06. **Board, tools:** as for 2.0.1 (`../migration_2.0.1/RESULTS.md`), L3G4200D
connected. **Library:** CSP4CMSIS v3.0.0 (commit `647a1cb`), unmodified. Procedure: `CHECKLIST.md`
section 9 of nucleo-g474re_The_Process. Scripts: `../migration_2.0.1/run.sh` (20 s run, SWD readings,
EXTI0 priority), `overrun_patch.py`, `overrun_read.sh` (unchanged; the process object layout is the same
in 3.0). Reference: the 2.0.1 follow-up with unbuffered stdout (`../migration_2.0.1/results/noheap_*`,
`hw_overrun_new_result.txt`), the state of main.

## Commits (branch csp4cmsis-3.0.0, from main `1038f26`)

| Commit | Change |
|---|---|
| lib/csp4cmsis | unmodified v3.0.0 sources, `LICENSE`, `VERSION` |
| Debug and Release defines | only `CSP4CMSIS_MAX_SYSCALL_INTERRUPT_PRIORITY=5` and `CSP4CMSIS_DEVICE_HEADER="stm32g4xx.h"` |
| `BufferedChannel<trigger_t, 1, BufferPolicy::KeepNewest>`, `SleepFor(Milliseconds(10))` | were `SamplingBufferedChannel<…>` (removed in 3.0) and `SleepFor(Milliseconds(10).to_ticks())` |
| README | 3.0.0, the two defines, `BufferedChannel` |

`Run(InParallel(...), ExecutionMode::StaticNetwork, NETWORK_PRIORITY)` already names its mode.

## Board results (sensor connected)

| | 2.0.1 Debug | 3.0.0 Debug | 2.0.1 Release | 3.0.0 Release |
|---|---|---|---|---|
| Build | 0 errors, 0 warnings | 0 errors, 0 warnings | 0 errors, 0 warnings | 0 errors, 0 warnings |
| text / data / bss (B) | 53 992 / 132 / 14 496 | 53 956 / 132 / 14 496 | 31 644 / 112 / 14 408 | 31 536 / 112 / 14 408 |
| UART at rest (20 s) | `--- Launching CSP Static Network (Zero-Heap) ---` | **identical** | same | **identical** |
| Stacks L3g4200d / ShakeDetect / UI / MainApp / defaultTask | 532 / 512 / 320 / 620 / 152 B | 532 / 512 / 320 / 620 / **144** B | 428 / 388 / 212 / 308 / 104 B | **436** / 388 / 212 / 308 / **100** B |
| Priorities processes / MainApp / defaultTask / EXTI0 | 8 / 16 / 24 / 5 | identical | 8 / 16 / 24 / 5 | identical |
| FreeRTOS heap / newlib `_sbrk` | 0 allocations / never called | identical | 0 allocations / never called | identical |

- **Stacks:** defaultTask −8 B (Debug), −4 B (Release); L3g4200d +8 B (Release); cause not examined,
  all well inside their 1 KB.
- **Static allocation without the define:** 0 FreeRTOS heap allocations; the trigger channel's semaphores
  are static (FreeRTOS detected from `FreeRTOS.h`).
- **Shake test (by hand):** not part of this run; it needs a person shaking the sensor (see below).

## Overrun test (instrumented scratch build, not committed; Debug; three runs)

| | Phase 1: write failures / first wait | Phase 2: interrupts / write failures / triggers taken | 8 s: interrupts / failures / triggers |
|---|---|---|---|
| 2.0.1, runs 1–3 | 0 / 0 ms | 10, 11, 10 / 0 / 1, 2, 1 | 933–935 / 0 / 933–934 |
| 3.0.0, runs 1–3 | 0 / 0 ms | 10, 11, 10 / 0 / 1, 2, 1 | 934–937 / 0 / 933–936 |

Identical behaviour: no ISR write fails, a trigger that arrives while L3g4200d is not waiting is kept, a
burst merges into one trigger; ≈ 935 data-ready interrupts in 8 s (100 Hz sensor, so the sensor is
connected and running). The scratch build has 1 warning, in the test instrumentation
(`-Wmisleading-indentation`), as with 2.0.1. `results/overrun_v300_result.txt`.

## Regeneration and fresh clone

- **GENERATE CODE** on the committed `.ioc`: no change in git; rebuilt ELFs byte-identical (Debug
  `8f3c79b8…`, Release `9e00f084…`, as on the board).
- **Fresh clone** to another path, empty workspace, import, build (0 errors, 0 warnings both): Release ELF
  byte-identical, Debug flash image identical; flashed: output identical (`results/fresh_debug_uart.txt`).
