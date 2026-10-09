/**
 * @file    PduR.h
 * @brief   PDU Router (SWS_PDURouter subset): Com <-> CanIf routing and
 *          CanIf -> CanIf PDU gateway (if-to-if, no buffering).
 */
#ifndef PDUR_H
#define PDUR_H

#include "PduR_Types.h"

#define PDUR_DEV_ERROR_DETECT       STD_ON

#define PDUR_E_UNINIT               0x01U
#define PDUR_E_PDU_ID_INVALID       0x02U
#define PDUR_E_PARAM_POINTER        0x05U

#define PDUR_SID_INIT               0xF0U
#define PDUR_SID_TRANSMIT           0x49U
#define PDUR_SID_RXINDICATION       0x42U
#define PDUR_SID_TXCONFIRMATION     0x40U

void PduR_Init(const PduR_PBConfigType *ConfigPtr);

#endif /* PDUR_H */
