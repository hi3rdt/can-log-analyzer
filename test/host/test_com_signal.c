/**
 * @file    test_com_signal.c
 * @brief   Unit tests for bit-level signal packing (hand-computed expectations).
 *          The cantools cross-check lives in test_comstack.c.
 */
#include <string.h>
#include "unit_test.h"
#include "Com_Signal.h"

static void test_intel_byte_aligned(void)
{
    const uint8 buf[8] = { 0x34U, 0x12U, 0U, 0U, 0U, 0U, 0U, 0U };
    UT_CHECK_EQ(0x1234U, Com_SignalUnpack(buf, 0U, 16U, COM_LITTLE_ENDIAN));
}

static void test_intel_cross_byte(void)
{
    /* 12-bit signal at bit 4: low nibble in byte 0 bits 4..7, rest in byte 1 */
    const uint8 buf[8] = { 0xB0U, 0x5AU, 0U, 0U, 0U, 0U, 0U, 0U };
    UT_CHECK_EQ(0x5ABU, Com_SignalUnpack(buf, 4U, 12U, COM_LITTLE_ENDIAN));
}

static void test_motorola_byte_aligned(void)
{
    /* Motorola start bit 7 = MSB of byte 0: 0x12 0x34 -> 0x1234 */
    const uint8 buf[8] = { 0x12U, 0x34U, 0U, 0U, 0U, 0U, 0U, 0U };
    UT_CHECK_EQ(0x1234U, Com_SignalUnpack(buf, 7U, 16U, COM_BIG_ENDIAN));
}

static void test_motorola_cross_byte(void)
{
    /* 11 bits, MSB at bit 37 (byte 4 bit 5): byte4 bits 5..0, byte5 bits 7..3 */
    uint8 buf[8] = { 0U };
    Com_SignalPack(buf, 37U, 11U, COM_BIG_ENDIAN, 0x5A5U);  /* 101 1010 0101 */
    UT_CHECK_EQ(0x2DU, buf[4]);  /* 10 1101 -> top 6 bits */
    UT_CHECK_EQ(0x28U, buf[5]);  /* 00101 << 3 */
    UT_CHECK_EQ(0x5A5U, Com_SignalUnpack(buf, 37U, 11U, COM_BIG_ENDIAN));
}

static void test_full_32_bit(void)
{
    uint8 buf[8] = { 0U };
    Com_SignalPack(buf, 0U, 32U, COM_LITTLE_ENDIAN, 0xDEADBEEFUL);
    UT_CHECK_EQ(0xDEADBEEFUL, Com_SignalUnpack(buf, 0U, 32U, COM_LITTLE_ENDIAN));
    Com_SignalPack(buf, 39U, 32U, COM_BIG_ENDIAN, 0xCAFEF00DUL);
    UT_CHECK_EQ(0xCAFEF00DUL, Com_SignalUnpack(buf, 39U, 32U, COM_BIG_ENDIAN));
    UT_CHECK_EQ(0xCAU, buf[4]);
    UT_CHECK_EQ(0x0DU, buf[7]);
}

static void test_pack_preserves_neighbours(void)
{
    uint8 buf[8];
    memset(buf, 0xFF, sizeof(buf));
    Com_SignalPack(buf, 2U, 3U, COM_LITTLE_ENDIAN, 0U);
    UT_CHECK_EQ(0xE3U, buf[0]);
    UT_CHECK_EQ(0xFFU, buf[1]);
}

static void test_pack_truncates_to_size(void)
{
    uint8 buf[8] = { 0U };
    Com_SignalPack(buf, 0U, 4U, COM_LITTLE_ENDIAN, 0xFFFFFFFFUL);
    UT_CHECK_EQ(0x0FU, buf[0]);
    UT_CHECK_EQ(0x00U, buf[1]);
}

static void test_sign_extend(void)
{
    UT_CHECK_EQ(-1,     Com_SignalSignExtend(0x7FFU, 11U));
    UT_CHECK_EQ(-1024,  Com_SignalSignExtend(0x400U, 11U));
    UT_CHECK_EQ(1023,   Com_SignalSignExtend(0x3FFU, 11U));
    UT_CHECK_EQ(-1,     Com_SignalSignExtend(1U, 1U));
    UT_CHECK_EQ(-2,     Com_SignalSignExtend(0xFFFFFFFEUL, 32U));
}

int main(void)
{
    UT_RUN(test_intel_byte_aligned);
    UT_RUN(test_intel_cross_byte);
    UT_RUN(test_motorola_byte_aligned);
    UT_RUN(test_motorola_cross_byte);
    UT_RUN(test_full_32_bit);
    UT_RUN(test_pack_preserves_neighbours);
    UT_RUN(test_pack_truncates_to_size);
    UT_RUN(test_sign_extend);
    return UT_REPORT();
}
