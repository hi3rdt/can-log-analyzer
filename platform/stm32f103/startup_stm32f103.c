/**
 * @file    startup_stm32f103.c
 * @brief   Vector table and reset handler for STM32F103x8/xB (medium density),
 *          written in C. Interrupt numbers follow RM0008 table 63.
 */
#include "Platform_Types.h"

/* Symbols provided by stm32f103c8.ld */
extern uint32 _estack;
extern uint32 _sidata;
extern uint32 _sdata;
extern uint32 _edata;
extern uint32 _sbss;
extern uint32 _ebss;

extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Core exceptions and IRQs default to Default_Handler unless a module defines them */
void NMI_Handler(void)                 __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)            __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)                 __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)            __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)              __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)             __attribute__((weak, alias("Default_Handler")));
void USB_HP_CAN1_TX_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void USB_LP_CAN1_RX0_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void CAN1_RX1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void CAN1_SCE_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));

typedef void (*VectorType)(void);

/* 16 core entries + 43 device IRQs (0..42) */
__attribute__((section(".isr_vector"), used))
static const VectorType VectorTable[16U + 43U] =
{
    (VectorType)&_estack,       /* Initial MSP (MISRA 11.1 deviation, required by HW) */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,                 /* Reserved */
    SVC_Handler,
    DebugMon_Handler,
    0,                          /* Reserved */
    PendSV_Handler,
    SysTick_Handler,
    /* IRQ 0..18: WWDG, PVD, TAMPER, RTC, FLASH, RCC, EXTI0..4, DMA1_Ch1..7, ADC1_2 */
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,
    USB_HP_CAN1_TX_IRQHandler,  /* IRQ 19 */
    USB_LP_CAN1_RX0_IRQHandler, /* IRQ 20 */
    CAN1_RX1_IRQHandler,        /* IRQ 21 */
    CAN1_SCE_IRQHandler,        /* IRQ 22 */
    /* IRQ 23..36: EXTI9_5, TIM1_BRK/UP/TRG_COM/CC, TIM2..4, I2C1/2 EV/ER, SPI1/2 */
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler,
    USART1_IRQHandler,          /* IRQ 37 */
    /* IRQ 38..42: USART2, USART3, EXTI15_10, RTCAlarm, USBWakeup */
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler
};

void Reset_Handler(void)
{
    uint32 *src = &_sidata;
    uint32 *dst = &_sdata;

    /* Copy initialised data from Flash to RAM */
    while (dst < &_edata)
    {
        *dst = *src;
        dst++;
        src++;
    }

    /* Zero .bss */
    dst = &_sbss;
    while (dst < &_ebss)
    {
        *dst = 0UL;
        dst++;
    }

    (void)main();

    for (;;)
    {
        /* main() must not return on an ECU */
    }
}

void Default_Handler(void)
{
    for (;;)
    {
        /* Unexpected interrupt: halt here so the debugger shows it */
    }
}
