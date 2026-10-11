/**
 * @file    Can.h
 * @brief   CAN driver interface (SWS_Can subset). Host tests link a recording
 *          stub (test/host/stubs/Can_Stub.c); the bxCAN implementation in
 *          Can.c is MCAL exercise 3 part B (docs/exercises/ex03-can-bxcan.vi.md).
 */
#ifndef CAN_H
#define CAN_H

#include "Can_GeneralTypes.h"
#include "Can_Hw.h"

/** Controller state transitions (SWS_Can Can_ControllerStateType subset). */
typedef enum
{
    CAN_CS_STARTED = 1,     /* leave init mode, take part in bus traffic */
    CAN_CS_STOPPED = 2,     /* init mode: no RX/TX, registers writable  */
    CAN_CS_SLEEP   = 3
} Can_ControllerStateType;

/** One 32-bit identifier/mask filter bank, assigned to RX FIFO 0. */
typedef struct
{
    Can_IdType id;
    Can_IdType mask;
} Can_FilterCfgType;

/** Post-build configuration of the single bxCAN controller. */
typedef struct
{
    uint32                   bitrate;             /* e.g. 500000            */
    uint16                   samplePointPermille; /* e.g. 875               */
    Can_Hw_TestModeType      testMode;            /* LOOPBACK on one board  */
    const Can_FilterCfgType *filters;             /* NULL_PTR = accept all  */
    uint8                    filterCount;         /* 0..14                  */
} Can_ConfigType;

/** Hardware object handles (unique across HTH and HRH). HTH 0 is the one
 *  gen/CanIf_Cfg.c already uses; it owns all three TX mailboxes. */
#define CAN_HTH_TX          ((Can_HwHandleType)0U)
#define CAN_HRH_FIFO0       ((Can_HwHandleType)1U)

/**
 * Initialise bxCAN: clocks, PB8/PB9 remap, bit timing, filters, interrupts.
 * Leaves the controller in CAN_CS_STOPPED.
 */
void Can_Init(const Can_ConfigType *Config);

/** Request a state transition. Returns E_NOT_OK on an invalid transition or timeout. */
Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_ControllerStateType Transition);

/**
 * Request transmission of an L-PDU on hardware transmit handle Hth.
 * @return E_OK if a mailbox was filled, CAN_BUSY if none is free,
 *         E_NOT_OK on a development error.
 */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

/** Poll for bus-off (used if the SCE interrupt is not enabled). */
void Can_MainFunction_BusOff(void);

#endif /* CAN_H */
