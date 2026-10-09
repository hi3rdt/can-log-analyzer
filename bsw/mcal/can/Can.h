/**
 * @file    Can.h
 * @brief   CAN driver interface (SWS_Can subset). The bxCAN implementation
 *          follows in a later milestone; host tests link a recording stub.
 */
#ifndef CAN_H
#define CAN_H

#include "Can_GeneralTypes.h"

/**
 * Request transmission of an L-PDU on hardware transmit handle Hth.
 * @return E_OK if a mailbox was filled, CAN_BUSY if none is free,
 *         E_NOT_OK on a development error.
 */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

#endif /* CAN_H */
