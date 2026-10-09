/**
 * @file    Can_Hw.h
 * @brief   Pure bxCAN helper functions (no register access).
 *
 * Exercise 1 (docs/exercises/ex01-bxcan-driver.vi.md): implement these in
 * Can_Hw.c. They compute register *values*; Can.c writes them to the bxCAN
 * registers. Keeping the bit fiddling out of Can.c means it can be unit-tested
 * on the host (test/host/test_can_hw.c) before it ever runs on the Blue Pill.
 *
 * Register layouts referenced below are from RM0008 rev 21, section 24.9
 * (bxCAN registers): CAN_BTR, CAN_TIxR / CAN_RIxR, CAN_TDLxR / CAN_TDHxR,
 * CAN_TSR and CAN_FiRx.
 */
#ifndef CAN_HW_H
#define CAN_HW_H

#include "Can_GeneralTypes.h"

/* ------------------------------------------------------- Bit timing --- */

/** Bit timing in time quanta (TQ). Values are real counts, not "minus one". */
typedef struct
{
    uint16 prescaler;   /* BRP + 1, 1..1024                                   */
    uint8  bs1;         /* TS1 + 1, 1..16 (PROP_SEG + PHASE_SEG1)             */
    uint8  bs2;         /* TS2 + 1, 1..8  (PHASE_SEG2)                        */
    uint8  sjw;         /* SJW + 1, 1..4                                      */
} Can_Hw_BitTimingType;

/** Test modes encoded in CAN_BTR bits 30 (LBKM) and 31 (SILM). */
typedef enum
{
    CAN_HW_MODE_NORMAL          = 0,
    CAN_HW_MODE_LOOPBACK        = 1,
    CAN_HW_MODE_SILENT          = 2,
    CAN_HW_MODE_SILENT_LOOPBACK = 3
} Can_Hw_TestModeType;

/**
 * Compute a bit timing for the requested bitrate and sample point.
 *
 * Contract (the unit tests check exactly this, so other valid solutions are
 * deliberately not accepted):
 *  - Try total TQ per bit (1 + bs1 + bs2) from 25 down to 8 and return the
 *    FIRST candidate that satisfies all of:
 *      a) pclkHz is an exact multiple of (bitrate * tq)
 *      b) prescaler = pclkHz / (bitrate * tq) is in 1..1024
 *      c) samplePos = (tq * samplePointPermille + 500) / 1000   (integer,
 *         rounded; counts SYNC_SEG, so the sample point is after TQ samplePos)
 *         bs1 = samplePos - 1 is in 1..16 and bs2 = tq - samplePos is in 1..8
 *  - sjw is always 1.
 *  - Returns E_NOT_OK (and leaves *timing untouched) if timing is NULL_PTR,
 *    bitrate is 0, samplePointPermille is outside 500..950, or no candidate
 *    fits.
 *
 * Example: 36 MHz, 500 kbit/s, 875 permille -> prescaler 4, bs1 15, bs2 2,
 * sjw 1 (18 TQ, sample point 16/18 = 88.9 %).
 */
Std_ReturnType Can_Hw_CalcBitTiming(uint32 pclkHz,
                                    uint32 bitrate,
                                    uint16 samplePointPermille,
                                    Can_Hw_BitTimingType *timing);

/**
 * Build the CAN_BTR register value: BRP[9:0], TS1[19:16], TS2[22:20],
 * SJW[25:24], LBKM[30], SILM[31]. All fields store "value - 1".
 * The caller guarantees *timing is valid (e.g. from Can_Hw_CalcBitTiming).
 */
uint32 Can_Hw_BuildBtr(const Can_Hw_BitTimingType *timing, Can_Hw_TestModeType mode);

/* ------------------------------------------------------ Identifiers --- */

/**
 * Encode a Can_IdType (bit 31 = extended, see Can_GeneralTypes.h) into the
 * CAN_TIxR layout: standard -> STID[31:21], IDE = 0; extended ->
 * EXID[31:3], IDE[2] = 1. RTR[1] and TXRQ[0] are always 0 (Can.c sets TXRQ
 * itself as the last write). ID bits outside the 11/29-bit range are masked.
 */
uint32 Can_Hw_EncodeId(Can_IdType id);

/**
 * Decode a CAN_RIxR value back into Can_IdType (inverse of Can_Hw_EncodeId).
 * RTR[1] and bit 0 are ignored.
 */
Can_IdType Can_Hw_DecodeId(uint32 rir);

/* ----------------------------------------------------------- Payload --- */

/**
 * Pack length bytes of sdu into CAN_TDLxR (bytes 0..3, byte 0 in bits 7:0)
 * and CAN_TDHxR (bytes 4..7). Bytes at index >= length are 0.
 * Must never read sdu[length] or beyond (the host test runs under ASan).
 * length > 8 is treated as 8. sdu may be NULL_PTR only when length is 0.
 */
void Can_Hw_PackData(const uint8 *sdu, uint8 length, uint32 *tdlr, uint32 *tdhr);

/**
 * Unpack CAN_RDLxR / CAN_RDHxR into sdu[0..length-1]. Must never write
 * sdu[length] or beyond. length > 8 is treated as 8.
 */
void Can_Hw_UnpackData(uint32 rdlr, uint32 rdhr, uint8 length, uint8 *sdu);

/* ---------------------------------------------------------- Mailboxes --- */

#define CAN_HW_NO_MAILBOX       0xFFU

/**
 * Return the lowest-numbered empty TX mailbox (0..2) according to the TME0,
 * TME1, TME2 bits (26, 27, 28) of a CAN_TSR value, or CAN_HW_NO_MAILBOX if
 * all three are pending.
 */
uint8 Can_Hw_FindFreeMailbox(uint32 tsr);

/* ------------------------------------------------------------ Filters --- */

/**
 * Build a 32-bit scale, identifier-mask filter bank (CAN_FiR1 = identifier,
 * CAN_FiR2 = mask). Both registers use the CAN_RIxR layout, so reuse
 * Can_Hw_EncodeId. Mask bit = 1 means "must match".
 * The IDE bit is always set in the mask, so a standard filter never matches
 * extended frames and vice versa. RTR is never compared (mask bit 1 = 0).
 * The extended flag of id selects the frame format; the flag in mask is
 * ignored.
 */
void Can_Hw_BuildMaskFilter32(Can_IdType id, Can_IdType mask, uint32 *fr1, uint32 *fr2);

#endif /* CAN_HW_H */
