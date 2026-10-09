/**
 * @file    Det.c
 * @brief   Default Error Tracer implementation (static RAM storage only).
 */
#include "Det.h"

static Det_ErrorType Det_LastError;
static uint32        Det_ErrorCount;

void Det_Init(void)
{
    Det_LastError.moduleId   = 0U;
    Det_LastError.instanceId = 0U;
    Det_LastError.apiId      = 0U;
    Det_LastError.errorId    = 0U;
    Det_ErrorCount           = 0U;
}

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    Det_LastError.moduleId   = ModuleId;
    Det_LastError.instanceId = InstanceId;
    Det_LastError.apiId      = ApiId;
    Det_LastError.errorId    = ErrorId;

    /* Saturate instead of wrapping so a flood of errors is never reported as 0 */
    if (Det_ErrorCount < 0xFFFFFFFFUL)
    {
        Det_ErrorCount++;
    }

    /* SWS_Det_00026: Det_ReportError always returns E_OK */
    return E_OK;
}

uint32 Det_GetErrorCount(void)
{
    return Det_ErrorCount;
}

Std_ReturnType Det_GetLastError(Det_ErrorType *ErrorPtr)
{
    Std_ReturnType ret = E_NOT_OK;

    if ((ErrorPtr != NULL_PTR) && (Det_ErrorCount != 0U))
    {
        *ErrorPtr = Det_LastError;
        ret = E_OK;
    }

    return ret;
}
