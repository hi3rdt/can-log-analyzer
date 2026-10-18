/**
 * @file    replay.c
 * @brief   Rest-bus replay task.
 *
 * Timing: frame k is due at wall_t0 + (ts_k - log_t0) * 100 / speed_pct.
 * The task sleeps with vTaskDelay for the whole milliseconds and busy-waits the
 * sub-millisecond rest, so frames keep the log's ~10 ms / 20 ms / 100 ms periods
 * with well under 1 ms of added jitter. After pause, speed change or wrap-around
 * the time base is re-anchored on the next frame.
 *
 * On wrap-around the alive counters in the log jump (e.g. 0x100 SpeedCounter):
 * the cluster sees one counter error per pass, as it would on a real ECU reset.
 */
#include "replay.h"

#include <inttypes.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "can_bus.h"
#include "gvret_csv.h"

static const char *TAG = "replay";

/* samples/demo_drive.csv, embedded by main/CMakeLists.txt (EMBED_TXTFILES) */
extern const char log_start[] asm("_binary_demo_drive_csv_start");
extern const char log_end[]   asm("_binary_demo_drive_csv_end");

static gvret_csv_t      s_csv;
static fault_table_t    s_faults;
static replay_stats_t   s_stats;
static portMUX_TYPE     s_lock = portMUX_INITIALIZER_UNLOCKED;

static volatile bool     s_playing;
static volatile bool     s_silence;
static volatile bool     s_rebase = true;
static volatile uint32_t s_speed_pct = 100U;

static void wait_until(int64_t due_us)
{
    for (;;)
    {
        int64_t rem = due_us - esp_timer_get_time();
        if (rem <= 0)
        {
            return;
        }
        if (rem >= 1000)
        {
            vTaskDelay(pdMS_TO_TICKS((uint32_t)(rem / 1000)));
        }
        else
        {
            esp_rom_delay_us((uint32_t)rem);
        }
    }
}

static void replay_task(void *arg)
{
    can_frame_t frame;
    uint64_t log_t0 = 0U;
    int64_t  wall_t0 = 0;

    (void)arg;
    for (;;)
    {
        int r;
        int64_t due;
        int64_t late;
        uint8_t copies;

        if (!s_playing)
        {
            s_rebase = true;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        r = gvret_csv_next(&s_csv, &frame);
        if (r == 0)
        {
            gvret_csv_rewind(&s_csv);
            s_stats.loops++;
            s_rebase = true;
            continue;
        }
        if (r < 0)
        {
            s_stats.parse_errors++;
            ESP_LOGW(TAG, "malformed log line %u skipped", s_csv.line);
            continue;
        }

        if (s_rebase)
        {
            s_rebase = false;
            log_t0  = frame.ts_us;
            wall_t0 = esp_timer_get_time();
        }

        due = wall_t0 + (int64_t)(((frame.ts_us - log_t0) * 100U) / s_speed_pct);
        wait_until(due);
        late = esp_timer_get_time() - due;
        if ((late > 0) && ((uint64_t)late > s_stats.late_max_us))
        {
            s_stats.late_max_us = (uint32_t)late;
        }

        taskENTER_CRITICAL(&s_lock);
        copies = fault_inject_apply(&s_faults, &frame);
        taskEXIT_CRITICAL(&s_lock);

        if (s_silence || (copies == 0U))
        {
            s_stats.frames_dropped++;
            continue;
        }
        while (copies > 0U)
        {
            if (can_bus_send(&frame) == ESP_OK)
            {
                s_stats.frames_sent++;
            }
            copies--;
        }
    }
}

void replay_start(bool play)
{
    fault_inject_init(&s_faults);
    gvret_csv_open(&s_csv, log_start, (size_t)(log_end - log_start));
    s_playing = play;
    (void)xTaskCreatePinnedToCore(replay_task, "replay", APP_REPLAY_STACK_BYTES, NULL,
                                  APP_REPLAY_TASK_PRIO, NULL, APP_REPLAY_TASK_CORE);
    ESP_LOGI(TAG, "log embedded: %u bytes, %s", (unsigned)(log_end - log_start),
             play ? "playing" : "paused");
}

void replay_set_playing(bool play)
{
    s_playing = play;
}

bool replay_is_playing(void)
{
    return s_playing;
}

bool replay_set_speed(uint32_t percent)
{
    if ((percent < 25U) || (percent > 1000U))
    {
        return false;
    }
    s_speed_pct = percent;
    s_rebase = true;
    return true;
}

uint32_t replay_speed(void)
{
    return s_speed_pct;
}

void replay_set_silence(bool on)
{
    s_silence = on;
}

bool replay_silence(void)
{
    return s_silence;
}

bool replay_fault_set(uint32_t id, bool extended, fault_kind_t kind)
{
    bool ok;

    taskENTER_CRITICAL(&s_lock);
    ok = fault_inject_set(&s_faults, id, extended, kind);
    taskEXIT_CRITICAL(&s_lock);
    return ok;
}

void replay_fault_clear_all(void)
{
    taskENTER_CRITICAL(&s_lock);
    fault_inject_clear_all(&s_faults);
    taskEXIT_CRITICAL(&s_lock);
}

void replay_fault_print(void)
{
    fault_table_t copy;
    unsigned n = 0U;

    taskENTER_CRITICAL(&s_lock);
    copy = s_faults;
    taskEXIT_CRITICAL(&s_lock);

    for (uint8_t i = 0U; i < FAULT_MAX_RULES; i++)
    {
        const fault_rule_t *r = &copy.rule[i];
        if (r->kind != FAULT_NONE)
        {
            printf("fault  : %s 0x%" PRIX32 " %s, %" PRIu32 " frames affected\n",
                   r->extended ? "EXT" : "STD", r->id, fault_kind_name(r->kind), r->hits);
            n++;
        }
    }
    if (n == 0U)
    {
        printf("fault  : none\n");
    }
}

void replay_get_stats(replay_stats_t *out)
{
    /* Counters are written by one task only; a torn read just shows a stale value */
    *out = s_stats;
}
