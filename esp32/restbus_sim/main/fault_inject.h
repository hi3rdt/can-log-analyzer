/**
 * @file    fault_inject.h
 * @brief   Per-ID fault injection applied to replayed frames before transmission.
 *
 * Each fault targets one failure mode the cluster's signal layer must detect:
 *   DROP     -> message disappears             (deadline monitoring / timeout)
 *   CORRUPT  -> bit 7 of the last byte flipped  (checksum / CRC check)
 *   FREEZE   -> first payload repeated forever  (alive counter stuck: sender hung)
 *   REPEAT   -> every frame sent twice          (counter repetition)
 *
 * Pure C, no ESP-IDF dependency (host-tested). Not thread-safe by itself: the
 * caller serialises fault_inject_set/clear (console) against fault_inject_apply (replay).
 */
#ifndef FAULT_INJECT_H
#define FAULT_INJECT_H

#include <stdbool.h>
#include <stdint.h>
#include "can_frame.h"

#define FAULT_MAX_RULES 8U

typedef enum
{
    FAULT_NONE = 0,
    FAULT_DROP,
    FAULT_CORRUPT,
    FAULT_FREEZE,
    FAULT_REPEAT
} fault_kind_t;

typedef struct
{
    uint32_t     id;
    bool         extended;
    fault_kind_t kind;
    bool         frozen_valid;      /* FREEZE: payload captured yet?   */
    uint8_t      frozen_data[8];
    uint32_t     hits;              /* frames affected so far          */
} fault_rule_t;

typedef struct
{
    fault_rule_t rule[FAULT_MAX_RULES];
} fault_table_t;

void fault_inject_init(fault_table_t *t);

/** Add or replace the rule for (id, extended). @return false if the table is full. */
bool fault_inject_set(fault_table_t *t, uint32_t id, bool extended, fault_kind_t kind);

/** Remove the rule for (id, extended). @return false if there was none. */
bool fault_inject_clear(fault_table_t *t, uint32_t id, bool extended);

void fault_inject_clear_all(fault_table_t *t);

/**
 * Apply the matching rule to a frame (modified in place).
 * @return number of copies to transmit: 0 (dropped), 1, or 2 (REPEAT)
 */
uint8_t fault_inject_apply(fault_table_t *t, can_frame_t *frame);

const char *fault_kind_name(fault_kind_t kind);

#endif /* FAULT_INJECT_H */
