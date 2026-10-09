/*
 * GENERATED FILE - DO NOT EDIT.
 * Source: config/gateway.dbc (node GW), config/routes.json
 * Generator: tools/dbc2c.py
 */

#ifndef COM_CFG_H
#define COM_CFG_H

#include "Com_Types.h"

/* Com I-PDU handles */
#define ComConf_ComIPdu_VehicleSpeed                 0U  /* RX 0x100 */
#define ComConf_ComIPdu_EngineData                   1U  /* RX 0x200 */
#define ComConf_ComIPdu_BodyStatus                   2U  /* RX 0x300 */
#define ComConf_ComIPdu_DiagExtended                 3U  /* RX 0x18FEEF00 */
#define ComConf_ComIPdu_GW_VehicleState              4U  /* TX 0x400 */

/* Com signal handles: raw value; physical = raw * factor + offset */
#define ComConf_ComSignal_VehicleSpeed               0U  /* factor 0.01, offset 0 [km/h] */
#define ComConf_ComSignal_WheelSpeedFL               1U  /* factor 0.1, offset 0 [km/h] */
#define ComConf_ComSignal_WheelSpeedFR               2U  /* factor 0.1, offset 0 [km/h] */
#define ComConf_ComSignal_AbsActive                  3U  /* factor 1, offset 0 */
#define ComConf_ComSignal_SpeedCounter               4U  /* factor 1, offset 0 */
#define ComConf_ComSignal_SpeedChecksum              5U  /* factor 1, offset 0 */
#define ComConf_ComSignal_EngineRpm                  6U  /* factor 0.25, offset 0 [rpm] */
#define ComConf_ComSignal_CoolantTemp                7U  /* factor 1, offset -40 [degC] */
#define ComConf_ComSignal_ThrottlePos                8U  /* factor 0.4, offset 0 [%] */
#define ComConf_ComSignal_EngineTorque               9U  /* factor 0.5, offset 0 [Nm] */
#define ComConf_ComSignal_DoorFL                     10U  /* factor 1, offset 0 */
#define ComConf_ComSignal_DoorFR                     11U  /* factor 1, offset 0 */
#define ComConf_ComSignal_DoorRL                     12U  /* factor 1, offset 0 */
#define ComConf_ComSignal_DoorRR                     13U  /* factor 1, offset 0 */
#define ComConf_ComSignal_SteeringAngle              14U  /* factor 0.1, offset 0 [deg] */
#define ComConf_ComSignal_OdometerKm                 15U  /* factor 1, offset 0 [km] */
#define ComConf_ComSignal_AmbientTemp                16U  /* factor 0.5, offset 0 [degC] */
#define ComConf_ComSignal_GW_VehicleSpeed            17U  /* factor 0.01, offset 0 [km/h] */
#define ComConf_ComSignal_GW_EngineRpm               18U  /* factor 0.25, offset 0 [rpm] */
#define ComConf_ComSignal_GW_CoolantTemp             19U  /* factor 1, offset -40 [degC] */
#define ComConf_ComSignal_GW_DoorsOpen               20U  /* factor 1, offset 0 */
#define ComConf_ComSignal_GW_SteeringAngle           21U  /* factor 0.1, offset 0 [deg] */

#define COM_NUM_IPDUS     5U
#define COM_NUM_SIGNALS   22U

extern const Com_ConfigType Com_Config;

#endif /* COM_CFG_H */
