/**
 * @file    test_det.c
 * @brief   Host unit tests for the Default Error Tracer.
 */
#include "unit_test.h"
#include "Det.h"

static void test_init_clears_state(void)
{
    Det_ErrorType err;

    (void)Det_ReportError(DET_MODULE_ID_COM, 0U, 1U, 2U);
    Det_Init();

    UT_CHECK_EQ(0U, Det_GetErrorCount());
    UT_CHECK_EQ(E_NOT_OK, Det_GetLastError(&err));
}

static void test_report_stores_last_error(void)
{
    Det_ErrorType err;

    Det_Init();
    UT_CHECK_EQ(E_OK, Det_ReportError(DET_MODULE_ID_CANIF, 0U, 0x05U, 0x10U));
    UT_CHECK_EQ(E_OK, Det_ReportError(DET_MODULE_ID_PDUR, 1U, 0x07U, 0x20U));

    UT_CHECK_EQ(2U, Det_GetErrorCount());
    UT_CHECK_EQ(E_OK, Det_GetLastError(&err));
    UT_CHECK_EQ(DET_MODULE_ID_PDUR, err.moduleId);
    UT_CHECK_EQ(1U, err.instanceId);
    UT_CHECK_EQ(0x07U, err.apiId);
    UT_CHECK_EQ(0x20U, err.errorId);
}

static void test_get_last_error_null_ptr(void)
{
    Det_Init();
    (void)Det_ReportError(DET_MODULE_ID_CAN, 0U, 0U, 1U);
    UT_CHECK_EQ(E_NOT_OK, Det_GetLastError(NULL_PTR));
}

int main(void)
{
    UT_RUN(test_init_clears_state);
    UT_RUN(test_report_stores_last_error);
    UT_RUN(test_get_last_error_null_ptr);
    return UT_REPORT();
}
