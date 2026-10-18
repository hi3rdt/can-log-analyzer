/**
 * @file    main.c
 * @brief   ESP32 rest-bus simulator for Project 1 (docs/projects/p1-instrument-cluster.vi.md).
 *
 * Replays samples/demo_drive.csv on CAN at 500 kbit/s, standing in for the ABS,
 * ECM and BCM of a car so the STM32 cluster ECU can be tested on a desk, with
 * fault injection from the serial console.
 */
#include "esp_console.h"
#include "esp_log.h"

#include "can_bus.h"
#include "console_cmds.h"
#include "replay.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(can_bus_init());
    replay_start(true);

    repl_config.prompt = "restbus>";
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_register_help_command());
    console_cmds_register();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    ESP_LOGI(TAG, "ready, type 'help'");
}
