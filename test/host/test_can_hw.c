/**
 * @file    test_can_hw.c
 * @brief   Host tests for the pure bxCAN helpers in Can_Hw.c (MCAL exercise 3).
 *
 * Expected register values were derived by hand from RM0008 section 24.9 and
 * cross-checked against the bit timing examples in docs/roadmap.md. Enable
 * with: cmake -S . -B build-host -DCAN_HW_EXERCISE_TESTS=ON
 */
#include <stdlib.h>
#include <string.h>

#include "Can_Hw.h"
#include "unit_test.h"

#define PCLK1_36MHZ     36000000UL

/* ------------------------------------------------------- Bit timing --- */

static void check_timing(uint32 pclk, uint32 bitrate, uint16 sp,
                         uint16 presc, uint8 bs1, uint8 bs2)
{
    Can_Hw_BitTimingType t = { 0U, 0U, 0U, 0U };

    UT_CHECK_EQ(E_OK, Can_Hw_CalcBitTiming(pclk, bitrate, sp, &t));
    UT_CHECK_EQ(presc, t.prescaler);
    UT_CHECK_EQ(bs1, t.bs1);
    UT_CHECK_EQ(bs2, t.bs2);
    UT_CHECK_EQ(1U, t.sjw);
}

static void test_bit_timing_automotive_rates(void)
{
    /* 18 TQ, sample point 16/18 = 88.9 % (roadmap phase 3 values) */
    check_timing(PCLK1_36MHZ,  500000UL, 875U,  4U, 15U, 2U);
    check_timing(PCLK1_36MHZ,  250000UL, 875U,  8U, 15U, 2U);
    check_timing(PCLK1_36MHZ,  125000UL, 875U, 16U, 15U, 2U);
    check_timing(PCLK1_36MHZ, 1000000UL, 875U,  2U, 15U, 2U);
}

static void test_bit_timing_sample_point_rounding(void)
{
    /* 1 Mbit/s @ 75 %: samplePos = (18 * 750 + 500) / 1000 = 14 */
    check_timing(PCLK1_36MHZ, 1000000UL, 750U, 2U, 13U, 4U);
    /* 95 %: 24 TQ gives bs1 = 22 (> 16), so 18 TQ with bs2 = 1 wins */
    check_timing(PCLK1_36MHZ, 500000UL, 950U, 4U, 16U, 1U);
    /* 50 %: 24 TQ and 18 TQ need bs2 > 8, first fit is 12 TQ */
    check_timing(PCLK1_36MHZ, 500000UL, 500U, 6U, 5U, 6U);
    /* HSI fallback (8 MHz): only 16 TQ divides, prescaler 1 */
    check_timing(8000000UL, 500000UL, 875U, 1U, 13U, 2U);
}

static void test_bit_timing_rejects_invalid_input(void)
{
    Can_Hw_BitTimingType t = { 0xBEEFU, 0xAAU, 0xBBU, 0xCCU };

    UT_CHECK_EQ(E_NOT_OK, Can_Hw_CalcBitTiming(PCLK1_36MHZ, 500000UL, 875U, NULL_PTR));
    UT_CHECK_EQ(E_NOT_OK, Can_Hw_CalcBitTiming(PCLK1_36MHZ, 0UL, 875U, &t));
    UT_CHECK_EQ(E_NOT_OK, Can_Hw_CalcBitTiming(PCLK1_36MHZ, 500000UL, 499U, &t));
    UT_CHECK_EQ(E_NOT_OK, Can_Hw_CalcBitTiming(PCLK1_36MHZ, 500000UL, 951U, &t));
    /* No TQ count in 8..25 divides 36 MHz exactly */
    UT_CHECK_EQ(E_NOT_OK, Can_Hw_CalcBitTiming(PCLK1_36MHZ, 33333UL, 875U, &t));
    /* Bitrate above the peripheral clock: prescaler would be 0 */
    UT_CHECK_EQ(E_NOT_OK, Can_Hw_CalcBitTiming(PCLK1_36MHZ, 40000000UL, 875U, &t));
    /* 2147983648 * 18 wraps to 9 000 000 in 32-bit arithmetic, which divides
     * 36 MHz. A uint32 multiply would wrongly accept this bitrate. */
    UT_CHECK_EQ(E_NOT_OK, Can_Hw_CalcBitTiming(PCLK1_36MHZ, 2147983648UL, 875U, &t));

    /* Output must stay untouched on every failure */
    UT_CHECK_EQ(0xBEEFU, t.prescaler);
    UT_CHECK_EQ(0xAAU, t.bs1);
    UT_CHECK_EQ(0xBBU, t.bs2);
    UT_CHECK_EQ(0xCCU, t.sjw);
}

