#!/bin/bash
# run.sh <elf> <tag> [seconds]: board run (UART log) and SWD readings (stacks, TCBs, heap, sbrk, EXTI0
# priority) for this example. Logs go to $OUT (default: .). Needs NUCLEO_RUN and STM32_PROGRAMMER_CLI.
S=${OUT:-.}; M=$(dirname $0); E=$1; T=$2; SEC=${3:-20}; CLI=${STM32_PROGRAMMER_CLI:-STM32_Programmer_CLI}
args=(--stack 'L3g4200d=MainApp_Task(void*)::pL3g4200d+0x10:1024' --stack 'ShakeDetect=MainApp_Task(void*)::pShakeDetect+0x10:1024'
      --stack 'UI=MainApp_Task(void*)::pUI+0x10:1024'
      --tcb 'L3g4200d=MainApp_Task(void*)::pL3g4200d+0x410' --tcb 'ShakeDetect=MainApp_Task(void*)::pShakeDetect+0x410' --tcb 'UI=MainApp_Task(void*)::pUI+0x410')
arm-none-eabi-nm -C $E | grep -q ' mainAppStack$' && args+=(--stack MainApp=mainAppStack --stack defaultTask=defaultTaskBuffer --tcb MainApp=mainAppControlBlock --tcb defaultTask=defaultTaskControlBlock)
python3 $M/measure.py $E $S/${T}_uart.txt $SEC "${args[@]}" > $S/${T}_measure.txt
v=$($CLI -c port=SWD mode=HOTPLUG -r32 0xE000E404 4 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -E '^0x' | awk '{print $3}')
echo "NVIC EXTI0 priority $(( (0x$v >> 16 & 0xff) >> 4 ))" >> $S/${T}_measure.txt
cat $S/${T}_measure.txt
