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
