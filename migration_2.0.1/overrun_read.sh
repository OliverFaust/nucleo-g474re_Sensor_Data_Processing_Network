#!/bin/bash
# overrun_read.sh <elf> <tag>: flash + 8 s UART log, then read the overrun-test counters over SWD
S=${OUT:-.}; E=$1; T=$2; CLI=${STM32_PROGRAMMER_CLI:-STM32_Programmer_CLI}
python3 $NUCLEO_RUN $E $S/${T}_uart.txt 8 > /dev/null
rd() { a=$(arm-none-eabi-nm $E | awk -v s=$1 '$3==s{print $1}'); v=$($CLI -c port=SWD mode=HOTPLUG -r32 0x$a 4 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -E '^0x' | awk '{print $3}'); printf '%d' 0x$v; }
echo "done=$(rd t_done) interrupts=$(rd t_isr) ISR-write failures=$(rd t_fail) triggers taken by L3g4200d=$(rd t_reads) samples taken by ShakeDetect=$(rd t_samples)"
