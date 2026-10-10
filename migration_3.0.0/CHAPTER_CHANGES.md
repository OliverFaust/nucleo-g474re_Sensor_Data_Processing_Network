# What changes in the book chapter text (Sensor Data Processing Network), 2.0.1 -> 3.0.0

1. **`application.cpp` listing:**
   - `static BufferedChannel<trigger_t, 1, BufferPolicy::KeepNewest> g_trigger_chan;` replaces
     `static SamplingBufferedChannel<trigger_t, 1, BufferPolicy::KeepNewest> g_trigger_chan;`. In 3.0
     `BufferedChannel<T, N, P>` is the one buffered channel; `SamplingBufferedChannel` is gone. The ISR
     writer (`g_trigger_chan.isrWriter()`, `IsrChanout<trigger_t>`, `putFromISR()`) is unchanged.
   - `SleepFor(Milliseconds(10));` replaces `SleepFor(Milliseconds(10).to_ticks());` (L3g4200d's start).
   - `Run(InParallel(pL3g4200d, pShakeDetect, pUI), ExecutionMode::StaticNetwork, NETWORK_PRIORITY)`
     is unchanged (in 3.0 the mode is always given).
2. **Text that explains the trigger channel:** "a `SamplingBufferedChannel` with KeepNewest" becomes "a
   `BufferedChannel` with KeepNewest"; the explanation (an interrupt cannot wait, so it writes into a
   buffered channel; triggers merge, the sensor keeps only its latest sample) is unchanged.
3. **Project setup:** two defines in both configurations (`CSP4CMSIS_MAX_SYSCALL_INTERRUPT_PRIORITY=5`,
   `CSP4CMSIS_DEVICE_HEADER="stm32g4xx.h"`); static allocation is the default, FreeRTOS is detected.
4. **Library version:** CSP4CMSIS 3.0.0 (stable 3.x API), unmodified in `lib/csp4cmsis/`.
5. **Unchanged:** output, overrun behaviour, priorities (EXTI0 at 5), memory (no heap at all), the
   (Zero-Heap) banner; stacks within ±8 B.
6. **Simplified listing (branch `simplify-chapter-code`):** the code shows the chapter's concept and
   nothing else.
   - `struct trigger_t {}` is gone: `BufferedChannel<bool, 1, BufferPolicy::KeepNewest> g_trigger_chan;`
     (capacity and policy unchanged).
   - No `IsrChanout` variable: `HAL_GPIO_EXTI_Callback()` calls `g_trigger_chan.isrWriter().putFromISR(true);`.
   - `struct Result { float result; }` is gone: ShakeDetect sends `bool` on `result_chan` (`true` = shake
     started, `false` = shake ended), and UI tests it directly. `struct Message { float x, y, z; }` stays:
     it carries the gyro sample.
   - The three processes lose their `name()` overrides; indentation normalised.
   - **`main.c`:** CubeMX's `defaultTask` is removed (its attributes, its creation, `StartDefaultTask`).
     The program's threads are now exactly the ones in `application.cpp` (MainApp and the processes),
     plus FreeRTOS's idle and timer tasks. The `.ioc` still contains the task: CubeMX does not allow a
     project without one and re-creates it on regeneration (from the `.ioc`, as the static, heap-free
     task it was); the README says to delete it again.
   - **Thread names:** without the `name()` overrides, the processes appear as `csp_task` in a
     debugger's thread view; MainApp keeps its name (`attr.name`).
   - **Comments** shortened to what is surprising; the explanations are in the chapter text.
   - The console output is unchanged.
