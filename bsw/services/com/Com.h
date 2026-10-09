/**
 * @file    Com.h
 * @brief   AUTOSAR COM (SWS_COM subset): signal <-> I-PDU packing.
 *
 * Signals are handled as raw values; scaling (factor/offset from the DBC)
 * belongs to the signal user, exactly like AUTOSAR COM.
 */
#ifndef COM_H
#define COM_H

#include "Com_Types.h"

#define COM_DEV_ERROR_DETECT        STD_ON

/* Return code in addition to E_OK / E_NOT_OK */
#define COM_SERVICE_NOT_AVAILABLE   ((uint8)0x80U)

#define COM_E_PARAM                 0x01U
#define COM_E_UNINIT                0x02U
#define COM_E_PARAM_POINTER         0x03U

#define COM_SID_INIT                0x01U
#define COM_SID_SENDSIGNAL          0x0AU
#define COM_SID_RECEIVESIGNAL       0x0BU
#define COM_SID_TRIGGERIPDUSEND     0x17U
#define COM_SID_TXCONFIRMATION      0x40U
#define COM_SID_RXINDICATION        0x42U

void           Com_Init(const Com_ConfigType *config);
uint8          Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr);
uint8          Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalDataPtr);
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId);

#endif /* COM_H */
