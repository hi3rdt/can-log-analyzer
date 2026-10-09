/**
 * @file    Com_Cbk.h
 * @brief   Com callbacks called by PduR.
 */
#ifndef COM_CBK_H
#define COM_CBK_H

#include "ComStack_Types.h"

void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#endif /* COM_CBK_H */
