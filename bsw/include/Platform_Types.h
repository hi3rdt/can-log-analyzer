/**
 * @file    Platform_Types.h
 * @brief   Platform-specific base types (AUTOSAR SWS_PlatformTypes subset).
 *          Target: ARM Cortex-M3 (STM32F103), also valid for 32/64-bit hosts.
 */
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>

#define CPU_TYPE_8          8U
#define CPU_TYPE_16         16U
#define CPU_TYPE_32         32U

#define MSB_FIRST           0U
#define LSB_FIRST           1U

#define HIGH_BYTE_FIRST     0U
#define LOW_BYTE_FIRST      1U

#define CPU_TYPE            CPU_TYPE_32
#define CPU_BIT_ORDER       LSB_FIRST
#define CPU_BYTE_ORDER      LOW_BYTE_FIRST

#ifndef TRUE
#define TRUE                1U
#endif
#ifndef FALSE
#define FALSE               0U
#endif

typedef uint8_t   boolean;
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef uint64_t  uint64;
typedef int8_t    sint8;
typedef int16_t   sint16;
typedef int32_t   sint32;
typedef int64_t   sint64;
typedef float     float32;
typedef double    float64;

#endif /* PLATFORM_TYPES_H */
