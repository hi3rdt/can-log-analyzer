/**
 * @file    test_comstack.c
 * @brief   Integration tests for CanIf -> PduR -> Com with the configuration
 *          generated from config/gateway.dbc, cross-checked against cantools.
 */
#include <string.h>
#include "unit_test.h"
#include "CanIf.h"
#include "CanIf_Cbk.h"
#include "CanIf_Cfg.h"
#include "PduR.h"
#include "PduR_Cfg.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "Det.h"
#include "Can_Stub.h"
#include "test_vectors.h"

extern sint32 SchM_Stub_Nesting;
extern uint32 SchM_Stub_EnterCount;

#define ARRAY_LEN(a)  (sizeof(a) / sizeof((a)[0]))

static void Stack_Init(void)
{
    Det_Init();
    CanStub_Reset();
    Com_Init(&Com_Config);
    PduR_Init(&PduR_Config);
    CanIf_Init(&CanIf_Config);
}

static void Rx_Frame(Can_IdType id, uint8 dlc, const uint8 *data)
{
    uint8       buf[8];
    Can_HwType  mailbox;
    PduInfoType info;

    memcpy(buf, data, 8U);
    mailbox.CanId        = id;
    mailbox.Hoh          = 0U;
    mailbox.ControllerId = 0U;
    info.SduDataPtr      = buf;
    info.MetaDataPtr     = NULL_PTR;
    info.SduLength       = dlc;
    CanIf_RxIndication(&mailbox, &info);
}

/** Read any signal type into a 64-bit value for comparison. */
static long long Read_Signal(Com_SignalIdType id, uint8 *status)
{
    union { boolean b; uint8 u8; uint16 u16; uint32 u32; sint8 s8; sint16 s16; sint32 s32; } v;
    long long out = 0;

    memset(&v, 0, sizeof(v));
    *status = Com_ReceiveSignal(id, &v);
    switch (Com_Config.signals[id].signalType)
    {
        case COM_BOOLEAN: out = v.b;   break;
        case COM_UINT8:   out = v.u8;  break;
        case COM_UINT16:  out = v.u16; break;
        case COM_UINT32:  out = v.u32; break;
        case COM_SINT8:   out = v.s8;  break;
        case COM_SINT16:  out = v.s16; break;
        case COM_SINT32:  out = v.s32; break;
        default:                       break;
    }
    return out;
}

static uint8 Write_Signal(Com_SignalIdType id, long long raw)
{
    union { boolean b; uint8 u8; uint16 u16; uint32 u32; sint8 s8; sint16 s16; sint32 s32; } v;

    switch (Com_Config.signals[id].signalType)
    {
        case COM_BOOLEAN: v.b   = (boolean)raw; break;
        case COM_UINT8:   v.u8  = (uint8)raw;   break;
        case COM_UINT16:  v.u16 = (uint16)raw;  break;
        case COM_UINT32:  v.u32 = (uint32)raw;  break;
        case COM_SINT8:   v.s8  = (sint8)raw;   break;
        case COM_SINT16:  v.s16 = (sint16)raw;  break;
        case COM_SINT32:  v.s32 = (sint32)raw;  break;
        default:          v.u32 = 0U;           break;
    }
    return Com_SendSignal(id, &v);
}

