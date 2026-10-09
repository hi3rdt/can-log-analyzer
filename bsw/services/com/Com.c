/**
 * @file    Com.c
 * @brief   AUTOSAR COM implementation (signal access, immediate I-PDU send).
 *
 * Concurrency: Com_RxIndication runs in the CAN RX ISR and only copies bytes
 * into the I-PDU buffer. Com_ReceiveSignal/Com_SendSignal run in task context
 * and do the bit work. Both sides touch the buffer inside
 * SchM_Enter/Exit_Com_EXCLUSIVE_AREA_0 so a signal is never read half-updated.
 *
 * Not yet implemented (milestone 4): periodic TX (Com_MainFunctionTx),
 * reception deadline monitoring (Com_MainFunctionRx), signal gateway.
 */
#include "Com.h"
#include "Com_Cbk.h"
#include "Com_Signal.h"
#include "PduR_Com.h"
#include "SchM.h"
#include "Det.h"

#define COM_MAX_PDU_LENGTH      8U

static const Com_ConfigType *Com_ConfigPtr = NULL_PTR;

#if (COM_DEV_ERROR_DETECT == STD_ON)
#define COM_DET_REPORT(sid, err) \
    ((void)Det_ReportError(DET_MODULE_ID_COM, 0U, (sid), (err)))
#else
#define COM_DET_REPORT(sid, err)
#endif

static uint32 Com_ReadApplicationValue(Com_SignalTypeType type, const void *dataPtr)
{
    uint32 raw;

    /* MISRA 11.5 deviation: the void* API is mandated by SWS_Com_00197 and the
     * pointed-to type is fixed by the generated signal configuration. */
    switch (type)
    {
        case COM_BOOLEAN: raw = (*(const boolean *)dataPtr != FALSE) ? 1UL : 0UL; break;
        case COM_UINT8:   raw = (uint32)*(const uint8 *)dataPtr;                break;
        case COM_UINT16:  raw = (uint32)*(const uint16 *)dataPtr;               break;
        case COM_UINT32:  raw = *(const uint32 *)dataPtr;                       break;
        case COM_SINT8:   raw = (uint32)(sint32)*(const sint8 *)dataPtr;        break;
        case COM_SINT16:  raw = (uint32)(sint32)*(const sint16 *)dataPtr;       break;
        case COM_SINT32:  raw = (uint32)*(const sint32 *)dataPtr;               break;
        default:          raw = 0UL;                                            break;
    }

    return raw;
}

static void Com_WriteApplicationValue(Com_SignalTypeType type, uint8 bitSize,
                                      uint32 raw, void *dataPtr)
{
    switch (type)
    {
        case COM_BOOLEAN: *(boolean *)dataPtr = (raw != 0UL) ? TRUE : FALSE;                    break;
        case COM_UINT8:   *(uint8 *)dataPtr   = (uint8)raw;                                     break;
        case COM_UINT16:  *(uint16 *)dataPtr  = (uint16)raw;                                    break;
        case COM_UINT32:  *(uint32 *)dataPtr  = raw;                                            break;
        case COM_SINT8:   *(sint8 *)dataPtr   = (sint8)Com_SignalSignExtend(raw, bitSize);      break;
        case COM_SINT16:  *(sint16 *)dataPtr  = (sint16)Com_SignalSignExtend(raw, bitSize);     break;
        case COM_SINT32:  *(sint32 *)dataPtr  = Com_SignalSignExtend(raw, bitSize);             break;
        default:          /* Unreachable with generated configuration */                        break;
    }
}

void Com_Init(const Com_ConfigType *config)
{
    if (config == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_INIT, COM_E_PARAM_POINTER);
    }
    else
    {
        PduIdType        pduId;
        Com_SignalIdType sigId;

        for (pduId = 0U; pduId < config->numIPdus; pduId++)
        {
            const Com_IPduConfigType *ipdu = &config->ipdus[pduId];
            PduLengthType byteIdx;

            for (byteIdx = 0U; byteIdx < ipdu->length; byteIdx++)
            {
                ipdu->buffer[byteIdx] = 0U;   /* Unused bits are sent as 0 */
            }
        }

        for (sigId = 0U; sigId < config->numSignals; sigId++)
        {
            const Com_SignalConfigType *sig = &config->signals[sigId];

            Com_SignalPack(config->ipdus[sig->ipduId].buffer, sig->bitPosition,
                           sig->bitSize, sig->endianness, sig->initValue);
        }

        Com_ConfigPtr = config;
    }
}

