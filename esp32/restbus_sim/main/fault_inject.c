/**
 * @file    fault_inject.c
 * @brief   Per-ID fault injection. Pure C, no ESP-IDF dependency (host-tested).
 */
#include "fault_inject.h"

#include <string.h>

static fault_rule_t *find_rule(fault_table_t *t, uint32_t id, bool extended)
{
    uint8_t i;

    for (i = 0U; i < FAULT_MAX_RULES; i++)
    {
        fault_rule_t *r = &t->rule[i];
        if ((r->kind != FAULT_NONE) && (r->id == id) && (r->extended == extended))
        {
            return r;
        }
    }
    return NULL;
}

void fault_inject_init(fault_table_t *t)
{
    memset(t, 0, sizeof(*t));
}

bool fault_inject_set(fault_table_t *t, uint32_t id, bool extended, fault_kind_t kind)
{
    fault_rule_t *r = find_rule(t, id, extended);
    uint8_t i;

    if (kind == FAULT_NONE)
    {
        return fault_inject_clear(t, id, extended);
    }
    for (i = 0U; (r == NULL) && (i < FAULT_MAX_RULES); i++)
    {
        if (t->rule[i].kind == FAULT_NONE)
        {
            r = &t->rule[i];
        }
    }
    if (r == NULL)
    {
        return false;
    }
    memset(r, 0, sizeof(*r));
    r->id       = id;
    r->extended = extended;
    r->kind     = kind;
    return true;
}

bool fault_inject_clear(fault_table_t *t, uint32_t id, bool extended)
{
    fault_rule_t *r = find_rule(t, id, extended);

    if (r == NULL)
    {
        return false;
    }
    memset(r, 0, sizeof(*r));
    return true;
}

void fault_inject_clear_all(fault_table_t *t)
{
    fault_inject_init(t);
}

uint8_t fault_inject_apply(fault_table_t *t, can_frame_t *frame)
{
    fault_rule_t *r = find_rule(t, frame->id, frame->extended);
    uint8_t copies = 1U;

    if (r == NULL)
    {
        return 1U;
    }

    switch (r->kind)
    {
    case FAULT_DROP:
        copies = 0U;
        break;

    case FAULT_CORRUPT:
        if (frame->dlc > 0U)
        {
            frame->data[frame->dlc - 1U] ^= 0x80U;
        }
        break;

    case FAULT_FREEZE:
        if (!r->frozen_valid)
        {
            memcpy(r->frozen_data, frame->data, sizeof(r->frozen_data));
            r->frozen_valid = true;
        }
        memcpy(frame->data, r->frozen_data, sizeof(frame->data));
        break;

    case FAULT_REPEAT:
        copies = 2U;
        break;

    case FAULT_NONE:
    default:
        break;
    }
    r->hits++;
    return copies;
}

const char *fault_kind_name(fault_kind_t kind)
{
    switch (kind)
    {
    case FAULT_DROP:    return "drop";
    case FAULT_CORRUPT: return "corrupt";
    case FAULT_FREEZE:  return "freeze";
    case FAULT_REPEAT:  return "repeat";
    case FAULT_NONE:
    default:            return "none";
    }
}