static void test_rx_matches_cantools(void)
{
    size_t i;
    int mismatches = 0;

    Stack_Init();
    for (i = 0U; i < ARRAY_LEN(rx_vectors); i++)
    {
        const RxVector *vec = &rx_vectors[i];
        uint8 status;
        long long got;

        Rx_Frame(vec->canId, vec->dlc, vec->data);
        got = Read_Signal(vec->signalId, &status);
        if ((status != E_OK) || (got != vec->expected))
        {
            if (mismatches < 5)
            {
                printf("  rx vector %zu: signal %u expected %lld got %lld\n",
                       i, (unsigned)vec->signalId, vec->expected, got);
            }
            mismatches++;
        }
    }
    UT_CHECK_EQ(0, mismatches);
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_tx_matches_cantools(void)
{
    size_t i;
    uint8 j;
    int mismatches = 0;

    Stack_Init();
    for (i = 0U; i < ARRAY_LEN(tx_vectors); i++)
    {
        const TxVector *vec = &tx_vectors[i];
        const CanStub_FrameType *frame;

        CanStub_Reset();
        for (j = 0U; j < vec->numSignals; j++)
        {
            UT_CHECK_EQ(E_OK, Write_Signal(vec->signals[j].signalId, vec->signals[j].raw));
        }
        UT_CHECK_EQ(E_OK, Com_TriggerIPDUSend(vec->pduId));
        UT_CHECK_EQ(1U, CanStub_GetFrameCount());

        frame = CanStub_GetFrame(0U);
        if ((frame == NULL_PTR) || (frame->id != vec->canId) || (frame->length != vec->dlc) ||
            (memcmp(frame->data, vec->expected, vec->dlc) != 0))
        {
            mismatches++;
        }
    }
    UT_CHECK_EQ(0, mismatches);
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_init_values(void)
{
    uint8 status;

    Stack_Init();
    /* CoolantTemp: physical 0 degC with offset -40 -> raw 40 */
    UT_CHECK_EQ(40, Read_Signal(ComConf_ComSignal_CoolantTemp, &status));
    UT_CHECK_EQ(0, Read_Signal(ComConf_ComSignal_VehicleSpeed, &status));
}

static void test_pdu_gateway_forwards_body_status(void)
{
    const uint8 data[8] = { 0x05U, 0x00U, 0x9CU, 0xFFU, 0U, 0U, 0U, 0U };
    const CanStub_FrameType *frame;
    uint8 status;

    Stack_Init();
    Rx_Frame(0x300UL, 4U, data);

    /* Fan-out: Com sees the signals ... */
    UT_CHECK_EQ(1, Read_Signal(ComConf_ComSignal_DoorFL, &status));
    UT_CHECK_EQ(0, Read_Signal(ComConf_ComSignal_DoorFR, &status));
    UT_CHECK_EQ(1, Read_Signal(ComConf_ComSignal_DoorRL, &status));
    UT_CHECK_EQ(-100, Read_Signal(ComConf_ComSignal_SteeringAngle, &status));

    /* ... and the same payload leaves unchanged on 0x500 */
    UT_CHECK_EQ(1U, CanStub_GetFrameCount());
    frame = CanStub_GetFrame(0U);
    UT_CHECK(frame != NULL_PTR);
    if (frame != NULL_PTR)
    {
        UT_CHECK_EQ(0x500U, frame->id);
        UT_CHECK_EQ(4U, frame->length);
        UT_CHECK_EQ(CanIfConf_TxPdu_GW_BodyStatusFwd, frame->swPduHandle);
        UT_CHECK(memcmp(frame->data, data, 4U) == 0);
    }
}

static void test_short_dlc_rejected(void)
{
    const uint8 good[8] = { 0x10U, 0x27U, 0U, 0U, 0U, 0U, 0U, 0U };
    const uint8 bad[8]  = { 0xFFU, 0xFFU, 0U, 0U, 0U, 0U, 0U, 0U };
    Det_ErrorType err;
    uint8 status;

    Stack_Init();
    Rx_Frame(0x100UL, 8U, good);
    Rx_Frame(0x100UL, 2U, bad);      /* DLC 2 < configured 8 */

    UT_CHECK_EQ(10000, Read_Signal(ComConf_ComSignal_VehicleSpeed, &status));
    UT_CHECK_EQ(E_OK, Det_GetLastError(&err));
    UT_CHECK_EQ(DET_MODULE_ID_CANIF, err.moduleId);
    UT_CHECK_EQ(CANIF_E_INVALID_DLC, err.errorId);
}

static void test_unknown_id_dropped(void)
{
    const uint8 data[8] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };

    Stack_Init();
    Rx_Frame(0x101UL, 8U, data);
    Rx_Frame(0x18FEEF00UL, 8U, data);   /* right number, but standard frame */
    Rx_Frame(0xFFFFFFFFUL, 8U, data);

    UT_CHECK_EQ(0U, Det_GetErrorCount());
    UT_CHECK_EQ(0U, CanStub_GetFrameCount());
}

static void test_extended_id_received(void)
{
    const uint8 data[8] = { 0x40U, 0xE2U, 0x01U, 0xF6U, 0U, 0U, 0U, 0U };
    uint8 status;

    Stack_Init();
    Rx_Frame(0x18FEEF00UL | CAN_ID_EXTENDED_FLAG, 8U, data);
    UT_CHECK_EQ(123456, Read_Signal(ComConf_ComSignal_OdometerKm, &status));
    UT_CHECK_EQ(-10, Read_Signal(ComConf_ComSignal_AmbientTemp, &status));
}

static void test_api_errors(void)
{
    uint16 value = 1U;
    Det_ErrorType err;

    Stack_Init();
    /* Writing an RX signal is a configuration misuse */
    UT_CHECK_EQ(COM_SERVICE_NOT_AVAILABLE, Com_SendSignal(ComConf_ComSignal_VehicleSpeed, &value));
    UT_CHECK_EQ(COM_SERVICE_NOT_AVAILABLE, Com_ReceiveSignal(COM_NUM_SIGNALS, &value));
    UT_CHECK_EQ(COM_SERVICE_NOT_AVAILABLE, Com_ReceiveSignal(0U, NULL_PTR));
    UT_CHECK_EQ(E_NOT_OK, Com_TriggerIPDUSend(ComConf_ComIPdu_VehicleSpeed));
    UT_CHECK_EQ(E_OK, Det_GetLastError(&err));
    UT_CHECK_EQ(DET_MODULE_ID_COM, err.moduleId);
    UT_CHECK_EQ(4U, Det_GetErrorCount());
}

static void test_can_busy_reported(void)
{
    Stack_Init();
    CanStub_SetReturn(CAN_BUSY);
    UT_CHECK_EQ(E_NOT_OK, Com_TriggerIPDUSend(ComConf_ComIPdu_GW_VehicleState));
}

static void test_exclusive_areas_balanced(void)
{
    UT_CHECK_EQ(0, SchM_Stub_Nesting);
    UT_CHECK(SchM_Stub_EnterCount > 0U);
}

int main(void)
{
    UT_RUN(test_rx_matches_cantools);
    UT_RUN(test_tx_matches_cantools);
    UT_RUN(test_init_values);
    UT_RUN(test_pdu_gateway_forwards_body_status);
    UT_RUN(test_short_dlc_rejected);
    UT_RUN(test_unknown_id_dropped);
    UT_RUN(test_extended_id_received);
    UT_RUN(test_api_errors);
    UT_RUN(test_can_busy_reported);
    UT_RUN(test_exclusive_areas_balanced);
    return UT_REPORT();
}