uint8 Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr)
{
    uint8 ret = COM_SERVICE_NOT_AVAILABLE;

    if (Com_ConfigPtr == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_SENDSIGNAL, COM_E_UNINIT);
    }
    else if (SignalId >= Com_ConfigPtr->numSignals)
    {
        COM_DET_REPORT(COM_SID_SENDSIGNAL, COM_E_PARAM);
    }
    else if (SignalDataPtr == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_SENDSIGNAL, COM_E_PARAM_POINTER);
    }
    else
    {
        const Com_SignalConfigType *sig  = &Com_ConfigPtr->signals[SignalId];
        const Com_IPduConfigType   *ipdu = &Com_ConfigPtr->ipdus[sig->ipduId];

        if (ipdu->direction != COM_SEND)
        {
            COM_DET_REPORT(COM_SID_SENDSIGNAL, COM_E_PARAM);
        }
        else
        {
            uint32 raw = Com_ReadApplicationValue(sig->signalType, SignalDataPtr);

            SchM_Enter_Com_EXCLUSIVE_AREA_0();
            Com_SignalPack(ipdu->buffer, sig->bitPosition, sig->bitSize, sig->endianness, raw);
            SchM_Exit_Com_EXCLUSIVE_AREA_0();

            ret = E_OK;
        }
    }

    return ret;
}

uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalDataPtr)
{
    uint8 ret = COM_SERVICE_NOT_AVAILABLE;

    if (Com_ConfigPtr == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_RECEIVESIGNAL, COM_E_UNINIT);
    }
    else if (SignalId >= Com_ConfigPtr->numSignals)
    {
        COM_DET_REPORT(COM_SID_RECEIVESIGNAL, COM_E_PARAM);
    }
    else if (SignalDataPtr == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_RECEIVESIGNAL, COM_E_PARAM_POINTER);
    }
    else
    {
        const Com_SignalConfigType *sig  = &Com_ConfigPtr->signals[SignalId];
        const Com_IPduConfigType   *ipdu = &Com_ConfigPtr->ipdus[sig->ipduId];
        uint32 raw;

        /* Reading a TX signal back is allowed and returns the last sent value */
        SchM_Enter_Com_EXCLUSIVE_AREA_0();
        raw = Com_SignalUnpack(ipdu->buffer, sig->bitPosition, sig->bitSize, sig->endianness);
        SchM_Exit_Com_EXCLUSIVE_AREA_0();

        Com_WriteApplicationValue(sig->signalType, sig->bitSize, raw, SignalDataPtr);
        ret = E_OK;
    }

    return ret;
}

Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    Std_ReturnType ret = E_NOT_OK;

    if (Com_ConfigPtr == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_TRIGGERIPDUSEND, COM_E_UNINIT);
    }
    else if ((PduId >= Com_ConfigPtr->numIPdus) ||
             (Com_ConfigPtr->ipdus[PduId].direction != COM_SEND))
    {
        COM_DET_REPORT(COM_SID_TRIGGERIPDUSEND, COM_E_PARAM);
    }
    else
    {
        const Com_IPduConfigType *ipdu = &Com_ConfigPtr->ipdus[PduId];
        uint8         snapshot[COM_MAX_PDU_LENGTH];
        PduInfoType   pduInfo;
        PduLengthType i;

        /* Copy under lock, transmit outside it: Can_Write must not run with
         * IRQs masked longer than needed. */
        SchM_Enter_Com_EXCLUSIVE_AREA_0();
        for (i = 0U; i < ipdu->length; i++)
        {
            snapshot[i] = ipdu->buffer[i];
        }
        SchM_Exit_Com_EXCLUSIVE_AREA_0();

        pduInfo.SduDataPtr  = snapshot;
        pduInfo.MetaDataPtr = NULL_PTR;
        pduInfo.SduLength   = ipdu->length;

        ret = PduR_ComTransmit(ipdu->pduRTxPduId, &pduInfo);
    }

    return ret;
}

void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    if (Com_ConfigPtr == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_RXINDICATION, COM_E_UNINIT);
    }
    else if ((RxPduId >= Com_ConfigPtr->numIPdus) ||
             (Com_ConfigPtr->ipdus[RxPduId].direction != COM_RECEIVE))
    {
        COM_DET_REPORT(COM_SID_RXINDICATION, COM_E_PARAM);
    }
    else if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        COM_DET_REPORT(COM_SID_RXINDICATION, COM_E_PARAM_POINTER);
    }
    else
    {
        const Com_IPduConfigType *ipdu = &Com_ConfigPtr->ipdus[RxPduId];
        PduLengthType copyLen = (PduInfoPtr->SduLength < ipdu->length) ?
                                PduInfoPtr->SduLength : ipdu->length;
        PduLengthType i;

        SchM_Enter_Com_EXCLUSIVE_AREA_0();
        for (i = 0U; i < copyLen; i++)
        {
            ipdu->buffer[i] = PduInfoPtr->SduDataPtr[i];
        }
        SchM_Exit_Com_EXCLUSIVE_AREA_0();
    }
}

void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    if (Com_ConfigPtr == NULL_PTR)
    {
        COM_DET_REPORT(COM_SID_TXCONFIRMATION, COM_E_UNINIT);
    }
    else if (TxPduId >= Com_ConfigPtr->numIPdus)
    {
        COM_DET_REPORT(COM_SID_TXCONFIRMATION, COM_E_PARAM);
    }
    else
    {
        /* Used by TX deadline monitoring in milestone 4 */
        (void)result;
    }
}
