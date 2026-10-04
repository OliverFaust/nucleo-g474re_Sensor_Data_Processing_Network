# Test-only instrumentation for the overrun test (never committed).
# Usage: overrun_patch.py <application.cpp> old|new
# Counts: ISR calls, failed ISR writes, triggers taken by L3g4200d, samples taken by ShakeDetect.
# MainApp, 2 s after start (sensor init done): N software interrupts on EXTI line 0 back to back,
# while it still holds the CPU (it runs above the network).
import sys
f, ver = sys.argv[1], sys.argv[2]; s = open(f).read()
def rep(a, b, count=1):
    global s
    assert s.count(a) == count, (a, s.count(a)); s = s.replace(a, b)
rep('using namespace csp;', '''using namespace csp;
extern "C" { volatile uint32_t t_isr, t_fail, t_reads, t_samples, t_done; }
static constexpr uint32_t OVERRUN_N = 10;''')
if ver == 'old':
    rep('g_trigger_chan.writer().putFromISR(trigger_t{});',
        't_isr++; if (!g_trigger_chan.writer().putFromISR(trigger_t{})) t_fail++;')
    rep('            trigger_reader >> t;\n', '            trigger_reader >> t;\n            t_reads++;\n')
    burst = ('vTaskDelay(pdMS_TO_TICKS(2000)); for (uint32_t i = 0; i < OVERRUN_N; i++) EXTI->SWIER1 = 1u; '
             't_done = 1;\n    vTaskDelete(NULL);')
    rep('    vTaskDelete(NULL);', '    ' + burst)
else:
    rep('@@ISR_WRITE@@', '@@ISR_WRITE@@')  # placeholder, set when the 2.0.1 code exists
rep('            in >> msg;\n', '            in >> msg;\n            t_samples++;\n')
open(f, 'w').write(s)
