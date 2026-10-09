/**
 * @file    CanIf.c
 * @brief   CAN Interface implementation.
 *
 * RX: binary search of the received CAN ID in the sorted RX table (software
 *     filter), DLC check, then PduR_CanIfRxIndication. Unknown IDs are dropped
 *     silently because hardware filters may be configured to accept all.
 * TX: map TxPduId -> CAN ID/HTH and call Can_Write. There is no TX buffer in
 *     this version: CAN_BUSY is reported to the caller as E_NOT_OK.
 */
#include "CanIf.h"
#include "CanIf_Cbk.h"
#include "Can.h"
#include "PduR_CanIf.h"
#include "Det.h"

static const CanIf_ConfigType *CanIf_ConfigPtr = NULL_PTR;

#if (CANIF_DEV_ERROR_DETECT == STD_ON)
#define CANIF_DET_REPORT(sid, err) \
    ((void)Det_ReportError(DET_MODULE_ID_CANIF, 0U, (sid), (err)))
#else
#define CANIF_DET_REPORT(sid, err)
#endif

/** @return index into rxPdus, or numRxPdus if the ID is not configured. */
static PduIdType CanIf_FindRxPdu(Can_IdType canId)
{
    const CanIf_RxPduConfigType *table = CanIf_ConfigPtr->rxPdus;
    PduIdType low   = 0U;
    PduIdType high  = CanIf_ConfigPtr->numRxPdus;   /* exclusive */
    PduIdType found = CanIf_ConfigPtr->numRxPdus;

    while ((low < high) && (found == CanIf_ConfigPtr->numRxPdus))
    {
        PduIdType mid = (PduIdType)(low + ((PduIdType)(high - low) / 2U));

        if (table[mid].canId == canId)
        {
            found = mid;
        }
        else if (table[mid].canId < canId)
        {
            low = (PduIdType)(mid + 1U);
        }
        else
        {
            high = mid;
        }
    }

    return found;
}

void CanIf_Init(const CanIf_ConfigType *ConfigPtr)
{
    if (ConfigPtr == NULL_PTR)
    {
        CANIF_DET_REPORT(CANIF_SID_INIT, CANIF_E_PARAM_POINTER);
    }
    else
    {
        CanIf_ConfigPtr = ConfigPtr;
    }
}

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
{
    if (CanIf_ConfigPtr == NULL_PTR)
    {
        CANIF_DET_REPORT(CANIF_SID_RXINDICATION, CANIF_E_UNINIT);
    }
    else if ((Mailbox == NULL_PTR) || (PduInfoPtr == NULL_PTR) ||
             (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        CANIF_DET_REPORT(CANIF_SID_RXINDICATION, CANIF_E_PARAM_POINTER);
    }
    else
    {
        PduIdType idx = CanIf_FindRxPdu(Mailbox->CanId);

        if (idx < CanIf_ConfigPtr->numRxPdus)
        {
            const CanIf_RxPduConfigType *cfg = &CanIf_ConfigPtr->rxPdus[idx];

            /* DLC check: a shorter frame would leave stale bytes in Com buffers */
            if (PduInfoPtr->SduLength < (PduLengthType)cfg->dlc)
            {
                CANIF_DET_REPORT(CANIF_SID_RXINDICATION, CANIF_E_INVALID_DLC);
            }
            else
            {
                PduR_CanIfRxIndication(cfg->upperPduId, PduInfoPtr);
            }
        }
    }
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    Std_ReturnType ret = E_NOT_OK;

    if (CanIf_ConfigPtr == NULL_PTR)
    {
        CANIF_DET_REPORT(CANIF_SID_TRANSMIT, CANIF_E_UNINIT);
    }
    else if (TxPduId >= CanIf_ConfigPtr->numTxPdus)
    {
        CANIF_DET_REPORT(CANIF_SID_TRANSMIT, CANIF_E_INVALID_TXPDUID);
    }
    else if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        CANIF_DET_REPORT(CANIF_SID_TRANSMIT, CANIF_E_PARAM_POINTER);
    }
    else if (PduInfoPtr->SduLength > 8U)
    {
        CANIF_DET_REPORT(CANIF_SID_TRANSMIT, CANIF_E_DATA_LENGTH_MISMATCH);
    }
    else
    {
        const CanIf_TxPduConfigType *cfg = &CanIf_ConfigPtr->txPdus[TxPduId];
        Can_PduType canPdu;

        canPdu.swPduHandle = TxPduId;
        canPdu.id          = cfg->canId;
        canPdu.length      = (uint8)PduInfoPtr->SduLength;
        canPdu.sdu         = PduInfoPtr->SduDataPtr;

        ret = (Can_Write(cfg->hth, &canPdu) == E_OK) ? E_OK : E_NOT_OK;
    }

    return ret;
}

void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    if (CanIf_ConfigPtr == NULL_PTR)
    {
        CANIF_DET_REPORT(CANIF_SID_TXCONFIRMATION, CANIF_E_UNINIT);
    }
    else if (CanTxPduId >= CanIf_ConfigPtr->numTxPdus)
    {
        CANIF_DET_REPORT(CANIF_SID_TXCONFIRMATION, CANIF_E_INVALID_TXPDUID);
    }
    else
    {
        PduR_CanIfTxConfirmation(CanIf_ConfigPtr->txPdus[CanTxPduId].upperPduId, E_OK);
    }
}
