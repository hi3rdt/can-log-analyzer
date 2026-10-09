/**
 * @file    Com_Signal.c
 * @brief   Generic bit-by-bit signal packing for Intel and Motorola layouts.
 *
 * A bit loop (max 32 iterations) is used instead of 64-bit shifting so the
 * same code handles both byte orders, any alignment and signals that cross
 * byte boundaries. It runs in task context (Com_ReceiveSignal/SendSignal),
 * never in the RX interrupt, which only copies bytes.
 *
 * Bit numbering follows the DBC convention: bit n is bit (n % 8) of byte
 * (n / 8). Intel signals grow towards higher bit numbers from their LSB.
 * Motorola signals start at their MSB and walk the "sawtooth": down to bit 0
 * of the byte, then to bit 7 of the next byte.
 */
#include "Com_Signal.h"

static uint8 Com_NextBitPosition(uint8 bitPos, Com_SignalEndiannessType endianness)
{
    uint8 next;

    if (endianness == COM_LITTLE_ENDIAN)
    {
        next = (uint8)(bitPos + 1U);
    }
    else if ((bitPos & 7U) == 0U)
    {
        next = (uint8)(bitPos + 15U);   /* bit 0 of byte n -> bit 7 of byte n+1 */
    }
    else
    {
        next = (uint8)(bitPos - 1U);
    }

    return next;
}

uint32 Com_SignalUnpack(const uint8 *buffer, uint8 bitPosition, uint8 bitSize,
                        Com_SignalEndiannessType endianness)
{
    uint32 value = 0UL;
    uint8  pos   = bitPosition;
    uint8  i;

    for (i = 0U; i < bitSize; i++)
    {
        uint32 bit = ((uint32)buffer[pos >> 3U] >> (pos & 7U)) & 1UL;

        if (endianness == COM_LITTLE_ENDIAN)
        {
            value |= bit << i;                  /* LSB first */
        }
        else
        {
            value = (value << 1U) | bit;        /* MSB first */
        }
        pos = Com_NextBitPosition(pos, endianness);
    }

    return value;
}

void Com_SignalPack(uint8 *buffer, uint8 bitPosition, uint8 bitSize,
                    Com_SignalEndiannessType endianness, uint32 rawValue)
{
    uint8 pos = bitPosition;
    uint8 i;

    for (i = 0U; i < bitSize; i++)
    {
        uint8  shift = (endianness == COM_LITTLE_ENDIAN) ? i : (uint8)(bitSize - 1U - i);
        uint8  mask  = (uint8)(1U << (pos & 7U));

        if (((rawValue >> shift) & 1UL) != 0UL)
        {
            buffer[pos >> 3U] |= mask;
        }
        else
        {
            buffer[pos >> 3U] &= (uint8)~mask;
        }
        pos = Com_NextBitPosition(pos, endianness);
    }
}

sint32 Com_SignalSignExtend(uint32 rawValue, uint8 bitSize)
{
    uint32 value = rawValue;

    if ((bitSize > 0U) && (bitSize < 32U))
    {
        uint32 signBit = 1UL << (bitSize - 1U);

        if ((value & signBit) != 0UL)
        {
            value |= ~((signBit << 1U) - 1UL);  /* fill upper bits with 1 */
        }
    }

    /* Implementation-defined conversion for values > INT32_MAX; GCC and every
     * Cortex-M compiler use two's complement modulo (MISRA 10.8 deviation). */
    return (sint32)value;
}
