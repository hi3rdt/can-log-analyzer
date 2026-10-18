/**
 * @file    test_restbus.c
 * @brief   Host unit tests for the pure-C parts of the ESP32 rest-bus simulator
 *          (esp32/restbus_sim): GVRET CSV parser and fault injection.
 */
#include <stdlib.h>
#include <string.h>

#include "unit_test.h"
#include "gvret_csv.h"
#include "fault_inject.h"

/* ------------------------------------------------------------ parser --- */

static void test_parse_basic_lines(void)
{
    static const char log[] =
        "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
        "15,00000300,false,Rx,0,4,00,0a,02,FF,\r\n"
        "193,18FEEF00,true,Rx,0,8,39,30,00,37,00,00,00,00,\n"
        "\n"
        "200,00000100,false,Rx,0,0,\n";
    gvret_csv_t csv;
    can_frame_t f;

    gvret_csv_open(&csv, log, sizeof(log));     /* includes the NUL, like EMBED_TXTFILES */

    UT_CHECK_EQ(1, gvret_csv_next(&csv, &f));
    UT_CHECK_EQ(15U, f.ts_us);
    UT_CHECK_EQ(0x300U, f.id);
    UT_CHECK(!f.extended);
    UT_CHECK_EQ(4U, f.dlc);
    UT_CHECK_EQ(0x0AU, f.data[1]);
    UT_CHECK_EQ(0xFFU, f.data[3]);
    UT_CHECK_EQ(0x00U, f.data[4]);              /* unused bytes cleared */
    UT_CHECK_EQ(2U, csv.line);

    UT_CHECK_EQ(1, gvret_csv_next(&csv, &f));
    UT_CHECK_EQ(0x18FEEF00UL, f.id);
    UT_CHECK(f.extended);
    UT_CHECK_EQ(8U, f.dlc);
    UT_CHECK_EQ(0x37U, f.data[3]);

    UT_CHECK_EQ(1, gvret_csv_next(&csv, &f));  /* blank line skipped */
    UT_CHECK_EQ(0U, f.dlc);
    UT_CHECK_EQ(5U, csv.line);

    UT_CHECK_EQ(0, gvret_csv_next(&csv, &f));
    UT_CHECK_EQ(0, gvret_csv_next(&csv, &f));  /* stays at end */

    gvret_csv_rewind(&csv);
    UT_CHECK_EQ(1, gvret_csv_next(&csv, &f));
    UT_CHECK_EQ(0x300U, f.id);
}

static void test_parse_rejects_malformed(void)
{
    static const char log[] =
        "header\n"
        "x,00000300,false,Rx,0,1,00,\n"         /* bad timestamp          */
        "1,00000800,false,Rx,0,1,00,\n"         /* 11-bit id out of range */
        "1,00000100,maybe,Rx,0,1,00,\n"         /* bad Extended flag      */
        "1,00000100,false,Rx,0,9,00,\n"         /* DLC > 8                */
        "1,00000100,false,Rx,0,2,00,\n"         /* fewer bytes than DLC   */
        "1,00000100,false,Rx,0,1,1FF,\n"        /* byte > 0xFF            */
        "2,00000100,false,Rx,0,1,AB,\n";
    gvret_csv_t csv;
    can_frame_t f;
    int i;

    gvret_csv_open(&csv, log, strlen(log));
    for (i = 0; i < 6; i++)
    {
        UT_CHECK_EQ(-1, gvret_csv_next(&csv, &f));
    }
    UT_CHECK_EQ(7U, csv.line);
    UT_CHECK_EQ(1, gvret_csv_next(&csv, &f));
    UT_CHECK_EQ(0xABU, f.data[0]);
    UT_CHECK_EQ(0, gvret_csv_next(&csv, &f));
}

static char *read_file(const char *path, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    char *buf = NULL;
    long n;

    if (fp == NULL)
    {
        return NULL;
    }
    if ((fseek(fp, 0, SEEK_END) == 0) && ((n = ftell(fp)) > 0) && (fseek(fp, 0, SEEK_SET) == 0))
    {
        buf = malloc((size_t)n);
        if ((buf != NULL) && (fread(buf, 1U, (size_t)n, fp) != (size_t)n))
        {
            free(buf);
            buf = NULL;
        }
        *len = (size_t)n;
    }
    (void)fclose(fp);
    return buf;
}

