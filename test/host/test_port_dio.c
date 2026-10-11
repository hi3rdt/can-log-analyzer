/**
 * @file    test_port_dio.c
 * @brief   Host tests for the Port and Dio MCAL drivers (MCAL exercise 1).
 *
 * The drivers run against RAM-backed fake registers (stubs/Mcal_FakeRegs.c).
 * Expected values come from RM0008 section 9.2 (GPIO registers) and 7.3.7
 * (RCC_APB2ENR). Enable with:
 *   cmake -S . -B build-host -DMCAL_PORT_DIO_EXERCISE_TESTS=ON
 */
#include "Port.h"
#include "Dio.h"
#include "Det.h"
#include "Mcal_FakeRegs.h"
#include "unit_test.h"

#define GPIOA   (&Mcal_FakeGpio[PORT_A])
#define GPIOB   (&Mcal_FakeGpio[PORT_B])
#define GPIOC   (&Mcal_FakeGpio[PORT_C])

#define RCC_APB2ENR_AFIOEN      (1UL << 0)
#define RCC_APB2ENR_IOPAEN      (1UL << 2)
#define RCC_APB2ENR_IOPBEN      (1UL << 3)
#define RCC_APB2ENR_IOPCEN      (1UL << 4)
#define RCC_APB2ENR_USART1EN    (1UL << 14)

#define PIN_PA0     PORT_PIN(PORT_A, 0U)
#define PIN_PA1     PORT_PIN(PORT_A, 1U)
#define PIN_PA7     PORT_PIN(PORT_A, 7U)
#define PIN_PB8     PORT_PIN(PORT_B, 8U)    /* CAN RX (remap 2) */
#define PIN_PB9     PORT_PIN(PORT_B, 9U)    /* CAN TX (remap 2) */
#define PIN_PC13    PORT_PIN(PORT_C, 13U)   /* Blue Pill LED, active low */

static void setup(void)
{
    Mcal_FakeRegs_Reset();
    Det_Init();
    Port_DeInit();
}

static void check_last_det(uint16 module, uint8 api, uint8 error)
{
    Det_ErrorType err = { 0U, 0U, 0U, 0U };

    UT_CHECK_EQ(E_OK, Det_GetLastError(&err));
    UT_CHECK_EQ(module, err.moduleId);
    UT_CHECK_EQ(api, err.apiId);
    UT_CHECK_EQ(error, err.errorId);
}

static boolean fake_regs_untouched(void)
{
    uint8 p;
    boolean same = (Mcal_FakeRcc.APB2ENR == 0UL) ? TRUE : FALSE;

    for (p = 0U; p < MCAL_GPIO_PORT_COUNT; p++)
    {
        if ((Mcal_FakeGpio[p].CRL != 0x44444444UL) || (Mcal_FakeGpio[p].CRH != 0x44444444UL) ||
            (Mcal_FakeGpio[p].ODR != 0UL) || (Mcal_FakeGpio[p].BSRR != 0UL) ||
            (Mcal_FakeGpio[p].BRR != 0UL))
        {
            same = FALSE;
        }
    }
    return same;
}

/* --------------------------------------------------------- Mode nibble --- */

static void test_mode_nibble_table(void)
{
    /* Inputs ignore speed (MODE = 00) */
    UT_CHECK_EQ(0x0U, Port_Hw_ModeNibble(PORT_MODE_ANALOG, PORT_SPEED_50MHZ));
    UT_CHECK_EQ(0x4U, Port_Hw_ModeNibble(PORT_MODE_INPUT_FLOATING, PORT_SPEED_50MHZ));
    UT_CHECK_EQ(0x8U, Port_Hw_ModeNibble(PORT_MODE_INPUT_PULLUP, PORT_SPEED_2MHZ));
    UT_CHECK_EQ(0x8U, Port_Hw_ModeNibble(PORT_MODE_INPUT_PULLDOWN, PORT_SPEED_10MHZ));
    /* Outputs: CNF in bits 3:2, MODE (speed) in bits 1:0 */
    UT_CHECK_EQ(0x2U, Port_Hw_ModeNibble(PORT_MODE_OUTPUT_PP, PORT_SPEED_2MHZ));
    UT_CHECK_EQ(0x1U, Port_Hw_ModeNibble(PORT_MODE_OUTPUT_PP, PORT_SPEED_10MHZ));
    UT_CHECK_EQ(0x7U, Port_Hw_ModeNibble(PORT_MODE_OUTPUT_OD, PORT_SPEED_50MHZ));
    UT_CHECK_EQ(0xBU, Port_Hw_ModeNibble(PORT_MODE_ALT_PP, PORT_SPEED_50MHZ));
    UT_CHECK_EQ(0xEU, Port_Hw_ModeNibble(PORT_MODE_ALT_OD, PORT_SPEED_2MHZ));
}