static void test_build_btr(void)
{
    const Can_Hw_BitTimingType t500k = { 4U, 15U, 2U, 1U };
    const Can_Hw_BitTimingType tMax  = { 1024U, 16U, 8U, 4U };

    UT_CHECK_EQ(0x001E0003UL, Can_Hw_BuildBtr(&t500k, CAN_HW_MODE_NORMAL));
    UT_CHECK_EQ(0x401E0003UL, Can_Hw_BuildBtr(&t500k, CAN_HW_MODE_LOOPBACK));
    UT_CHECK_EQ(0x801E0003UL, Can_Hw_BuildBtr(&t500k, CAN_HW_MODE_SILENT));
    UT_CHECK_EQ(0xC01E0003UL, Can_Hw_BuildBtr(&t500k, CAN_HW_MODE_SILENT_LOOPBACK));
    /* Every field at its maximum: no field may spill into its neighbour */
    UT_CHECK_EQ(0x037F03FFUL, Can_Hw_BuildBtr(&tMax, CAN_HW_MODE_NORMAL));
}

/* ------------------------------------------------------ Identifiers --- */

static void test_encode_standard_id(void)
{
    UT_CHECK_EQ(0x24600000UL, Can_Hw_EncodeId(0x123UL));
    UT_CHECK_EQ(0xFFE00000UL, Can_Hw_EncodeId(0x7FFUL));
    UT_CHECK_EQ(0x00000000UL, Can_Hw_EncodeId(0x000UL));
    /* Bits above the 11-bit range are masked, not shifted into IDE/RTR */
    UT_CHECK_EQ(0x24600000UL, Can_Hw_EncodeId(0x0000F123UL));
}

static void test_encode_extended_id(void)
{
    UT_CHECK_EQ(0xC6D78884UL, Can_Hw_EncodeId(CAN_ID_EXTENDED_FLAG | 0x18DAF110UL));
    UT_CHECK_EQ(0xFFFFFFFCUL, Can_Hw_EncodeId(CAN_ID_EXTENDED_FLAG | 0x1FFFFFFFUL));
    UT_CHECK_EQ(0x00000004UL, Can_Hw_EncodeId(CAN_ID_EXTENDED_FLAG));
    /* Bit 30 (CAN FD flag in this project) and bit 29 must not leak in */
    UT_CHECK_EQ(0x0000000CUL, Can_Hw_EncodeId(0xE0000001UL));
}

static void test_decode_id(void)
{
    UT_CHECK_EQ(0x123UL, Can_Hw_DecodeId(0x24600000UL));
    /* RTR and bit 0 are ignored */
    UT_CHECK_EQ(0x123UL, Can_Hw_DecodeId(0x24600003UL));
    UT_CHECK_EQ(CAN_ID_EXTENDED_FLAG | 0x18DAF110UL, Can_Hw_DecodeId(0xC6D78886UL));
    UT_CHECK_EQ(CAN_ID_EXTENDED_FLAG | 0x1FFFFFFFUL, Can_Hw_DecodeId(0xFFFFFFFEUL));
}

static void test_id_round_trip(void)
{
    uint32 id;

    for (id = 0UL; id <= CAN_ID_STD_MASK; id++)
    {
        UT_CHECK_EQ(id, Can_Hw_DecodeId(Can_Hw_EncodeId(id)));
    }
    for (id = 1UL; id <= CAN_ID_EXT_MASK; id = (id << 1) | 1UL)
    {
        UT_CHECK_EQ(CAN_ID_EXTENDED_FLAG | id,
                    Can_Hw_DecodeId(Can_Hw_EncodeId(CAN_ID_EXTENDED_FLAG | id)));
    }
}

/* ----------------------------------------------------------- Payload --- */

static void test_pack_full_frame(void)
{
    const uint8 sdu[8] = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U };
    uint32 lo = 0xDEADBEEFUL;
    uint32 hi = 0xDEADBEEFUL;

    Can_Hw_PackData(sdu, 8U, &lo, &hi);
    UT_CHECK_EQ(0x44332211UL, lo);
    UT_CHECK_EQ(0x88776655UL, hi);
}

static void test_pack_short_frame_reads_only_length_bytes(void)
{
    /* Exactly 3 heap bytes: ASan aborts on any read of sdu[3..7] */
    uint8 *sdu = (uint8 *)malloc(3U);
    uint32 lo = 0xDEADBEEFUL;
    uint32 hi = 0xDEADBEEFUL;

    UT_CHECK(sdu != NULL);
    if (sdu != NULL)
    {
        sdu[0] = 0xA1U;
        sdu[1] = 0xB2U;
        sdu[2] = 0xC3U;
        Can_Hw_PackData(sdu, 3U, &lo, &hi);
        UT_CHECK_EQ(0x00C3B2A1UL, lo);
        UT_CHECK_EQ(0x00000000UL, hi);
        free(sdu);
    }

    /* DLC 0: no payload pointer needed, both registers cleared */
    lo = 0xDEADBEEFUL;
    hi = 0xDEADBEEFUL;
    Can_Hw_PackData(NULL_PTR, 0U, &lo, &hi);
    UT_CHECK_EQ(0UL, lo);
    UT_CHECK_EQ(0UL, hi);
}

