# Test-only instrumentation for the overrun test (never committed).
# Usage: overrun_patch.py <application.cpp> old|new
# Counters: ISR calls, failed ISR writes, triggers taken by L3g4200d, samples taken by ShakeDetect.
# Phase 1 (p1_*, trigger while the reader is NOT waiting): right after Run() created the network,
#   before L3g4200d has run at all, N software interrupts on EXTI line 0. p1_first_wait_ms: how long
#   L3g4200d's first wait for a trigger took (0 = a trigger was already waiting).
# Phase 2 (d_*, overrun while the reader IS waiting): 2 s later, snapshot, N interrupts back to back
#   while MainApp still holds the CPU (it runs above the network), 1 ms for the network to react,
#   then the differences. With a sensor, real data-ready interrupts (100 Hz) also count in the
#   totals; the differences cover only the 1 ms window.
import sys
f, ver = sys.argv[1], sys.argv[2]; s = open(f).read()
def rep(a, b):
    global s
    assert s.count(a) == 1, (a, s.count(a)); s = s.replace(a, b)
rep('using namespace csp;', '''using namespace csp;
extern "C" { volatile uint32_t t_isr, t_fail, t_reads, t_samples, t_done, d_isr, d_fail, d_reads, d_samples,
                            p1_isr, p1_fail, p1_first_wait_ms = 0xFFFFFFFFu; }
static uint32_t t_first = 1;
static constexpr uint32_t OVERRUN_N = 10;''')
rep('            trigger_reader >> t;\n', '            uint32_t w0 = HAL_GetTick();\n            trigger_reader >> t;\n            if (t_first) { p1_first_wait_ms = HAL_GetTick() - w0; t_first = 0; }\n            t_reads++;\n')
rep('            in >> msg;\n', '            in >> msg;\n            t_samples++;\n')
def burst(delay2000, delay1):
    return ('for (uint32_t i = 0; i < OVERRUN_N; i++) EXTI->SWIER1 = 1u; p1_isr = t_isr; p1_fail = t_fail; '
            f'{delay2000} uint32_t i0 = t_isr, f0 = t_fail, r0 = t_reads, s0 = t_samples; '
            'for (uint32_t i = 0; i < OVERRUN_N; i++) EXTI->SWIER1 = 1u; '
            f'{delay1} d_isr = t_isr - i0; d_fail = t_fail - f0; d_reads = t_reads - r0; '
            'd_samples = t_samples - s0; t_done = 1;\n')
if ver == 'old':
    rep('g_trigger_chan.writer().putFromISR(trigger_t{});',
        't_isr++; if (!g_trigger_chan.writer().putFromISR(trigger_t{})) t_fail++;')
    rep('    vTaskDelete(NULL);', '    ' + burst('vTaskDelay(pdMS_TO_TICKS(2000));', 'vTaskDelay(1);') + '    vTaskDelete(NULL);')
else:
    rep('g_trigger_isr.putFromISR(trigger_t{});',
        't_isr++; if (!g_trigger_isr.putFromISR(trigger_t{})) t_fail++;')
    rep('    osThreadExit();\n}', '    ' + burst('osDelay(2000);', 'osDelay(1);') + '    osThreadExit();\n}')
open(f, 'w').write(s)
