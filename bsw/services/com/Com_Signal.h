/**
 * @file    Com_Signal.h
 * @brief   Bit-level signal packing used by Com (internal, also unit-tested).
 */
#ifndef COM_SIGNAL_H
#define COM_SIGNAL_H

#include "Com_Types.h"

/** Extract a raw signal (zero-extended) from a PDU buffer. */
uint32 Com_SignalUnpack(const uint8 *buffer, uint8 bitPosition, uint8 bitSize,
                        Com_SignalEndiannessType endianness);

/** Write the low bitSize bits of rawValue into a PDU buffer, other bits untouched. */
void Com_SignalPack(uint8 *buffer, uint8 bitPosition, uint8 bitSize,
                    Com_SignalEndiannessType endianness, uint32 rawValue);

/** Two's complement sign extension of a bitSize-bit raw value. */
sint32 Com_SignalSignExtend(uint32 rawValue, uint8 bitSize);

#endif /* COM_SIGNAL_H */
