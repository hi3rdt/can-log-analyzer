/**
 * @file    Mcal_FakeRegs.h
 * @brief   RAM-backed fake peripherals for MCAL host tests.
 *
 * A fake register is plain memory: writing BSRR does NOT update ODR, and IDR
 * only changes when the test writes it. Tests therefore check what the driver
 * wrote (e.g. BSRR) and preset what the driver reads (e.g. IDR).
 */
#ifndef MCAL_FAKEREGS_H
#define MCAL_FAKEREGS_H

#include "Mcal_Reg.h"

extern Mcal_GpioRegType Mcal_FakeGpio[MCAL_GPIO_PORT_COUNT];
extern Mcal_RccRegType  Mcal_FakeRcc;

/** Load reset values from RM0008 (CRL/CRH = 0x44444444, rest 0). */
void Mcal_FakeRegs_Reset(void);

#endif /* MCAL_FAKEREGS_H */
