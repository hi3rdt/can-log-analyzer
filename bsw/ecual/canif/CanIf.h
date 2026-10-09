/**
 * @file    CanIf.h
 * @brief   CAN Interface (SWS_CANInterface subset): software filtering, DLC
 *          check and mapping between CAN IDs and PDU ids.
 */
#ifndef CANIF_H
#define CANIF_H

#include "CanIf_Types.h"

#define CANIF_DEV_ERROR_DETECT      STD_ON

/* Development error codes */
#define CANIF_E_PARAM_POINTER       20U
#define CANIF_E_UNINIT              30U
#define CANIF_E_INVALID_TXPDUID     50U
#define CANIF_E_INVALID_DLC         61U
#define CANIF_E_DATA_LENGTH_MISMATCH 62U

/* Service ids */
#define CANIF_SID_INIT              0x01U
#define CANIF_SID_TRANSMIT          0x49U
#define CANIF_SID_TXCONFIRMATION    0x13U
#define CANIF_SID_RXINDICATION      0x14U

void           CanIf_Init(const CanIf_ConfigType *ConfigPtr);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

#endif /* CANIF_H */
