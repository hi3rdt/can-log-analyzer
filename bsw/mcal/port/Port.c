/**
 * @file    Port.c
 * @brief   Port driver for STM32F103 GPIO (MCAL exercise 1).
 *
 * TODO(hi3rdt): replace every stub body below with your implementation, then
 * build the host tests with -DMCAL_PORT_DIO_EXERCISE_TESTS=ON until
 * test_port_dio reports 0 failures. The contracts are in Port.h, the guide
 * is docs/exercises/ex01-port-dio.vi.md.
 *
 * Registers are reached only through Mcal_GpioBase[] and Mcal_RccBase
 * (Mcal_Reg.h), never through a hard-coded address, so the same file runs
 * on the target and against the fake registers of the host tests.
 */
#include "Port.h"
#include "Det.h"
#include "Mcal_Reg.h"

/* Configuration stored by Port_Init; NULL_PTR means "not initialised". */
static const Port_ConfigType *Port_ConfigPtr = NULL_PTR;

uint8 Port_Hw_ModeNibble(Port_PinModeType mode, Port_PinSpeedType speed)
{
    (void)mode;
    (void)speed;
    return 0U;          /* TODO(hi3rdt) */
}

void Port_Init(const Port_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;    /* TODO(hi3rdt): validate, then clock, ODR, CRL/CRH per pin */
    (void)Port_ConfigPtr;
}

void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction)
{
    (void)Pin;
    (void)Direction;    /* TODO(hi3rdt) */
}

void Port_DeInit(void)
{
    Port_ConfigPtr = NULL_PTR;
}
