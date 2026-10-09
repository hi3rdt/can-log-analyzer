/*
 * GENERATED FILE - DO NOT EDIT.
 * Source: config/gateway.dbc (node GW), config/routes.json
 * Generator: tools/dbc2c.py
 */

#include "CanIf_Cfg.h"

/* Sorted by CAN ID for binary search; upperPduId = PduR RX route */
static const CanIf_RxPduConfigType CanIf_RxPdus[CANIF_NUM_RX_PDUS] =
{
    { 0x100UL, 8U, 0U },  /* VehicleSpeed */
    { 0x200UL, 8U, 1U },  /* EngineData */
    { 0x300UL, 4U, 2U },  /* BodyStatus */
    { (0x18FEEF00UL | CAN_ID_EXTENDED_FLAG), 8U, 3U },  /* DiagExtended */
};

/* Single HTH 0: the bxCAN driver owns all three TX mailboxes */
static const CanIf_TxPduConfigType CanIf_TxPdus[CANIF_NUM_TX_PDUS] =
{
    { 0x400UL, 8U, 0U, 0U },  /* GW_VehicleState */
    { 0x500UL, 4U, 0U, 1U },  /* GW_BodyStatusFwd */
};

const CanIf_ConfigType CanIf_Config =
{
    CanIf_RxPdus, CANIF_NUM_RX_PDUS,
    CanIf_TxPdus, CANIF_NUM_TX_PDUS
};
