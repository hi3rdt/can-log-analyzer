/**
 * @file    Can_Hw.c
 * @brief   Pure bxCAN helpers (exercise 1, see docs/exercises/ex01-bxcan-driver.vi.md).
 *
 * TODO(hi3rdt): replace every stub body below with your implementation, then
 * build the host tests with -DCAN_HW_EXERCISE_TESTS=ON until test_can_hw
 * reports 0 failures. The contracts are in Can_Hw.h.
 */
#include "Can_Hw.h"

Std_ReturnType Can_Hw_CalcBitTiming(uint32 pclkHz,
                                    uint32 bitrate,
                                    uint16 samplePointPermille,
                                    Can_Hw_BitTimingType *timing)
{
    (void)pclkHz;
    (void)bitrate;
    (void)samplePointPermille;
    (void)timing;
    return E_NOT_OK;    /* TODO(hi3rdt) */
}

uint32 Can_Hw_BuildBtr(const Can_Hw_BitTimingType *timing, Can_Hw_TestModeType mode)
{
    (void)timing;
    (void)mode;
    return 0UL;         /* TODO(hi3rdt) */
}

uint32 Can_Hw_EncodeId(Can_IdType id)
{
    (void)id;
    return 0UL;         /* TODO(hi3rdt) */
}

Can_IdType Can_Hw_DecodeId(uint32 rir)
{
    (void)rir;
    return 0UL;         /* TODO(hi3rdt) */
}

void Can_Hw_PackData(const uint8 *sdu, uint8 length, uint32 *tdlr, uint32 *tdhr)
{
    (void)sdu;
    (void)length;
    (void)tdlr;
    (void)tdhr;         /* TODO(hi3rdt) */
}

void Can_Hw_UnpackData(uint32 rdlr, uint32 rdhr, uint8 length, uint8 *sdu)
{
    (void)rdlr;
    (void)rdhr;
    (void)length;
    (void)sdu;          /* TODO(hi3rdt) */
}

uint8 Can_Hw_FindFreeMailbox(uint32 tsr)
{
    (void)tsr;
    return CAN_HW_NO_MAILBOX;   /* TODO(hi3rdt) */
}

void Can_Hw_BuildMaskFilter32(Can_IdType id, Can_IdType mask, uint32 *fr1, uint32 *fr2)
{
    (void)id;
    (void)mask;
    (void)fr1;
    (void)fr2;          /* TODO(hi3rdt) */
}