static void test_pack_length_above_8_is_clamped(void)
{
    const uint8 sdu[9] = { 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U };
    uint32 lo = 0UL;
    uint32 hi = 0UL;

    Can_Hw_PackData(sdu, 9U, &lo, &hi);
    UT_CHECK_EQ(0x04030201UL, lo);
    UT_CHECK_EQ(0x08070605UL, hi);
}

static void test_unpack_writes_only_length_bytes(void)
{
    uint8 buf[9];

    (void)memset(buf, 0xEE, sizeof(buf));
    Can_Hw_UnpackData(0x44332211UL, 0x88776655UL, 5U, buf);
    UT_CHECK_EQ(0x11U, buf[0]);
    UT_CHECK_EQ(0x44U, buf[3]);
    UT_CHECK_EQ(0x55U, buf[4]);
    UT_CHECK_EQ(0xEEU, buf[5]);     /* untouched */

    (void)memset(buf, 0xEE, sizeof(buf));
    Can_Hw_UnpackData(0x44332211UL, 0x88776655UL, 15U, buf);
    UT_CHECK_EQ(0x88U, buf[7]);
    UT_CHECK_EQ(0xEEU, buf[8]);     /* length clamped to 8 */
}

/* ---------------------------------------------------------- Mailboxes --- */

static void test_find_free_mailbox(void)
{
    UT_CHECK_EQ(0U, Can_Hw_FindFreeMailbox(0x1C000000UL));     /* all empty */
    UT_CHECK_EQ(1U, Can_Hw_FindFreeMailbox(0x18000000UL));     /* 0 pending */
    UT_CHECK_EQ(2U, Can_Hw_FindFreeMailbox(0x10000000UL));
    UT_CHECK_EQ(0U, Can_Hw_FindFreeMailbox(0x14000000UL));     /* 0 and 2 free */
    UT_CHECK_EQ(CAN_HW_NO_MAILBOX, Can_Hw_FindFreeMailbox(0x00000000UL));
    /* Every other TSR bit set (RQCP, TXOK, CODE, LOW...) but no TMEx */
    UT_CHECK_EQ(CAN_HW_NO_MAILBOX, Can_Hw_FindFreeMailbox(0xE3FFFFFFUL));
}

/* ------------------------------------------------------------ Filters --- */

static void test_mask_filter(void)
{
    uint32 fr1 = 0UL;
    uint32 fr2 = 0UL;

    /* Exact match on VehicleSpeed (0x100) */
    Can_Hw_BuildMaskFilter32(0x100UL, 0x7FFUL, &fr1, &fr2);
    UT_CHECK_EQ(0x20000000UL, fr1);
    UT_CHECK_EQ(0xFFE00004UL, fr2);

    /* Range 0x300..0x3FF: only the top 3 ID bits compared */
    Can_Hw_BuildMaskFilter32(0x300UL, 0x700UL, &fr1, &fr2);
    UT_CHECK_EQ(0x60000000UL, fr1);
    UT_CHECK_EQ(0xE0000004UL, fr2);

    /* Extended DiagExtended (0x18FEEF00), exact match */
    Can_Hw_BuildMaskFilter32(CAN_ID_EXTENDED_FLAG | 0x18FEEF00UL, 0x1FFFFFFFUL, &fr1, &fr2);
    UT_CHECK_EQ(0xC7F77804UL, fr1);
    UT_CHECK_EQ(0xFFFFFFFCUL, fr2);

    /* Extended flag in the mask is ignored; the id decides the format */
    Can_Hw_BuildMaskFilter32(0x100UL, CAN_ID_EXTENDED_FLAG | 0x7FFUL, &fr1, &fr2);
    UT_CHECK_EQ(0xFFE00004UL, fr2);

    /* Accept every standard frame, but still no extended ones */
    Can_Hw_BuildMaskFilter32(0x000UL, 0x000UL, &fr1, &fr2);
    UT_CHECK_EQ(0x00000000UL, fr1);
    UT_CHECK_EQ(0x00000004UL, fr2);
}

int main(void)
{
    UT_RUN(test_bit_timing_automotive_rates);
    UT_RUN(test_bit_timing_sample_point_rounding);
    UT_RUN(test_bit_timing_rejects_invalid_input);
    UT_RUN(test_build_btr);
    UT_RUN(test_encode_standard_id);
    UT_RUN(test_encode_extended_id);
    UT_RUN(test_decode_id);
    UT_RUN(test_id_round_trip);
    UT_RUN(test_pack_full_frame);
    UT_RUN(test_pack_short_frame_reads_only_length_bytes);
    UT_RUN(test_pack_length_above_8_is_clamped);
    UT_RUN(test_unpack_writes_only_length_bytes);
    UT_RUN(test_find_free_mailbox);
    UT_RUN(test_mask_filter);
    return UT_REPORT();
}
