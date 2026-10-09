/**
 * @file    system_stm32f103.h
 * @brief   Clock tree and SysTick setup for the Blue Pill.
 */
#ifndef SYSTEM_STM32F103_H
#define SYSTEM_STM32F103_H

#include "Std_Types.h"

/** HSE 8 MHz -> PLL x9 -> SYSCLK 72 MHz, AHB 72 MHz, APB1 36 MHz, APB2 72 MHz. */
Std_ReturnType System_ClockInit(void);

/** Start SysTick with a 1 ms period; System_TickHook() is called from the ISR. */
void System_SysTickInit(void);

/** Milliseconds since System_SysTickInit(); wraps after ~49.7 days. */
uint32 System_GetTickMs(void);

#endif /* SYSTEM_STM32F103_H */
