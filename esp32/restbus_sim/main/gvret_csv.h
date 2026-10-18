/**
 * @file    gvret_csv.h
 * @brief   Streaming parser for SavvyCAN GVRET CSV logs held in memory (no copy, no malloc).
 *
 * Format (header line first, timestamps in microseconds):
 *   Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8
 *   15,00000300,false,Rx,0,4,00,00,02,00,
 */
#ifndef GVRET_CSV_H
#define GVRET_CSV_H

#include <stddef.h>
#include "can_frame.h"

typedef struct
{
    const char *begin;  /* first byte of the log text                     */
    const char *end;    /* one past the last byte                         */
    const char *pos;    /* parse cursor                                   */
    unsigned    line;   /* 1-based number of the line parsed last          */
} gvret_csv_t;

/** Bind the parser to a text buffer and skip the header line. */
void gvret_csv_open(gvret_csv_t *csv, const char *text, size_t len);

/** Move the cursor back to the first frame. */
void gvret_csv_rewind(gvret_csv_t *csv);

/**
 * Parse the next frame.
 * @return  1 frame parsed, 0 end of log, -1 malformed line (skipped; its number is csv->line)
 */
int gvret_csv_next(gvret_csv_t *csv, can_frame_t *frame);

#endif /* GVRET_CSV_H */
