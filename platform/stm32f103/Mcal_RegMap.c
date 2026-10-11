/**
 * @file    Mcal_RegMap.c
 * @brief   Real peripheral addresses for the MCAL base pointers (RM0008 3.3).
 *
 * MISRA C:2012 Rule 11.4 deviation: converting a fixed address to a pointer
 * is required for memory-mapped I/O and is confined to this file.
 */
#include "Mcal_Reg.h"

Mcal_GpioRegType * const Mcal_GpioBase[MCAL_GPIO_PORT_COUNT] =
{
    (Mcal_GpioRegType *)0x40010800UL,   /* GPIOA */
    (Mcal_GpioRegType *)0x40010C00UL,   /* GPIOB */
    (Mcal_GpioRegType *)0x40011000UL    /* GPIOC */
};

Mcal_RccRegType * const Mcal_RccBase = (Mcal_RccRegType *)0x40021000UL;
