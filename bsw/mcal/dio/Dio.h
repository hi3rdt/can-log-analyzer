/**
 * @file    Dio.h
 * @brief   DIO driver (SWS_DIODriver subset) for STM32F103 GPIO.
 *
 * MCAL exercise 1 (docs/exercises/ex01-port-dio.vi.md). Channel ids use the
 * same numbering as Port: id = port * 16 + pin (PC13 = 45).
 *
 * Dio never changes pin configuration (that is Port's job) and never uses
 * read-modify-write on ODR: writes go through BSRR so they are atomic and
 * safe to call from an ISR and from a task on the same port.
 */
#ifndef DIO_H
#define DIO_H

#include "Std_Types.h"

typedef uint8  Dio_ChannelType;
typedef uint8  Dio_PortType;
typedef uint8  Dio_LevelType;       /* STD_HIGH / STD_LOW */
typedef uint16 Dio_PortLevelType;   /* one bit per pin    */

#define DIO_CHANNEL(port, pin)      ((Dio_ChannelType)(((port) * 16U) + (pin)))
#define DIO_CHANNEL_COUNT           48U
#define DIO_PORT_COUNT              3U

/* Service IDs and development error codes (SWS_Dio). */
#define DIO_SID_READ_CHANNEL        0x00U
#define DIO_SID_WRITE_CHANNEL       0x01U
#define DIO_SID_READ_PORT           0x02U
#define DIO_SID_WRITE_PORT          0x03U
#define DIO_SID_FLIP_CHANNEL        0x11U

#define DIO_E_PARAM_INVALID_CHANNEL_ID  0x0AU
#define DIO_E_PARAM_INVALID_PORT_ID     0x14U

/**
 * Level of the channel from IDR. Invalid ChannelId: Det
 * DIO_E_PARAM_INVALID_CHANNEL_ID and return STD_LOW.
 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId);

/**
 * STD_HIGH: BSRR = 1 << pin. STD_LOW: BSRR = 1 << (pin + 16). One write,
 * ODR is never written directly. Any Level other than STD_HIGH counts as
 * STD_LOW. Invalid ChannelId: Det, no register access.
 */
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);

/**
 * Invert the OUTPUT latch (ODR, not IDR) through one BSRR write and return
 * the new level. Invalid ChannelId: Det, return STD_LOW, no write.
 */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId);

/** IDR bits 15..0 of the port. Invalid PortId: Det DIO_E_PARAM_INVALID_PORT_ID, return 0. */
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);

/**
 * Drive all 16 pins in one atomic write:
 * BSRR = Level | ((~Level & 0xFFFF) << 16). Invalid PortId: Det, no write.
 */
void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);

#endif /* DIO_H */
