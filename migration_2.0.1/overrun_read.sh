#!/bin/bash
# overrun_read.sh <elf> <tag> [seconds]: flash + UART log, then read the overrun-test counters over SWD.
S=${OUT:-.}; E=$1; T=$2; SEC=${3:-8}; CLI=${STM32_PROGRAMMER_CLI:-STM32_Programmer_CLI}
python3 $NUCLEO_RUN $E $S/${T}_uart.txt $SEC > /dev/null
rd() { a=$(arm-none-eabi-nm $E | awk -v s=$1 '$3==s{print $1}'); v=$($CLI -c port=SWD mode=HOTPLUG -r32 0x$a 4 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -E '^0x' | awk '{print $3}'); printf '%d' 0x$v; }
echo "phase 1 (reader not waiting): interrupts=$(rd p1_isr) ISR-write failures=$(rd p1_fail) first wait=$(rd p1_first_wait_ms) ms | phase 2 burst: interrupts=$(rd d_isr) ISR-write failures=$(rd d_fail) triggers taken=$(rd d_reads) samples taken=$(rd d_samples) | totals after ${SEC} s: interrupts=$(rd t_isr) failures=$(rd t_fail) triggers=$(rd t_reads) samples=$(rd t_samples) done=$(rd t_done)"
