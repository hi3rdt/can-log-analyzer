/**
 * @file    can_bus.h
 * @brief   TWAI (CAN) bring-up, transmit wrapper, RX logging and bus-off recovery.
 */
#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "can_frame.h"

typedef enum
{
    CAN_BUS_MODE_NORMAL = 0,    /* needs a second node (the STM32) to ACK frames       */
    CAN_BUS_MODE_NO_ACK         /* self-test: transmit without ACK, receive own frames */
} can_bus_mode_t;

typedef struct
{
    uint32_t tx_ok;             /* frames accepted into the TX queue                  */
    uint32_t tx_queue_full;     /* frames lost because the TX queue was full          */
    uint32_t tx_bus_off;        /* frames refused while bus-off / recovering           */
    uint32_t rx_frames;
    uint32_t bus_off_events;
} can_bus_stats_t;

/** Install and start the TWAI driver in the mode selected before the last reset. */
esp_err_t can_bus_init(void);

/** Queue one frame for transmission, never blocks. */
esp_err_t can_bus_send(const can_frame_t *frame);

can_bus_mode_t can_bus_mode(void);

/** Store the mode and reboot (driver reinstall while tasks block on it is not safe). */
void can_bus_set_mode_and_restart(can_bus_mode_t mode);

void can_bus_set_rx_log(bool on);

void can_bus_get_stats(can_bus_stats_t *out);

/** Print driver state, TEC/REC and counters to the console. */
void can_bus_print_status(void);

#endif /* CAN_BUS_H */
