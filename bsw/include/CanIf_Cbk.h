/**
 * @file    CanIf_Cbk.h
 * @brief   Upper-layer callbacks the Can driver calls (SWS_CANInterface names).
 *
 * In an AUTOSAR stack these are implemented by CanIf. This project has no
 * CanIf: the firmware test harness in app/ implements them (forward the frame
 * to the Uart SLCAN stream, count confirmations), and host tests implement
 * them as recording stubs. The Can driver only depends on this header.
 * Usually called from ISR context: keep implementations short.
 */
#ifndef CANIF_CBK_H
#define CANIF_CBK_H

#include "Can_GeneralTypes.h"

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);
void CanIf_TxConfirmation(PduIdType CanTxPduId);

#endif /* CANIF_CBK_H */
