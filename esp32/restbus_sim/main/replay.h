/**
 * @file    replay.h
 * @brief   Rest-bus replay: sends the frames of an embedded GVRET log with their
 *          original timing, through the fault injection table.
 */
#ifndef REPLAY_H
#define REPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "fault_inject.h"

typedef struct
{
    uint32_t frames_sent;
    uint32_t frames_dropped;    /* removed by fault injection or "silence" */
    uint32_t parse_errors;
    uint32_t loops;             /* completed passes over the log           */
    uint32_t late_max_us;       /* worst lateness against the log timing   */
} replay_stats_t;

/** Start the replay task (paused or playing). */
void replay_start(bool play);

void replay_set_playing(bool play);
bool replay_is_playing(void);

/** Playback speed in percent of real time (25..1000). */
bool replay_set_speed(uint32_t percent);
uint32_t replay_speed(void);

/** true: send nothing at all (simulates every other ECU going silent). */
void replay_set_silence(bool on);
bool replay_silence(void);

bool replay_fault_set(uint32_t id, bool extended, fault_kind_t kind);
void replay_fault_clear_all(void);
void replay_fault_print(void);

void replay_get_stats(replay_stats_t *out);

#endif /* REPLAY_H */
