/**
 * @file    main.c
 * @brief   Firmware entry point. For now: clock + SysTick bring-up and a
 *          1 Hz heartbeat on PC13 (Blue Pill LED, active low). BSW modules
 *          (Det and the MCAL drivers) are initialised here as they land.
 */
#include "Std_Types.h"
#include "Det.h"
#include "stm32f103_regs.h"
#include "system_stm32f103.h"

#define LED_PIN             13U
#define HEARTBEAT_HALF_MS   500UL

static void Led_Init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* PC13: general purpose push-pull output, 2 MHz (CNF=00, MODE=10) */
    GPIO_CRH(GPIOC_BASE) = (GPIO_CRH(GPIOC_BASE) & ~(0xFUL << 20)) | (0x2UL << 20);
    GPIO_BSRR(GPIOC_BASE) = (1UL << LED_PIN);   /* LED off */
}

static void Led_Toggle(void)
{
    if ((GPIO_ODR(GPIOC_BASE) & (1UL << LED_PIN)) != 0UL)
    {
        GPIO_BSRR(GPIOC_BASE) = (1UL << (LED_PIN + 16U));  /* reset -> LED on */
    }
    else
    {
        GPIO_BSRR(GPIOC_BASE) = (1UL << LED_PIN);          /* set -> LED off */
    }
}

int main(void)
{
    uint32 lastToggleMs;

    Det_Init();

    if (System_ClockInit() != E_OK)
    {
        /* Running on HSI: timing-dependent modules (CAN bit timing) would be wrong */
        (void)Det_ReportError(DET_MODULE_ID_DET, 0U, 0U, 0xFFU);
    }
    System_SysTickInit();
    Led_Init();

    lastToggleMs = System_GetTickMs();

    for (;;)
    {
        /* Unsigned subtraction handles tick wrap-around */
        if ((System_GetTickMs() - lastToggleMs) >= HEARTBEAT_HALF_MS)
        {
            lastToggleMs += HEARTBEAT_HALF_MS;
            Led_Toggle();
        }
    }
}
