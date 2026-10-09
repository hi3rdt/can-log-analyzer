/**
 * @file    Can_Stub.h
 * @brief   Recording CAN driver stub for host tests.
 */
#ifndef CAN_STUB_H
#define CAN_STUB_H

#include "Can.h"

#define CAN_STUB_MAX_FRAMES  16U

typedef struct
{
    Can_HwHandleType hth;
    PduIdType        swPduHandle;
    Can_IdType       id;
    uint8            length;
    uint8            data[8];
} CanStub_FrameType;

void                     CanStub_Reset(void);
void                     CanStub_SetReturn(Std_ReturnType ret);
uint32                   CanStub_GetFrameCount(void);
const CanStub_FrameType *CanStub_GetFrame(uint32 index);

#endif /* CAN_STUB_H */
