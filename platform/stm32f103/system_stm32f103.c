/**
 * @file    system_stm32f103.c
 * @brief   Clock tree and SysTick setup (register level, no HAL).
 */
#include "system_stm32f103.h"
#include "stm32f103_regs.h"

/* Bounded wait so a missing/broken crystal does not hang the ECU forever. */
#define SYSTEM_READY_TIMEOUT    0x00100000UL

static volatile uint32 System_TickMs;

Std_ReturnType System_ClockInit(void)
{
    Std_ReturnType ret = E_NOT_OK;
    uint32 timeout = SYSTEM_READY_TIMEOUT;

    RCC_CR |= RCC_CR_HSEON;
    while (((RCC_CR & RCC_CR_HSERDY) == 0UL) && (timeout > 0UL))
    {
        timeout--;
    }

    if (timeout > 0UL)
    {
        /* 72 MHz from Flash needs 2 wait states (RM0008 3.3.3) */
        FLASH_ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2WS;

        /* PLL source HSE, x9; APB1 /2 (max 36 MHz); AHB and APB2 /1 */
        RCC_CFGR = RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMUL9 | RCC_CFGR_PPRE1_DIV2;

        RCC_CR |= RCC_CR_PLLON;
        timeout = SYSTEM_READY_TIMEOUT;
        while (((RCC_CR & RCC_CR_PLLRDY) == 0UL) && (timeout > 0UL))
        {
            timeout--;
        }

        if (timeout > 0UL)
        {
            RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_PLL;
            while ((RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL)
            {
                /* Switch completes within a few cycles once PLL is locked */
            }
            ret = E_OK;
        }
    }

    /* On E_NOT_OK the core keeps running on HSI 8 MHz; caller decides. */
    return ret;
}

void System_SysTickInit(void)
{
    System_TickMs = 0UL;
    SYST_RVR = (SYSCLK_HZ / 1000UL) - 1UL;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

uint32 System_GetTickMs(void)
{
    /* 32-bit aligned read is atomic on Cortex-M3 */
    return System_TickMs;
}

void SysTick_Handler(void);
void SysTick_Handler(void)
{
    System_TickMs++;
}
