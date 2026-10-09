/**
 * @file    PduR_CanIf.h
 * @brief   PduR API towards CanIf (lower layer callbacks).
 */
#ifndef PDUR_CANIF_H
#define PDUR_CANIF_H

#include "ComStack_Types.h"

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#endif /* PDUR_CANIF_H */
