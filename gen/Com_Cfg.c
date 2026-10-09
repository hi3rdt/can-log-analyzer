/*
 * GENERATED FILE - DO NOT EDIT.
 * Source: config/gateway.dbc (node GW), config/routes.json
 * Generator: tools/dbc2c.py
 */

#include "Com_Cfg.h"

static uint8 Com_Buffer_VehicleSpeed[8];
static uint8 Com_Buffer_EngineData[8];
static uint8 Com_Buffer_BodyStatus[4];
static uint8 Com_Buffer_DiagExtended[8];
static uint8 Com_Buffer_GW_VehicleState[8];

static const Com_IPduConfigType Com_IPdus[COM_NUM_IPDUS] =
{
    { COM_RECEIVE, 8U, Com_Buffer_VehicleSpeed, 0U },  /* VehicleSpeed */
    { COM_RECEIVE, 8U, Com_Buffer_EngineData, 0U },  /* EngineData */
    { COM_RECEIVE, 4U, Com_Buffer_BodyStatus, 0U },  /* BodyStatus */
    { COM_RECEIVE, 8U, Com_Buffer_DiagExtended, 0U },  /* DiagExtended */
    { COM_SEND   , 8U, Com_Buffer_GW_VehicleState, 0U },  /* GW_VehicleState */
};

static const Com_SignalConfigType Com_Signals[COM_NUM_SIGNALS] =
{
    /* bitPos, size, endianness,          type,         ipdu, init */
    {  0U, 16U, COM_LITTLE_ENDIAN, COM_UINT16  , 0U, 0x0UL },  /* VehicleSpeed */
    { 16U, 12U, COM_LITTLE_ENDIAN, COM_UINT16  , 0U, 0x0UL },  /* WheelSpeedFL */
    { 28U, 12U, COM_LITTLE_ENDIAN, COM_UINT16  , 0U, 0x0UL },  /* WheelSpeedFR */
    { 40U,  1U, COM_LITTLE_ENDIAN, COM_BOOLEAN , 0U, 0x0UL },  /* AbsActive */
    { 56U,  4U, COM_LITTLE_ENDIAN, COM_UINT8   , 0U, 0x0UL },  /* SpeedCounter */
    { 60U,  4U, COM_LITTLE_ENDIAN, COM_UINT8   , 0U, 0x0UL },  /* SpeedChecksum */
    {  7U, 16U, COM_BIG_ENDIAN   , COM_UINT16  , 1U, 0x0UL },  /* EngineRpm */
    { 23U,  8U, COM_BIG_ENDIAN   , COM_UINT8   , 1U, 0x28UL },  /* CoolantTemp */
    { 24U,  8U, COM_LITTLE_ENDIAN, COM_UINT8   , 1U, 0x0UL },  /* ThrottlePos */
    { 37U, 11U, COM_BIG_ENDIAN   , COM_SINT16  , 1U, 0x0UL },  /* EngineTorque */
    {  0U,  1U, COM_LITTLE_ENDIAN, COM_BOOLEAN , 2U, 0x0UL },  /* DoorFL */
    {  1U,  1U, COM_LITTLE_ENDIAN, COM_BOOLEAN , 2U, 0x0UL },  /* DoorFR */
    {  2U,  1U, COM_LITTLE_ENDIAN, COM_BOOLEAN , 2U, 0x0UL },  /* DoorRL */
    {  3U,  1U, COM_LITTLE_ENDIAN, COM_BOOLEAN , 2U, 0x0UL },  /* DoorRR */
    { 16U, 16U, COM_LITTLE_ENDIAN, COM_SINT16  , 2U, 0x0UL },  /* SteeringAngle */
    {  0U, 24U, COM_LITTLE_ENDIAN, COM_UINT32  , 3U, 0x0UL },  /* OdometerKm */
    { 24U,  8U, COM_LITTLE_ENDIAN, COM_SINT8   , 3U, 0x0UL },  /* AmbientTemp */
    {  0U, 16U, COM_LITTLE_ENDIAN, COM_UINT16  , 4U, 0x0UL },  /* GW_VehicleSpeed */
    { 16U, 16U, COM_LITTLE_ENDIAN, COM_UINT16  , 4U, 0x0UL },  /* GW_EngineRpm */
    { 32U,  8U, COM_LITTLE_ENDIAN, COM_UINT8   , 4U, 0x28UL },  /* GW_CoolantTemp */
    { 40U,  4U, COM_LITTLE_ENDIAN, COM_UINT8   , 4U, 0x0UL },  /* GW_DoorsOpen */
    { 55U, 16U, COM_BIG_ENDIAN   , COM_SINT16  , 4U, 0x0UL },  /* GW_SteeringAngle */
};

const Com_ConfigType Com_Config =
{
    Com_Signals, COM_NUM_SIGNALS,
    Com_IPdus,   COM_NUM_IPDUS
};
