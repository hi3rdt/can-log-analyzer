/**
 * @file    SchM.h
 * @brief   Exclusive areas for BSW modules (minimal BSW Scheduler).
 *
 * Com buffers are written from the CAN RX interrupt (Can -> CanIf -> PduR ->
 * Com_RxIndication) and read from task context (Com_ReceiveSignal), so every
 * access to shared PDU buffers is wrapped in an exclusive area.
 *
 * The target implementation masks interrupts (PRIMASK) and supports nesting;
 * the host test build links a stub that only counts enter/exit calls.
 */
#ifndef SCHM_H
#define SCHM_H

#include "Std_Types.h"

void SchM_Enter_Exclusive(void);
void SchM_Exit_Exclusive(void);

#define SchM_Enter_Com_EXCLUSIVE_AREA_0()     SchM_Enter_Exclusive()
#define SchM_Exit_Com_EXCLUSIVE_AREA_0()      SchM_Exit_Exclusive()

#endif /* SCHM_H */