/* ----------------------------------------------------------- Port_Init --- */

static const Port_PinConfigType BoardPins[] =
{
    /* pin       mode                      speed             level     changeable */
    { PIN_PC13, PORT_MODE_OUTPUT_PP,      PORT_SPEED_2MHZ,  STD_HIGH, FALSE },  /* LED off */
    { PIN_PB8,  PORT_MODE_INPUT_PULLUP,   PORT_SPEED_2MHZ,  STD_LOW,  FALSE },  /* CAN RX  */
    { PIN_PB9,  PORT_MODE_ALT_PP,         PORT_SPEED_50MHZ, STD_HIGH, FALSE },  /* CAN TX  */
    { PIN_PA0,  PORT_MODE_ANALOG,         PORT_SPEED_2MHZ,  STD_LOW,  FALSE },
    { PIN_PA1,  PORT_MODE_INPUT_PULLDOWN, PORT_SPEED_2MHZ,  STD_LOW,  FALSE },
    { PIN_PA7,  PORT_MODE_OUTPUT_OD,      PORT_SPEED_10MHZ, STD_LOW,  TRUE  }
};

static const Port_ConfigType BoardConfig = { BoardPins, 6U };

static void test_init_enables_port_clocks_and_keeps_other_bits(void)
{
    setup();
    Mcal_FakeRcc.APB2ENR = RCC_APB2ENR_USART1EN | RCC_APB2ENR_AFIOEN;

    Port_Init(&BoardConfig);

    UT_CHECK_EQ(RCC_APB2ENR_USART1EN | RCC_APB2ENR_AFIOEN |
                RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN,
                Mcal_FakeRcc.APB2ENR);
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_init_writes_crl_crh_nibbles(void)
{
    setup();
    Port_Init(&BoardConfig);

    /* GPIOA CRL: pin0 analog 0x0, pin1 pull-down 0x8, pin7 OD 10 MHz 0x5 */
    UT_CHECK_EQ(0x54444480UL, GPIOA->CRL);
    UT_CHECK_EQ(0x44444444UL, GPIOA->CRH);
    /* GPIOB CRH: pin8 pull-up 0x8, pin9 AF PP 50 MHz 0xB */
    UT_CHECK_EQ(0x44444444UL, GPIOB->CRL);
    UT_CHECK_EQ(0x444444B8UL, GPIOB->CRH);
    /* GPIOC CRH: pin13 PP 2 MHz 0x2 */
    UT_CHECK_EQ(0x44244444UL, GPIOC->CRH);
}

static void test_init_sets_odr_for_levels_and_pulls(void)
{
    setup();
    /* Bits the config does not own must survive (read-modify-write) */
    GPIOA->ODR = (1UL << 1) | (1UL << 7) | (1UL << 15);    /* PA1, PA7 must be cleared */
    GPIOB->ODR = (1UL << 9) | (1UL << 0);                   /* PB0 untouched           */
    GPIOC->ODR = 0UL;

    Port_Init(&BoardConfig);

    UT_CHECK_EQ(1UL << 15, GPIOA->ODR);                     /* PA0 analog: untouched   */
    UT_CHECK_EQ((1UL << 0) | (1UL << 8) | (1UL << 9), GPIOB->ODR);  /* pull-up, AF high */
    UT_CHECK_EQ(1UL << 13, GPIOC->ODR);                     /* LED initial high        */
}

static void test_init_preserves_unconfigured_nibbles(void)
{
    static const Port_PinConfigType onePin[] =
    {
        { PORT_PIN(PORT_B, 3U), PORT_MODE_OUTPUT_PP, PORT_SPEED_50MHZ, STD_LOW, FALSE }
    };
    static const Port_ConfigType cfg = { onePin, 1U };

    setup();
    GPIOB->CRL = 0x12345678UL;
    Port_Init(&cfg);

    UT_CHECK_EQ(0x12343678UL, GPIOB->CRL);
}

static void test_init_null_pointer(void)
{
    static const Port_ConfigType noPins = { NULL_PTR, 1U };

    setup();
    Port_Init(NULL_PTR);
    check_last_det(DET_MODULE_ID_PORT, PORT_SID_INIT, PORT_E_PARAM_POINTER);

    Port_Init(&noPins);
    check_last_det(DET_MODULE_ID_PORT, PORT_SID_INIT, PORT_E_PARAM_POINTER);
    UT_CHECK_EQ(2U, Det_GetErrorCount());
    UT_CHECK(fake_regs_untouched());
}

static void test_init_invalid_pin_writes_nothing(void)
{
    /* The bad pin is LAST: a driver that checks while writing would already
     * have configured PC13. */
    static const Port_PinConfigType pins[] =
    {
        { PIN_PC13, PORT_MODE_OUTPUT_PP, PORT_SPEED_2MHZ, STD_HIGH, FALSE },
        { 48U,      PORT_MODE_OUTPUT_PP, PORT_SPEED_2MHZ, STD_HIGH, FALSE }
    };
    static const Port_ConfigType cfg = { pins, 2U };

    setup();
    Port_Init(&cfg);

    check_last_det(DET_MODULE_ID_PORT, PORT_SID_INIT, PORT_E_PARAM_CONFIG);
    UT_CHECK(fake_regs_untouched());

    /* Failed init must not count as initialised */
    Port_SetPinDirection(PIN_PC13, PORT_PIN_IN);
    check_last_det(DET_MODULE_ID_PORT, PORT_SID_SET_PIN_DIRECTION, PORT_E_UNINIT);
}

/* ------------------------------------------------ Port_SetPinDirection --- */

static void test_set_direction_before_init(void)
{
    setup();
    Port_SetPinDirection(PIN_PA7, PORT_PIN_IN);

    check_last_det(DET_MODULE_ID_PORT, PORT_SID_SET_PIN_DIRECTION, PORT_E_UNINIT);
    UT_CHECK(fake_regs_untouched());
}

static void test_set_direction_changes_only_that_nibble(void)
{
    setup();
    Port_Init(&BoardConfig);

    Port_SetPinDirection(PIN_PA7, PORT_PIN_IN);
    UT_CHECK_EQ(0x44444480UL, GPIOA->CRL);              /* PA7 -> floating input */

    Port_SetPinDirection(PIN_PA7, PORT_PIN_OUT);
    UT_CHECK_EQ(0x14444480UL, GPIOA->CRL);              /* push-pull, 10 MHz     */
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_set_direction_errors(void)
{
    setup();
    Port_Init(&BoardConfig);

    Port_SetPinDirection(48U, PORT_PIN_IN);
    check_last_det(DET_MODULE_ID_PORT, PORT_SID_SET_PIN_DIRECTION, PORT_E_PARAM_PIN);

    Port_SetPinDirection(PIN_PC13, PORT_PIN_IN);       /* configured, not changeable */
    check_last_det(DET_MODULE_ID_PORT, PORT_SID_SET_PIN_DIRECTION,
                   PORT_E_DIRECTION_UNCHANGEABLE);
    UT_CHECK_EQ(0x44244444UL, GPIOC->CRH);

    Port_SetPinDirection(PORT_PIN(PORT_B, 0U), PORT_PIN_OUT);  /* not in config */
    check_last_det(DET_MODULE_ID_PORT, PORT_SID_SET_PIN_DIRECTION,
                   PORT_E_DIRECTION_UNCHANGEABLE);
    UT_CHECK_EQ(0x44444444UL, GPIOB->CRL);
    UT_CHECK_EQ(3U, Det_GetErrorCount());
}

/* ----------------------------------------------------------------- Dio --- */

static void test_read_channel_uses_idr(void)
{
    setup();
    GPIOC->IDR = 1UL << 13;
    GPIOC->ODR = 0UL;           /* output latch must not be what is read */
    GPIOA->IDR = 0xFFFEUL;

    UT_CHECK_EQ(STD_HIGH, Dio_ReadChannel(PIN_PC13));
    UT_CHECK_EQ(STD_LOW,  Dio_ReadChannel(PIN_PA0));
    UT_CHECK_EQ(STD_HIGH, Dio_ReadChannel(PORT_PIN(PORT_A, 15U)));
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_write_channel_uses_bsrr(void)
{
    setup();
    GPIOC->ODR = 0x1234UL;

    Dio_WriteChannel(PIN_PC13, STD_HIGH);
    UT_CHECK_EQ(1UL << 13, GPIOC->BSRR);

    Dio_WriteChannel(PIN_PC13, STD_LOW);
    UT_CHECK_EQ(1UL << (13U + 16U), GPIOC->BSRR);

    Dio_WriteChannel(PORT_PIN(PORT_B, 15U), STD_HIGH);
    UT_CHECK_EQ(1UL << 15, GPIOB->BSRR);

    UT_CHECK_EQ(0x1234UL, GPIOC->ODR);      /* never read-modify-write on ODR */
    UT_CHECK_EQ(0UL, GPIOC->BRR);
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_flip_channel_uses_odr(void)
{
    setup();
    /* Pin reads low (e.g. open-drain held low) but the latch is high:
     * flip must follow the latch, so the result is LOW. */
    GPIOA->ODR = 1UL << 7;
    GPIOA->IDR = 0UL;
    UT_CHECK_EQ(STD_LOW, Dio_FlipChannel(PIN_PA7));
    UT_CHECK_EQ(1UL << (7U + 16U), GPIOA->BSRR);
    UT_CHECK_EQ(1UL << 7, GPIOA->ODR);

    GPIOC->ODR = 0UL;
    GPIOC->IDR = 1UL << 13;
    UT_CHECK_EQ(STD_HIGH, Dio_FlipChannel(PIN_PC13));
    UT_CHECK_EQ(1UL << 13, GPIOC->BSRR);
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_read_port(void)
{
    setup();
    GPIOB->IDR = 0xFFFFA5C3UL;          /* upper half is reserved on hardware */

    UT_CHECK_EQ(0xA5C3U, Dio_ReadPort(PORT_B));
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_write_port_is_one_atomic_write(void)
{
    setup();
    GPIOA->ODR = 0x00FFUL;

    Dio_WritePort(PORT_A, 0x8001U);
    UT_CHECK_EQ(0x7FFE8001UL, GPIOA->BSRR);

    Dio_WritePort(PORT_C, 0xFFFFU);
    UT_CHECK_EQ(0x0000FFFFUL, GPIOC->BSRR);

    Dio_WritePort(PORT_B, 0x0000U);
    UT_CHECK_EQ(0xFFFF0000UL, GPIOB->BSRR);

    UT_CHECK_EQ(0x00FFUL, GPIOA->ODR);
    UT_CHECK_EQ(0U, Det_GetErrorCount());
}

static void test_dio_invalid_ids(void)
{
    setup();
    GPIOA->IDR = 0xFFFFUL;
    GPIOA->ODR = 0xFFFFUL;

    UT_CHECK_EQ(STD_LOW, Dio_ReadChannel(DIO_CHANNEL_COUNT));
    check_last_det(DET_MODULE_ID_DIO, DIO_SID_READ_CHANNEL, DIO_E_PARAM_INVALID_CHANNEL_ID);

    Dio_WriteChannel(0xFFU, STD_HIGH);
    check_last_det(DET_MODULE_ID_DIO, DIO_SID_WRITE_CHANNEL, DIO_E_PARAM_INVALID_CHANNEL_ID);

    UT_CHECK_EQ(STD_LOW, Dio_FlipChannel(48U));
    check_last_det(DET_MODULE_ID_DIO, DIO_SID_FLIP_CHANNEL, DIO_E_PARAM_INVALID_CHANNEL_ID);

    UT_CHECK_EQ(0U, Dio_ReadPort(DIO_PORT_COUNT));
    check_last_det(DET_MODULE_ID_DIO, DIO_SID_READ_PORT, DIO_E_PARAM_INVALID_PORT_ID);

    Dio_WritePort(3U, 0xFFFFU);
    check_last_det(DET_MODULE_ID_DIO, DIO_SID_WRITE_PORT, DIO_E_PARAM_INVALID_PORT_ID);

    UT_CHECK_EQ(5U, Det_GetErrorCount());
    UT_CHECK_EQ(0UL, GPIOA->BSRR);
    UT_CHECK_EQ(0UL, GPIOB->BSRR);
    UT_CHECK_EQ(0UL, GPIOC->BSRR);
}

int main(void)
{
    UT_RUN(test_mode_nibble_table);
    UT_RUN(test_init_enables_port_clocks_and_keeps_other_bits);
    UT_RUN(test_init_writes_crl_crh_nibbles);
    UT_RUN(test_init_sets_odr_for_levels_and_pulls);
    UT_RUN(test_init_preserves_unconfigured_nibbles);
    UT_RUN(test_init_null_pointer);
    UT_RUN(test_init_invalid_pin_writes_nothing);
    UT_RUN(test_set_direction_before_init);
    UT_RUN(test_set_direction_changes_only_that_nibble);
    UT_RUN(test_set_direction_errors);
    UT_RUN(test_read_channel_uses_idr);
    UT_RUN(test_write_channel_uses_bsrr);
    UT_RUN(test_flip_channel_uses_odr);
    UT_RUN(test_read_port);
    UT_RUN(test_write_port_is_one_atomic_write);
    UT_RUN(test_dio_invalid_ids);
    return UT_REPORT();
}
