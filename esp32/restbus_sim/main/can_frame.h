/**
 * @file    can_frame.h
 * @brief   Classic CAN frame as read from a log, independent of the TWAI driver types.
 */
#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint64_t ts_us;     /* log timestamp, microseconds              */
    uint32_t id;        /* 11-bit or 29-bit identifier              */
    bool     extended;  /* true for a 29-bit identifier             */
    uint8_t  dlc;       /* 0..8                                     */
    uint8_t  data[8];
} can_frame_t;

#endif /* CAN_FRAME_H */
