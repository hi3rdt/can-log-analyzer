/**
 * @file    Det.h
 * @brief   Default Error Tracer (SWS_Det subset).
 *
 * BSW modules report development errors (wrong parameter, call before init, ...)
 * through Det_ReportError(). This implementation keeps the last error and a
 * counter in RAM so it can be inspected from a debugger, a host unit test or
 * forwarded to the PC over UART by a later module.
 */
#ifndef DET_H
#define DET_H

#include "Std_Types.h"

/* AUTOSAR module IDs used in this project (from the AUTOSAR BSW module list). */
#define DET_MODULE_ID_CAN       80U
#define DET_MODULE_ID_CANIF     60U
#define DET_MODULE_ID_PDUR      51U
#define DET_MODULE_ID_COM       50U
#define DET_MODULE_ID_DET       15U

typedef struct
{
    uint16 moduleId;
    uint8  instanceId;
    uint8  apiId;
    uint8  errorId;
} Det_ErrorType;

void           Det_Init(void);
Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);

/* Project extensions (not in SWS) for diagnostics and tests. */
uint32         Det_GetErrorCount(void);
Std_ReturnType Det_GetLastError(Det_ErrorType *ErrorPtr);

#endif /* DET_H */
