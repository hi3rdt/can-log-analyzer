/**
 * @file    stm32f103_regs.h
 * @brief   Minimal register map for STM32F103C8 (RM0008), written by hand
 *          instead of pulling in CMSIS/HAL so every access is explicit.
 *          Only peripherals used by this project are listed.
 */
#ifndef STM32F103_REGS_H
#define STM32F103_REGS_H

#include "Platform_Types.h"

/* MISRA C:2012 Dir 4.9 / Rule 11.4 deviation: register access requires
 * casting fixed addresses to volatile pointers. Kept in this file only. */
#define REG32(addr)             (*(volatile uint32 *)(addr))

/* ---------------------------------------------------------------- RCC --- */
#define RCC_BASE                0x40021000UL
#define RCC_CR                  REG32(RCC_BASE + 0x00UL)
#define RCC_CFGR                REG32(RCC_BASE + 0x04UL)
#define RCC_APB2ENR             REG32(RCC_BASE + 0x18UL)
#define RCC_APB1ENR             REG32(RCC_BASE + 0x1CUL)

#define RCC_CR_HSEON            (1UL << 16)
#define RCC_CR_HSERDY           (1UL << 17)
#define RCC_CR_PLLON            (1UL << 24)
#define RCC_CR_PLLRDY           (1UL << 25)

#define RCC_CFGR_SW_MASK        (3UL << 0)
#define RCC_CFGR_SW_PLL         (2UL << 0)
#define RCC_CFGR_SWS_MASK       (3UL << 2)
#define RCC_CFGR_SWS_PLL        (2UL << 2)
#define RCC_CFGR_PPRE1_DIV2     (4UL << 8)
#define RCC_CFGR_PLLSRC_HSE     (1UL << 16)
#define RCC_CFGR_PLLMUL9        (7UL << 18)

#define RCC_APB2ENR_AFIOEN      (1UL << 0)
#define RCC_APB2ENR_IOPBEN      (1UL << 3)
#define RCC_APB2ENR_IOPCEN      (1UL << 4)
#define RCC_APB2ENR_USART1EN    (1UL << 14)
#define RCC_APB1ENR_CANEN       (1UL << 25)

/* -------------------------------------------------------------- FLASH --- */
#define FLASH_BASE_R            0x40022000UL
#define FLASH_ACR               REG32(FLASH_BASE_R + 0x00UL)
#define FLASH_ACR_LATENCY_2WS   (2UL << 0)
#define FLASH_ACR_PRFTBE        (1UL << 4)

/* --------------------------------------------------------------- GPIO --- */
#define GPIOB_BASE              0x40010C00UL
#define GPIOC_BASE              0x40011000UL
#define GPIO_CRL(base)          REG32((base) + 0x00UL)
#define GPIO_CRH(base)          REG32((base) + 0x04UL)
#define GPIO_IDR(base)          REG32((base) + 0x08UL)
#define GPIO_ODR(base)          REG32((base) + 0x0CUL)
#define GPIO_BSRR(base)         REG32((base) + 0x10UL)

/* ------------------------------------------------------------ SysTick --- */
#define SYST_CSR                REG32(0xE000E010UL)
#define SYST_RVR                REG32(0xE000E014UL)
#define SYST_CVR                REG32(0xE000E018UL)
#define SYST_CSR_ENABLE         (1UL << 0)
#define SYST_CSR_TICKINT        (1UL << 1)
#define SYST_CSR_CLKSOURCE      (1UL << 2)  /* 1 = core clock (HCLK) */

/* -------------------------------------------------------------- Clock --- */
#define SYSCLK_HZ               72000000UL  /* HSE 8 MHz x PLL 9 */
#define PCLK1_HZ                36000000UL  /* APB1 = SYSCLK / 2 (bxCAN clock) */

#endif /* STM32F103_REGS_H */
