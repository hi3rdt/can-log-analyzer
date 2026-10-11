/**
 * @file    SchM.h
 * @brief   Exclusive areas for BSW modules (minimal BSW Scheduler).
 *
 * Data shared between an interrupt and task context (for example the Can
 * driver's TX mailbox bookkeeping, written by Can_Write and read by the TX
 * ISR) is accessed inside an exclusive area.
 *
 * The target implementation masks interrupts (PRIMASK) and supports nesting;
 * the host test build links a stub that only counts enter/exit calls.
 */
#ifndef SCHM_H
#define SCHM_H

#include "Std_Types.h"

void SchM_Enter_Exclusive(void);
void SchM_Exit_Exclusive(void);

#endif /* SCHM_H */
