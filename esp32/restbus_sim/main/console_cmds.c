/**
 * @file    console_cmds.c
 * @brief   Serial console (idf.py monitor) commands to drive the simulator.
 *
 *   status                         replay + TWAI state, counters, active faults
 *   play | pause
 *   speed <percent>                25..1000, 100 = real time
 *   fault <id> <kind> [ext]        kind: drop | corrupt | freeze | repeat | none
 *   clear                          remove every fault
 *   silence <on|off>               stop sending everything (all ECUs dead)
 *   rxlog <on|off>                 print frames received from the bus
 *   mode <normal|noack>            reboot into normal or self-test (no ACK) mode
 */
#include "console_cmds.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"

#include "can_bus.h"
#include "replay.h"

static bool parse_on_off(const char *s, bool *out)
{
    if (strcmp(s, "on") == 0)  { *out = true;  return true; }
    if (strcmp(s, "off") == 0) { *out = false; return true; }
    return false;
}

static int cmd_status(int argc, char **argv)
{
    replay_stats_t st;

    (void)argc;
    (void)argv;
    replay_get_stats(&st);
    printf("replay : %s, speed %" PRIu32 " %%, silence %s, loops %" PRIu32 "\n",
           replay_is_playing() ? "playing" : "paused", replay_speed(),
           replay_silence() ? "on" : "off", st.loops);
    printf("         sent %" PRIu32 ", dropped %" PRIu32 ", parse_err %" PRIu32
           ", worst late %" PRIu32 " us\n",
           st.frames_sent, st.frames_dropped, st.parse_errors, st.late_max_us);
    replay_fault_print();
    can_bus_print_status();
    return 0;
}

static int cmd_play(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    replay_set_playing(true);
    return 0;
}

static int cmd_pause(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    replay_set_playing(false);
    return 0;
}

static int cmd_speed(int argc, char **argv)
{
    if ((argc != 2) || !replay_set_speed((uint32_t)strtoul(argv[1], NULL, 10)))
    {
        printf("usage: speed <25..1000>\n");
        return 1;
    }
    return 0;
}

static int cmd_fault(int argc, char **argv)
{
    static const struct { const char *name; fault_kind_t kind; } kinds[] =
    {
        { "drop", FAULT_DROP }, { "corrupt", FAULT_CORRUPT }, { "freeze", FAULT_FREEZE },
        { "repeat", FAULT_REPEAT }, { "none", FAULT_NONE },
    };
    char *endp = NULL;
    uint32_t id;
    bool ext;
    size_t i;

    if ((argc < 3) || (argc > 4))
    {
        printf("usage: fault <id hex> <drop|corrupt|freeze|repeat|none> [ext]\n");
        return 1;
    }
    id  = (uint32_t)strtoul(argv[1], &endp, 16);
    ext = (argc == 4) && (strcmp(argv[3], "ext") == 0);
    if ((endp == argv[1]) || (*endp != '\0') || (id > (ext ? 0x1FFFFFFFUL : 0x7FFUL)))
    {
        printf("bad id '%s'\n", argv[1]);
        return 1;
    }
    for (i = 0U; i < (sizeof(kinds) / sizeof(kinds[0])); i++)
    {
        if (strcmp(argv[2], kinds[i].name) == 0)
        {
            if (!replay_fault_set(id, ext, kinds[i].kind) && (kinds[i].kind != FAULT_NONE))
            {
                printf("fault table full\n");
                return 1;
            }
            return 0;
        }
    }
    printf("unknown fault '%s'\n", argv[2]);
    return 1;
}

static int cmd_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    replay_fault_clear_all();
    return 0;
}

static int cmd_silence(int argc, char **argv)
{
    bool on;

    if ((argc != 2) || !parse_on_off(argv[1], &on))
    {
        printf("usage: silence <on|off>\n");
        return 1;
    }
    replay_set_silence(on);
    return 0;
}

static int cmd_rxlog(int argc, char **argv)
{
    bool on;

    if ((argc != 2) || !parse_on_off(argv[1], &on))
    {
        printf("usage: rxlog <on|off>\n");
        return 1;
    }
    can_bus_set_rx_log(on);
    return 0;
}

static int cmd_mode(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "normal") == 0))
    {
        can_bus_set_mode_and_restart(CAN_BUS_MODE_NORMAL);
    }
    else if ((argc == 2) && (strcmp(argv[1], "noack") == 0))
    {
        can_bus_set_mode_and_restart(CAN_BUS_MODE_NO_ACK);
    }
    else
    {
        printf("usage: mode <normal|noack>\n");
        return 1;
    }
    return 0;
}

void console_cmds_register(void)
{
    static const esp_console_cmd_t cmds[] =
    {
        { .command = "status",  .help = "Replay and CAN status",                   .func = cmd_status  },
        { .command = "play",    .help = "Start/resume the log replay",             .func = cmd_play    },
        { .command = "pause",   .help = "Pause the log replay",                    .func = cmd_pause   },
        { .command = "speed",   .help = "speed <25..1000>: replay speed in %",     .func = cmd_speed   },
        { .command = "fault",   .help = "fault <id> <drop|corrupt|freeze|repeat|none> [ext]", .func = cmd_fault },
        { .command = "clear",   .help = "Remove all faults",                       .func = cmd_clear   },
        { .command = "silence", .help = "silence <on|off>: stop all frames",       .func = cmd_silence },
        { .command = "rxlog",   .help = "rxlog <on|off>: print received frames",   .func = cmd_rxlog   },
        { .command = "mode",    .help = "mode <normal|noack>: reboot in that mode", .func = cmd_mode   },
    };

    for (size_t i = 0U; i < (sizeof(cmds) / sizeof(cmds[0])); i++)
    {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}
