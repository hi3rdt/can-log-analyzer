/**
 * @file    Mcal_Reg.h
 * @brief   Peripheral register layouts shared by the MCAL modules.
 *
 * MCAL modules never use fixed addresses directly. They go through the base
 * pointers declared here, which are defined twice:
 *   - firmware: platform/stm32f103/Mcal_RegMap.c (real addresses, RM0008)
 *   - host tests: test/host/stubs/Mcal_FakeRegs.c (plain RAM structs)
 * so the same Port.c / Dio.c runs unchanged on the PC against fake registers.
 */
#ifndef MCAL_REG_H
#define MCAL_REG_H

#include "Std_Types.h"

/** GPIO port registers, RM0008 section 9.5 (offsets 0x00..0x18). */
typedef struct
{
    volatile uint32 CRL;    /* 0x00 configuration, pins 0..7  (4 bits per pin) */
    volatile uint32 CRH;    /* 0x04 configuration, pins 8..15                  */
    volatile uint32 IDR;    /* 0x08 input data (read only)                     */
    volatile uint32 ODR;    /* 0x0C output data                                */
    volatile uint32 BSRR;   /* 0x10 bit set (15:0) / reset (31:16), write only */
    volatile uint32 BRR;    /* 0x14 bit reset (15:0), write only               */
    volatile uint32 LCKR;   /* 0x18 configuration lock                         */
} Mcal_GpioRegType;

/** RCC registers used by the MCAL, RM0008 section 7.3 (offsets 0x00..0x1C). */
typedef struct
{
    volatile uint32 CR;         /* 0x00 */
    volatile uint32 CFGR;       /* 0x04 */
    volatile uint32 CIR;        /* 0x08 */
    volatile uint32 APB2RSTR;   /* 0x0C */
    volatile uint32 APB1RSTR;   /* 0x10 */
    volatile uint32 AHBENR;     /* 0x14 */
    volatile uint32 APB2ENR;    /* 0x18 IOPAEN = bit 2, IOPBEN = 3, IOPCEN = 4 */
    volatile uint32 APB1ENR;    /* 0x1C */
} Mcal_RccRegType;

/** GPIO ports available on the STM32F103C8 (LQFP48): A, B, C. */
#define MCAL_GPIO_PORT_COUNT    3U

extern Mcal_GpioRegType * const Mcal_GpioBase[MCAL_GPIO_PORT_COUNT];
extern Mcal_RccRegType  * const Mcal_RccBase;

#endif /* MCAL_REG_H */
