/**
 * @file    can_bus.c
 * @brief   TWAI (CAN) bring-up, transmit wrapper, RX logging and bus-off recovery.
 */
#include "can_bus.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "driver/twai.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"

static const char *TAG = "can_bus";

/* Survives esp_restart() (not a power cycle): the mode chosen with "mode noack" */
#define MODE_MAGIC  0x4D4F4445UL    /* "MODE" */
static RTC_NOINIT_ATTR uint32_t s_mode_magic;
static RTC_NOINIT_ATTR uint32_t s_mode_saved;

static can_bus_mode_t   s_mode = CAN_BUS_MODE_NORMAL;
static volatile bool    s_rx_log;
static can_bus_stats_t  s_stats;
static portMUX_TYPE     s_lock = portMUX_INITIALIZER_UNLOCKED;

#define STAT_INC(field)  do { taskENTER_CRITICAL(&s_lock); s_stats.field++; taskEXIT_CRITICAL(&s_lock); } while (0)

static void rx_task(void *arg)
{
    twai_message_t msg;

    (void)arg;
    for (;;)
    {
        if (twai_receive(&msg, portMAX_DELAY) == ESP_OK)
        {
            STAT_INC(rx_frames);
            if (s_rx_log)
            {
                char hex[3U * 8U + 1U];
                size_t n = 0U;
                for (uint8_t i = 0U; (i < msg.data_length_code) && (i < 8U); i++)
                {
                    n += (size_t)snprintf(&hex[n], sizeof(hex) - n, "%02X ", msg.data[i]);
                }
                hex[n] = '\0';
                printf("RX %s %08" PRIX32 " [%u] %s\n", msg.extd ? "EXT" : "STD",
                       msg.identifier, msg.data_length_code, hex);
            }
        }
    }
}

/* Bus-off handling: report, recover (128 x 11 recessive bits), restart. */
static void monitor_task(void *arg)
{
    uint32_t alerts;

    (void)arg;
    for (;;)
    {
        if (twai_read_alerts(&alerts, portMAX_DELAY) != ESP_OK)
        {
            continue;
        }
        if ((alerts & TWAI_ALERT_ERR_PASS) != 0U)
        {
            ESP_LOGW(TAG, "error passive (no ACK? check STM32, wiring, 120R termination)");
        }
        if ((alerts & TWAI_ALERT_BUS_OFF) != 0U)
        {
            STAT_INC(bus_off_events);
            ESP_LOGE(TAG, "bus-off, starting recovery");
            (void)twai_initiate_recovery();
        }
        if ((alerts & TWAI_ALERT_BUS_RECOVERED) != 0U)
        {
            ESP_LOGI(TAG, "bus recovered, restarting driver");
            (void)twai_start();
        }
        if ((alerts & TWAI_ALERT_ERR_ACTIVE) != 0U)
        {
            ESP_LOGI(TAG, "error active again");
        }
    }
}

esp_err_t can_bus_init(void)
{
    twai_mode_t twai_mode = TWAI_MODE_NORMAL;
    esp_err_t err;

    if ((s_mode_magic == MODE_MAGIC) && (s_mode_saved == (uint32_t)CAN_BUS_MODE_NO_ACK))
    {
        s_mode = CAN_BUS_MODE_NO_ACK;
        twai_mode = TWAI_MODE_NO_ACK;
    }

    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(APP_TWAI_TX_GPIO, APP_TWAI_RX_GPIO, twai_mode);
    g_config.tx_queue_len   = APP_TWAI_TX_QUEUE_LEN;
    g_config.rx_queue_len   = APP_TWAI_RX_QUEUE_LEN;
    g_config.alerts_enabled = TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF |
                              TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_ERR_ACTIVE;

    /* 500 kbit/s with the automotive sample point instead of the IDF default (75 %):
     * 80 MHz APB / 10 = 8 MHz TQ clock, 1 + 13 + 2 = 16 TQ per bit, sample point
     * 14/16 = 87.5 %, close to the STM32 setting (16/18 = 88.9 %). */
    twai_timing_config_t  t_config =
    {
        .clk_src              = TWAI_CLK_SRC_DEFAULT,
        .quanta_resolution_hz = 8000000U,
        .brp                  = 0U,
        .tseg_1               = 13U,
        .tseg_2               = 2U,
        .sjw                  = 2U,
        .triple_sampling      = false,
    };
    twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK)
    {
        return err;
    }
    err = twai_start();
    if (err != ESP_OK)
    {
        return err;
    }

    (void)xTaskCreate(rx_task, "can_rx", APP_MONITOR_STACK_BYTES, NULL, APP_MONITOR_TASK_PRIO, NULL);
    (void)xTaskCreate(monitor_task, "can_mon", APP_MONITOR_STACK_BYTES, NULL, APP_MONITOR_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "TWAI started, 500 kbit/s, TX GPIO%d RX GPIO%d, mode %s",
             APP_TWAI_TX_GPIO, APP_TWAI_RX_GPIO,
             (s_mode == CAN_BUS_MODE_NO_ACK) ? "NO_ACK (self-test)" : "NORMAL");
    return ESP_OK;
}