/* The whole committed demo log parses, and timestamps never go backwards. */
static void test_parse_demo_log(void)
{
    size_t len = 0U;
    char *text = read_file(SAMPLE_CSV, &len);
    gvret_csv_t csv;
    can_frame_t f;
    unsigned frames = 0U;
    unsigned errors = 0U;
    unsigned speed_frames = 0U;
    uint64_t last_ts = 0U;
    int r;

    UT_CHECK(text != NULL);
    if (text == NULL)
    {
        return;
    }
    gvret_csv_open(&csv, text, len);
    while ((r = gvret_csv_next(&csv, &f)) != 0)
    {
        if (r < 0)
        {
            errors++;
            continue;
        }
        UT_CHECK(f.ts_us >= last_ts);
        last_ts = f.ts_us;
        frames++;
        if (f.id == 0x100U)
        {
            speed_frames++;
        }
    }
    UT_CHECK_EQ(0U, errors);
    UT_CHECK_EQ(5130U, frames);
    UT_CHECK_EQ(3000U, speed_frames);          /* 30 s at 10 ms */
    free(text);
}

/* ------------------------------------------------------------ faults --- */

static can_frame_t make_frame(uint32_t id, uint8_t first)
{
    can_frame_t f;
    uint8_t i;

    memset(&f, 0, sizeof(f));
    f.id  = id;
    f.dlc = 8U;
    for (i = 0U; i < 8U; i++)
    {
        f.data[i] = (uint8_t)(first + i);
    }
    return f;
}

static void test_fault_kinds(void)
{
    fault_table_t t;
    can_frame_t f;

    fault_inject_init(&t);
    f = make_frame(0x100U, 0x10U);
    UT_CHECK_EQ(1U, fault_inject_apply(&t, &f));            /* no rule: untouched */
    UT_CHECK_EQ(0x17U, f.data[7]);

    UT_CHECK(fault_inject_set(&t, 0x100U, false, FAULT_DROP));
    UT_CHECK_EQ(0U, fault_inject_apply(&t, &f));
    f = make_frame(0x200U, 0x10U);
    UT_CHECK_EQ(1U, fault_inject_apply(&t, &f));            /* other id unaffected */

    UT_CHECK(fault_inject_set(&t, 0x100U, false, FAULT_CORRUPT));  /* replaces DROP */
    f = make_frame(0x100U, 0x10U);
    UT_CHECK_EQ(1U, fault_inject_apply(&t, &f));
    UT_CHECK_EQ(0x97U, f.data[7]);                          /* bit 7 of last byte flipped */
    UT_CHECK_EQ(0x16U, f.data[6]);

    UT_CHECK(fault_inject_set(&t, 0x100U, false, FAULT_FREEZE));
    f = make_frame(0x100U, 0x10U);
    UT_CHECK_EQ(1U, fault_inject_apply(&t, &f));
    f = make_frame(0x100U, 0x40U);
    UT_CHECK_EQ(1U, fault_inject_apply(&t, &f));
    UT_CHECK_EQ(0x10U, f.data[0]);                          /* first payload held */
    UT_CHECK_EQ(0x17U, f.data[7]);

    UT_CHECK(fault_inject_set(&t, 0x100U, false, FAULT_REPEAT));
    UT_CHECK_EQ(2U, fault_inject_apply(&t, &f));

    UT_CHECK(fault_inject_set(&t, 0x100U, false, FAULT_NONE)); /* "none" clears */
    UT_CHECK_EQ(1U, fault_inject_apply(&t, &f));
    UT_CHECK(!fault_inject_clear(&t, 0x100U, false));
}

static void test_fault_ext_id_and_table_full(void)
{
    fault_table_t t;
    can_frame_t f;
    uint32_t i;

    fault_inject_init(&t);
    UT_CHECK(fault_inject_set(&t, 0x100U, true, FAULT_DROP));
    f = make_frame(0x100U, 0U);                             /* standard 0x100 */
    UT_CHECK_EQ(1U, fault_inject_apply(&t, &f));
    f.extended = true;
    UT_CHECK_EQ(0U, fault_inject_apply(&t, &f));

    for (i = 1U; i < FAULT_MAX_RULES; i++)
    {
        UT_CHECK(fault_inject_set(&t, 0x200U + i, false, FAULT_DROP));
    }
    UT_CHECK(!fault_inject_set(&t, 0x7FFU, false, FAULT_DROP));
    UT_CHECK(fault_inject_set(&t, 0x201U, false, FAULT_REPEAT));   /* update still fits */
    fault_inject_clear_all(&t);
    UT_CHECK(fault_inject_set(&t, 0x7FFU, false, FAULT_DROP));
}

int main(void)
{
    UT_RUN(test_parse_basic_lines);
    UT_RUN(test_parse_rejects_malformed);
    UT_RUN(test_parse_demo_log);
    UT_RUN(test_fault_kinds);
    UT_RUN(test_fault_ext_id_and_table_full);
    return UT_REPORT();
}
