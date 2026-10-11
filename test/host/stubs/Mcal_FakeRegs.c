/**
 * @file    Mcal_FakeRegs.c
 * @brief   Host definitions of the MCAL base pointers, pointing at RAM.
 */
#include <string.h>

#include "Mcal_FakeRegs.h"

Mcal_GpioRegType Mcal_FakeGpio[MCAL_GPIO_PORT_COUNT];
Mcal_RccRegType  Mcal_FakeRcc;

Mcal_GpioRegType * const Mcal_GpioBase[MCAL_GPIO_PORT_COUNT] =
{
    &Mcal_FakeGpio[0], &Mcal_FakeGpio[1], &Mcal_FakeGpio[2]
};

Mcal_RccRegType * const Mcal_RccBase = &Mcal_FakeRcc;

void Mcal_FakeRegs_Reset(void)
{
    uint32 i;

    (void)memset(Mcal_FakeGpio, 0, sizeof(Mcal_FakeGpio));
    (void)memset(&Mcal_FakeRcc, 0, sizeof(Mcal_FakeRcc));
    for (i = 0U; i < MCAL_GPIO_PORT_COUNT; i++)
    {
        Mcal_FakeGpio[i].CRL = 0x44444444UL;   /* every pin: input floating */
        Mcal_FakeGpio[i].CRH = 0x44444444UL;
    }
}
