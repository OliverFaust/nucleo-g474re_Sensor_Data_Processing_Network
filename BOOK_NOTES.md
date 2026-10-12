# Book notes

The source files deliberately contain no comments: the explanations are in the book, which refers to the code by line. This file keeps every comment removed from this chapter's code on 2026-10-12 (from commit `12ae539`), so that nothing is lost.

For each file, in file order: where the comment was (line numbers before the removal), the comment itself, and the code it explains, numbered as in the source file now, i.e. as in the book listing.

## `Core/Src/application.cpp`

1. **Line 4**, at the end of the code line; explains listing line 4:

   ```cpp
   // StaticTask_t
   ```

   ```cpp
   4  #include "FreeRTOS.h"
   ```

2. **Line 12**, above the code; explains listing line 12:

   ```cpp
   // PB0 (EXTI0, rising edge), wired to the gyro's DRDY/INT2 pin.
   ```

   ```cpp
   12  #define GYRO_DRDY_PIN GPIO_PIN_0
   ```

3. **Line 14**, above the code; explains listing line 13:

   ```cpp
   // SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI), initialised by main.c; chip select PB12.
   ```

   ```cpp
   13  extern SPI_HandleTypeDef hspi2;
   ```

4. **Lines 19–21**, above the code; explains listing line 17:

   ```cpp
   // Written by the data-ready interrupt: "a new sample is waiting". KeepNewest, capacity 1:
   // triggers that arrive while L3g4200d is busy merge into one, and none is lost, so the
   // data-ready line (low only after a read) cannot get stuck high.
   ```

   ```cpp
   17  static BufferedChannel<bool, 1, BufferPolicy::KeepNewest> g_trigger_chan;
   ```

5. **Line 58**, at the end of the code line; explains listing line 53:

   ```cpp
   // clears a data-ready already pending
   ```

   ```cpp
   53  L3G4200D_ReadDPS(&gyro, &msg.x, &msg.y, &msg.z);
   ```

6. **Line 69**, at the end of the code line; explains listing line 64:

   ```cpp
   // true = shake started, false = shake ended
   ```

   ```cpp
   64  Chanout<bool> out;
   ```

7. **Line 75**, at the end of the code line; explains listing line 70:

   ```cpp
   // mean filter speed
   ```

   ```cpp
   70  const float alpha = 0.02f;
   ```

8. **Line 76**, at the end of the code line; explains listing line 71:

   ```cpp
   // ~100 ms at 100 Hz
   ```

   ```cpp
   71  const int window_size = 10;
   ```

9. **Line 90**, at the end of the code line; explains listing line 85:

   ```cpp
   // high-pass: subtract the running mean
   ```

   ```cpp
   85  mean += alpha * (mag - mean);
   ```

10. **Line 97**, at the end of the code line; explains listing line 92:

   ```cpp
   // hysteresis
   ```

   ```cpp
   92  if (!shaking && avg_energy > threshold_on) {
   ```

11. **Lines 117–118**, above the code; explains listing lines 112–115 (the first lines of the block that follows):

   ```cpp
   // The only process that prints while the network runs (L3g4200d prints only before
   // its first sample).
   ```

   ```cpp
   112      void run() override {
   113          bool shaking;
   114          while (true) {
   115              in >> shaking;
   ```

12. **Lines 132–133**, above the code; explains listing lines 125–126:

   ```cpp
   // MainApp runs above the network, so the processes first run after MainApp has printed
   // its banner and exited.
   ```

   ```cpp
   125  static constexpr osPriority_t MAIN_APP_PRIORITY = osPriorityBelowNormal;
   126  static constexpr osPriority_t NETWORK_PRIORITY  = osPriorityLow;
   ```

13. **Line 137**, above the code; explains listing lines 128–129:

   ```cpp
   // Static stack (384 words = 1.5 KB) and control block: no heap.
   ```

   ```cpp
   128  alignas(8) static uint32_t mainAppStack[384];
   129  static StaticTask_t mainAppControlBlock;
   ```