esp_err_t can_bus_send(const can_frame_t *frame)
{
    twai_message_t msg;
    esp_err_t err;

    memset(&msg, 0, sizeof(msg));
    msg.extd             = frame->extended ? 1U : 0U;
    msg.self             = (s_mode == CAN_BUS_MODE_NO_ACK) ? 1U : 0U;  /* loop back in self-test */
    msg.identifier       = frame->id;
    msg.data_length_code = frame->dlc;
    memcpy(msg.data, frame->data, sizeof(msg.data));

    err = twai_transmit(&msg, 0);
    if (err == ESP_OK)
    {
        STAT_INC(tx_ok);
    }
    else if (err == ESP_ERR_TIMEOUT)
    {
        STAT_INC(tx_queue_full);
    }
    else
    {
        STAT_INC(tx_bus_off);
    }
    return err;
}

can_bus_mode_t can_bus_mode(void)
{
    return s_mode;
}

void can_bus_set_mode_and_restart(can_bus_mode_t mode)
{
    s_mode_saved = (uint32_t)mode;
    s_mode_magic = MODE_MAGIC;
    printf("Restarting in %s mode...\n", (mode == CAN_BUS_MODE_NO_ACK) ? "noack" : "normal");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

void can_bus_set_rx_log(bool on)
{
    s_rx_log = on;
}

void can_bus_get_stats(can_bus_stats_t *out)
{
    taskENTER_CRITICAL(&s_lock);
    *out = s_stats;
    taskEXIT_CRITICAL(&s_lock);
}

void can_bus_print_status(void)
{
    static const char *const state_name[] = { "STOPPED", "RUNNING", "BUS_OFF", "RECOVERING" };
    twai_status_info_t info;
    can_bus_stats_t st;

    can_bus_get_stats(&st);
    if (twai_get_status_info(&info) != ESP_OK)
    {
        printf("TWAI driver not running\n");
        return;
    }
    printf("TWAI   : %s, mode %s, TEC %" PRIu32 ", REC %" PRIu32 ", to_tx %" PRIu32 "\n",
           ((uint32_t)info.state < 4U) ? state_name[info.state] : "?",
           (s_mode == CAN_BUS_MODE_NO_ACK) ? "noack" : "normal",
           info.tx_error_counter, info.rx_error_counter, info.msgs_to_tx);
    printf("errors : tx_failed %" PRIu32 ", bus_err %" PRIu32 ", arb_lost %" PRIu32
           ", rx_missed %" PRIu32 "\n",
           info.tx_failed_count, info.bus_error_count, info.arb_lost_count, info.rx_missed_count);
    printf("frames : tx_ok %" PRIu32 ", tx_queue_full %" PRIu32 ", tx_refused %" PRIu32
           ", rx %" PRIu32 ", bus_off %" PRIu32 "\n",
           st.tx_ok, st.tx_queue_full, st.tx_bus_off, st.rx_frames, st.bus_off_events);
}
