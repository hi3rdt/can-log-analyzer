/**
 * @file    Can_GeneralTypes.h
 * @brief   Types shared by Can and its upper layer (SWS_CANGeneralTypes subset).
 */
#ifndef CAN_GENERALTYPES_H
#define CAN_GENERALTYPES_H

#include "ComStack_Types.h"

/**
 * CAN identifier. Bit 31 set = extended (29-bit) frame, same convention as
 * DBC files, so generated IDs can be used unchanged. Bit 30 (CAN FD) unused.
 */
typedef uint32 Can_IdType;

#define CAN_ID_EXTENDED_FLAG    0x80000000UL
#define CAN_ID_STD_MASK         0x000007FFUL
#define CAN_ID_EXT_MASK         0x1FFFFFFFUL

/** Hardware object handle (HRH for reception, HTH for transmission). */
typedef uint16 Can_HwHandleType;

/** Return value of Can_Write when no TX mailbox is free. */
#define CAN_BUSY                ((Std_ReturnType)0x02U)

/** Received L-PDU location, passed from Can to CanIf_RxIndication. */
typedef struct
{
    Can_IdType       CanId;
    Can_HwHandleType Hoh;
    uint8            ControllerId;
} Can_HwType;

/** L-PDU handed to Can_Write by the upper layer. */
typedef struct
{
    PduIdType   swPduHandle;    /* caller's TX id, returned in CanIf_TxConfirmation */
    uint8       length;         /* DLC 0..8 */
    Can_IdType  id;
    uint8      *sdu;
} Can_PduType;

#endif /* CAN_GENERALTYPES_H */
