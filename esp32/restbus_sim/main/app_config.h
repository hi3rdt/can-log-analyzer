/**
 * @file    app_config.h
 * @brief   Build-time configuration of the rest-bus simulator (pins, bit rate, queues).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* TWAI pins to the CAN transceiver. Any free GPIO works; avoid strapping pins
 * (0, 2, 12, 15) and input-only pins (34..39) for TX.
 * If the transceiver is a 5 V part (TJA1050, MCP2551), its RXD output is 5 V:
 * put a divider (e.g. 2.2k / 3.3k) on RX, ESP32 GPIOs are NOT 5 V tolerant. */
#define APP_TWAI_TX_GPIO        5
#define APP_TWAI_RX_GPIO        4

#define APP_TWAI_TX_QUEUE_LEN   32U
#define APP_TWAI_RX_QUEUE_LEN   16U

/* Replay task: highest app priority, pinned to core 1 so Wi-Fi/console on core 0
 * do not add jitter to the frame timing. */
#define APP_REPLAY_TASK_PRIO    10U
#define APP_REPLAY_TASK_CORE    1
#define APP_REPLAY_STACK_BYTES  4096U

#define APP_MONITOR_TASK_PRIO   5U
#define APP_MONITOR_STACK_BYTES 3072U

#endif /* APP_CONFIG_H */
