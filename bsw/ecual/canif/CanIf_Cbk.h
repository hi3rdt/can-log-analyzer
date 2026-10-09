/**
 * @file    CanIf_Cbk.h
 * @brief   CanIf callbacks called by the CAN driver (usually from ISR context).
 */
#ifndef CANIF_CBK_H
#define CANIF_CBK_H

#include "Can_GeneralTypes.h"

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);
void CanIf_TxConfirmation(PduIdType CanTxPduId);

#endif /* CANIF_CBK_H */
