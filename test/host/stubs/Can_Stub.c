/**
 * @file    Can_Stub.c
 * @brief   Records every Can_Write call so tests can check ID, DLC and payload.
 */
#include "Can_Stub.h"

static CanStub_FrameType CanStub_Frames[CAN_STUB_MAX_FRAMES];
static uint32            CanStub_Count;
static Std_ReturnType    CanStub_Return = E_OK;

void CanStub_Reset(void)
{
    CanStub_Count  = 0U;
    CanStub_Return = E_OK;
}

void CanStub_SetReturn(Std_ReturnType ret)
{
    CanStub_Return = ret;
}

uint32 CanStub_GetFrameCount(void)
{
    return CanStub_Count;
}

const CanStub_FrameType *CanStub_GetFrame(uint32 index)
{
    return (index < CanStub_Count) ? &CanStub_Frames[index] : NULL_PTR;
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    if ((CanStub_Return == E_OK) && (CanStub_Count < CAN_STUB_MAX_FRAMES))
    {
        CanStub_FrameType *f = &CanStub_Frames[CanStub_Count];
        uint8 i;

        f->hth         = Hth;
        f->swPduHandle = PduInfo->swPduHandle;
        f->id          = PduInfo->id;
        f->length      = PduInfo->length;
        for (i = 0U; i < 8U; i++)
        {
            f->data[i] = (i < PduInfo->length) ? PduInfo->sdu[i] : 0U;
        }
        CanStub_Count++;
    }
    return CanStub_Return;
}
