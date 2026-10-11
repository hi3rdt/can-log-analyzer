/**
 * @file    Dio.c
 * @brief   DIO driver for STM32F103 GPIO (MCAL exercise 1).
 *
 * TODO(hi3rdt): replace every stub body below with your implementation, then
 * build the host tests with -DMCAL_PORT_DIO_EXERCISE_TESTS=ON until
 * test_port_dio reports 0 failures. The contracts are in Dio.h.
 *
 * Rule for this file: outputs change only through BSRR (one atomic write),
 * never by read-modify-write on ODR.
 */
#include "Dio.h"
#include "Det.h"
#include "Mcal_Reg.h"

Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId)
{
    (void)ChannelId;
    return STD_LOW;     /* TODO(hi3rdt) */
}

void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level)
{
    (void)ChannelId;
    (void)Level;        /* TODO(hi3rdt) */
}

Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId)
{
    (void)ChannelId;
    return STD_LOW;     /* TODO(hi3rdt) */
}

Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId)
{
    (void)PortId;
    return 0U;          /* TODO(hi3rdt) */
}

void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level)
{
    (void)PortId;
    (void)Level;        /* TODO(hi3rdt) */
}
