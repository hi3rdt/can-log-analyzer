/**
 * @file    gvret_csv.c
 * @brief   Streaming GVRET CSV parser. Pure C, no ESP-IDF dependency (host-tested).
 */
#include "gvret_csv.h"

#include <string.h>

/* One field of the current line: [p, q) */
typedef struct
{
    const char *p;
    const char *q;
} field_t;

static const char *line_end(const char *pos, const char *end)
{
    while ((pos < end) && (*pos != '\n'))
    {
        pos++;
    }
    return pos;
}

/* Take the next comma-separated field of [*cur, eol). Returns false when the line is exhausted. */
static bool next_field(const char **cur, const char *eol, field_t *f)
{
    const char *p = *cur;

    if (p >= eol)
    {
        return false;
    }
    f->p = p;
    while ((p < eol) && (*p != ',') && (*p != '\r'))
    {
        p++;
    }
    f->q = p;
    /* Step over the separator; '\r' ends the line */
    *cur = ((p < eol) && (*p == ',')) ? (p + 1) : eol;
    return true;
}

static bool parse_dec(const field_t *f, uint64_t *out)
{
    uint64_t v = 0U;
    const char *p;

    if (f->p == f->q)
    {
        return false;
    }
    for (p = f->p; p < f->q; p++)
    {
        if ((*p < '0') || (*p > '9'))
        {
            return false;
        }
        v = (v * 10U) + (uint64_t)(*p - '0');
    }
    *out = v;
    return true;
}

static bool parse_hex(const field_t *f, uint32_t *out)
{
    uint32_t v = 0U;
    const char *p;

    if ((f->p == f->q) || ((f->q - f->p) > 8))
    {
        return false;
    }
    for (p = f->p; p < f->q; p++)
    {
        uint32_t nib;
        if ((*p >= '0') && (*p <= '9'))      { nib = (uint32_t)(*p - '0'); }
        else if ((*p >= 'A') && (*p <= 'F')) { nib = (uint32_t)(*p - 'A') + 10U; }
        else if ((*p >= 'a') && (*p <= 'f')) { nib = (uint32_t)(*p - 'a') + 10U; }
        else                                 { return false; }
        v = (v << 4) | nib;
    }
    *out = v;
    return true;
}

static bool field_is(const field_t *f, const char *s)
{
    size_t n = strlen(s);
    return ((size_t)(f->q - f->p) == n) && (memcmp(f->p, s, n) == 0);
}

/* Parse one data line [p, eol) into frame. */
static bool parse_line(const char *p, const char *eol, can_frame_t *frame)
{
    field_t f;
    uint64_t u64;
    uint32_t u32;
    uint8_t i;

    if (!next_field(&p, eol, &f) || !parse_dec(&f, &u64)) { return false; }
    frame->ts_us = u64;

    if (!next_field(&p, eol, &f) || !parse_hex(&f, &u32)) { return false; }
    frame->id = u32;

    if (!next_field(&p, eol, &f)) { return false; }
    if (field_is(&f, "true"))       { frame->extended = true; }
    else if (field_is(&f, "false")) { frame->extended = false; }
    else                            { return false; }

    if (!next_field(&p, eol, &f)) { return false; }   /* Dir: ignored */
    if (!next_field(&p, eol, &f)) { return false; }   /* Bus: ignored */

    if (!next_field(&p, eol, &f) || !parse_dec(&f, &u64) || (u64 > 8U)) { return false; }
    frame->dlc = (uint8_t)u64;

    if ((frame->extended && (frame->id > 0x1FFFFFFFUL)) ||
        (!frame->extended && (frame->id > 0x7FFUL)))
    {
        return false;
    }

    memset(frame->data, 0, sizeof(frame->data));
    for (i = 0U; i < frame->dlc; i++)
    {
        if (!next_field(&p, eol, &f) || !parse_hex(&f, &u32) || (u32 > 0xFFU))
        {
            return false;
        }
        frame->data[i] = (uint8_t)u32;
    }
    return true;
}

void gvret_csv_open(gvret_csv_t *csv, const char *text, size_t len)
{
    csv->begin = text;
    csv->end   = text + len;
    /* EMBED_TXTFILES appends a NUL; do not treat it as log content */
    while ((csv->end > csv->begin) && (csv->end[-1] == '\0'))
    {
        csv->end--;
    }
    gvret_csv_rewind(csv);
}

void gvret_csv_rewind(gvret_csv_t *csv)
{
    const char *eol = line_end(csv->begin, csv->end);

    /* Skip the header line ("Time Stamp,ID,...") */
    csv->pos  = (eol < csv->end) ? (eol + 1) : eol;
    csv->line = 1U;     /* header consumed */
}

int gvret_csv_next(gvret_csv_t *csv, can_frame_t *frame)
{
    for (;;)
    {
        const char *start = csv->pos;
        const char *eol;

        if (start >= csv->end)
        {
            return 0;
        }
        eol = line_end(start, csv->end);
        csv->pos = (eol < csv->end) ? (eol + 1) : eol;
        csv->line++;

        /* Blank line (or a lone "\r"): skip silently */
        if ((eol == start) || ((eol == (start + 1)) && (*start == '\r')))
        {
            continue;
        }
        return parse_line(start, eol, frame) ? 1 : -1;
    }
}
