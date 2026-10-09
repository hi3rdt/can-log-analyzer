/**
 * @file    SchM_Stub.c
 * @brief   Host exclusive area: tracks nesting so tests can assert that every
 *          SchM_Enter has a matching SchM_Exit.
 */
#include "SchM.h"

sint32 SchM_Stub_Nesting = 0;
uint32 SchM_Stub_EnterCount = 0U;

void SchM_Enter_Exclusive(void)
{
    SchM_Stub_Nesting++;
    SchM_Stub_EnterCount++;
}

void SchM_Exit_Exclusive(void)
{
    SchM_Stub_Nesting--;
}
