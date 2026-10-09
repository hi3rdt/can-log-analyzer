/**
 * @file    PduR.c
 * @brief   PDU Router implementation. Pure table lookups, no buffering:
 *          every call is forwarded synchronously in the caller's context.
 */
#include "PduR.h"
#include "PduR_CanIf.h"
#include "PduR_Com.h"
#include "CanIf.h"
#include "Com_Cbk.h"
#include "Det.h"

static const PduR_PBConfigType *PduR_ConfigPtr = NULL_PTR;

#if (PDUR_DEV_ERROR_DETECT == STD_ON)
#define PDUR_DET_REPORT(sid, err) \
    ((void)Det_ReportError(DET_MODULE_ID_PDUR, 0U, (sid), (err)))
#else
#define PDUR_DET_REPORT(sid, err)
#endif

void PduR_Init(const PduR_PBConfigType *ConfigPtr)
{
    if (ConfigPtr == NULL_PTR)
    {
        PDUR_DET_REPORT(PDUR_SID_INIT, PDUR_E_PARAM_POINTER);
    }
    else
    {
        PduR_ConfigPtr = ConfigPtr;
    }
}

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL_PTR)
    {
        PDUR_DET_REPORT(PDUR_SID_RXINDICATION, PDUR_E_UNINIT);
    }
    else if (RxPduId >= PduR_ConfigPtr->numRxRoutes)
    {
        PDUR_DET_REPORT(PDUR_SID_RXINDICATION, PDUR_E_PDU_ID_INVALID);
    }
    else if (PduInfoPtr == NULL_PTR)
    {
        PDUR_DET_REPORT(PDUR_SID_RXINDICATION, PDUR_E_PARAM_POINTER);
    }
    else
    {
        const PduR_RxRouteType *route = &PduR_ConfigPtr->rxRoutes[RxPduId];
        uint8 i;

        for (i = 0U; i < route->numDests; i++)
        {
            const PduR_DestPduType *dest = &route->dests[i];

            if (dest->module == PDUR_DEST_COM)
            {
                Com_RxIndication(dest->pduId, PduInfoPtr);
            }
            else
            {
                /* Gateway: a busy mailbox drops this instance of the PDU; the
                 * next periodic frame from the source ECU replaces it. */
                (void)CanIf_Transmit(dest->pduId, PduInfoPtr);
            }
        }
    }
}

Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    Std_ReturnType ret = E_NOT_OK;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        PDUR_DET_REPORT(PDUR_SID_TRANSMIT, PDUR_E_UNINIT);
    }
    else if (TxPduId >= PduR_ConfigPtr->numComTxRoutes)
    {
        PDUR_DET_REPORT(PDUR_SID_TRANSMIT, PDUR_E_PDU_ID_INVALID);
    }
    else
    {
        ret = CanIf_Transmit(PduR_ConfigPtr->comTxRoutes[TxPduId].canIfTxPduId, PduInfoPtr);
    }

    return ret;
}

void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    if (PduR_ConfigPtr == NULL_PTR)
    {
        PDUR_DET_REPORT(PDUR_SID_TXCONFIRMATION, PDUR_E_UNINIT);
    }
    else if (TxPduId >= PduR_ConfigPtr->numTxConfRoutes)
    {
        PDUR_DET_REPORT(PDUR_SID_TXCONFIRMATION, PDUR_E_PDU_ID_INVALID);
    }
    else if (PduR_ConfigPtr->txConfRoutes[TxPduId].toCom == TRUE)
    {
        Com_TxConfirmation(PduR_ConfigPtr->txConfRoutes[TxPduId].comPduId, result);
    }
    else
    {
        /* Gateway PDU: no upper layer to confirm */
    }
}
