/**
 * @file    SchM_CortexM.c
 * @brief   Nestable exclusive area for Cortex-M3 using PRIMASK.
 *          Keep exclusive areas short: they block every maskable IRQ.
 */
#include "SchM.h"

static uint32 SchM_SavedPrimask;
static uint32 SchM_NestCount;

void SchM_Enter_Exclusive(void)
{
    uint32 primask;

    /* Read the current mask, then disable IRQs; the "memory" clobber stops
     * the compiler from moving buffer accesses out of the protected region. */
    __asm__ volatile ("mrs %0, primask\n\tcpsid i" : "=r" (primask) : : "memory");

    if (SchM_NestCount == 0UL)
    {
        SchM_SavedPrimask = primask;
    }
    SchM_NestCount++;
}

void SchM_Exit_Exclusive(void)
{
    if (SchM_NestCount > 0UL)
    {
        SchM_NestCount--;
        if (SchM_NestCount == 0UL)
        {
            /* Restore instead of "cpsie i" so a caller that entered with IRQs
             * already masked (e.g. inside a fault handler) keeps them masked. */
            __asm__ volatile ("msr primask, %0" : : "r" (SchM_SavedPrimask) : "memory");
        }
    }
}
