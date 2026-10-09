/**
 * @file    PduR_Com.h
 * @brief   PduR API towards Com (upper layer).
 */
#ifndef PDUR_COM_H
#define PDUR_COM_H

#include "ComStack_Types.h"

Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

#endif /* PDUR_COM_H */
