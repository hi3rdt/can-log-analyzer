/*
 * GENERATED FILE - DO NOT EDIT.
 * Source: config/gateway.dbc (node GW), config/routes.json
 * Generator: tools/dbc2c.py
 */

#include "PduR_Cfg.h"

static const PduR_DestPduType PduR_Dests_VehicleSpeed[1] =
{
    { PDUR_DEST_COM, 0U },
};

static const PduR_DestPduType PduR_Dests_EngineData[1] =
{
    { PDUR_DEST_COM, 1U },
};

static const PduR_DestPduType PduR_Dests_BodyStatus[2] =
{
    { PDUR_DEST_COM, 2U },
    { PDUR_DEST_CANIF, 1U },
};

static const PduR_DestPduType PduR_Dests_DiagExtended[1] =
{
    { PDUR_DEST_COM, 3U },
};

static const PduR_RxRouteType PduR_RxRoutes[PDUR_NUM_RX_ROUTES] =
{
    { PduR_Dests_VehicleSpeed, 1U },
    { PduR_Dests_EngineData, 1U },
    { PduR_Dests_BodyStatus, 2U },
    { PduR_Dests_DiagExtended, 1U },
};

static const PduR_ComTxRouteType PduR_ComTxRoutes[PDUR_NUM_COM_TX_ROUTES] =
{
    { 0U },  /* GW_VehicleState */
};

static const PduR_TxConfRouteType PduR_TxConfRoutes[PDUR_NUM_TXCONF_ROUTES] =
{
    { TRUE, 4U },  /* GW_VehicleState */
    { FALSE, 0U },  /* GW_BodyStatusFwd (gateway) */
};

const PduR_PBConfigType PduR_Config =
{
    PduR_RxRoutes,     PDUR_NUM_RX_ROUTES,
    PduR_ComTxRoutes, PDUR_NUM_COM_TX_ROUTES,
    PduR_TxConfRoutes, PDUR_NUM_TXCONF_ROUTES
};
