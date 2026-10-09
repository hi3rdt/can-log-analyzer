/**
 * @file    ComStack_Types.h
 * @brief   Communication stack types (SWS_ComStackTypes subset).
 */
#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"

/** Handle of an I-PDU / L-PDU; index into the generated configuration tables. */
typedef uint16 PduIdType;

/** Length of a PDU in bytes (classic CAN: 0..8). */
typedef uint16 PduLengthType;

/** Basic information about a PDU of any type. */
typedef struct
{
    uint8         *SduDataPtr;  /* Payload */
    uint8         *MetaDataPtr; /* Optional meta data (e.g. CAN ID), may be NULL_PTR */
    PduLengthType  SduLength;   /* Payload length in bytes */
} PduInfoType;

#endif /* COMSTACK_TYPES_H */
