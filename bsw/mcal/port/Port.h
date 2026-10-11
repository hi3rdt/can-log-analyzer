/**
 * @file    Port.h
 * @brief   Port driver (SWS_PortDriver subset) for STM32F103 GPIO.
 *
 * MCAL exercise 1 (docs/exercises/ex01-port-dio.vi.md). The contracts below
 * are what test/host/test_port_dio.c checks against fake registers.
 *
 * Pin numbering used by Port and Dio: id = port * 16 + pin, port A = 0,
 * B = 1, C = 2. Example: PC13 (Blue Pill LED) = 2 * 16 + 13 = 45.
 */
#ifndef PORT_H
#define PORT_H

#include "Std_Types.h"

typedef uint8 Port_PinType;

#define PORT_PIN(port, pin)         ((Port_PinType)(((port) * 16U) + (pin)))
#define PORT_A                      0U
#define PORT_B                      1U
#define PORT_C                      2U
#define PORT_PIN_COUNT              48U     /* 3 ports x 16 pins */

typedef enum
{
    PORT_PIN_IN  = 0,
    PORT_PIN_OUT = 1
} Port_PinDirectionType;

/** STM32F1 pin modes (CNF[1:0] + MODE[1:0] per pin, RM0008 9.2). */
typedef enum
{
    PORT_MODE_ANALOG = 0,
    PORT_MODE_INPUT_FLOATING,
    PORT_MODE_INPUT_PULLUP,
    PORT_MODE_INPUT_PULLDOWN,
    PORT_MODE_OUTPUT_PP,        /* general purpose push-pull  */
    PORT_MODE_OUTPUT_OD,        /* general purpose open-drain */
    PORT_MODE_ALT_PP,           /* alternate function push-pull (e.g. CAN TX) */
    PORT_MODE_ALT_OD
} Port_PinModeType;

/** Output speed = MODE[1:0] for output modes (ignored for inputs). */
typedef enum
{
    PORT_SPEED_10MHZ = 1,       /* MODE = 01 */
    PORT_SPEED_2MHZ  = 2,       /* MODE = 10 */
    PORT_SPEED_50MHZ = 3        /* MODE = 11 */
} Port_PinSpeedType;

typedef struct
{
    Port_PinType      pin;
    Port_PinModeType  mode;
    Port_PinSpeedType speed;
    uint8             initialLevel;         /* STD_HIGH / STD_LOW, outputs only */
    boolean           directionChangeable;  /* allows Port_SetPinDirection     */
} Port_PinConfigType;

typedef struct
{
    const Port_PinConfigType *pins;
    uint8                     pinCount;
} Port_ConfigType;

/* Service IDs and development error codes (SWS_Port). */
#define PORT_SID_INIT                   0x00U
#define PORT_SID_SET_PIN_DIRECTION      0x01U

#define PORT_E_PARAM_PIN                0x0AU
#define PORT_E_DIRECTION_UNCHANGEABLE   0x0BU
#define PORT_E_PARAM_CONFIG             0x0CU
#define PORT_E_UNINIT                   0x0FU
#define PORT_E_PARAM_POINTER            0x10U

/**
 * Return the 4-bit CNF|MODE value for one pin (RM0008 table 20):
 *   ANALOG 0x0, INPUT_FLOATING 0x4, INPUT_PULLUP/PULLDOWN 0x8,
 *   OUTPUT_PP 0x0|speed, OUTPUT_OD 0x4|speed, ALT_PP 0x8|speed, ALT_OD 0xC|speed.
 * Pure function, no register access.
 */
uint8 Port_Hw_ModeNibble(Port_PinModeType mode, Port_PinSpeedType speed);

/**
 * Configure every pin in ConfigPtr, in array order. For each pin:
 *  1. Enable its port clock: OR IOPxEN (bit 2 + port) into RCC APB2ENR,
 *     keeping all other APB2ENR bits.
 *  2. Set ODR bit (read-modify-write) BEFORE changing CRL/CRH:
 *     outputs (OUTPUT_* and ALT_*): 1 if initialLevel == STD_HIGH else 0;
 *     INPUT_PULLUP: 1; INPUT_PULLDOWN: 0; other modes: ODR untouched. Setting ODR first
 *     avoids a glitch on the pin when it becomes an output.
 *  3. Write Port_Hw_ModeNibble() into the pin's nibble of CRL (pins 0..7)
 *     or CRH (pins 8..15) by read-modify-write; other nibbles unchanged.
 * Errors (Det, module DET_MODULE_ID_PORT, SID PORT_SID_INIT), nothing written:
 *  - ConfigPtr == NULL_PTR or ConfigPtr->pins == NULL_PTR: PORT_E_PARAM_POINTER
 *  - any pin id >= PORT_PIN_COUNT: PORT_E_PARAM_CONFIG (checked for all pins
 *    before touching any register)
 */
void Port_Init(const Port_ConfigType *ConfigPtr);

/**
 * Change the direction of a pin whose config has directionChangeable = TRUE.
 * PORT_PIN_OUT -> OUTPUT_PP with the configured speed; PORT_PIN_IN ->
 * INPUT_FLOATING. Only the pin's CRL/CRH nibble changes.
 * Errors (SID PORT_SID_SET_PIN_DIRECTION): before Port_Init PORT_E_UNINIT;
 * Pin >= PORT_PIN_COUNT PORT_E_PARAM_PIN; pin not in the config or not
 * changeable PORT_E_DIRECTION_UNCHANGEABLE.
 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);

/** Test support: forget the stored configuration (back to "uninitialised"). */
void Port_DeInit(void);

#endif /* PORT_H */
