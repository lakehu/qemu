/*
 * STM32 Microcontroller RCC (Reset and Clock Control) module
 *
 * Copyright (C) 2010 Andre Beckus
 *
 * Source code based on omap_clk.c
 * Implementation based on ST Microelectronics "RM0008 Reference Manual Rev 10"
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hw/arm/stm32.h"
#include "hw/arm/stm32_clktree.h"
#include "qemu/bitops.h"
#include "hw/hw.h"
#include "hw/irq.h"
//#include "hw/arm/arm.h"
#include "hw/qdev-clock.h"
#include <stdio.h>


/* DEFINITIONS*/

/* See README for DEBUG details. */
#define DEBUG_STM32_RCC

#ifdef DEBUG_STM32_RCC
#define DPRINTF(fmt, ...)                                       \
    do { fprintf(stderr, "STM32_RCC: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define HSI_FREQ 8000000
#define LSI_FREQ 40000

#define RCC_CR_OFFSET 0x00
#define RCC_CR_PLL3RDY_CL_BIT   29
#define RCC_CR_PLL3ON_CL_BIT    28
#define RCC_CR_PLL2RDY_CL_BIT   27
#define RCC_CR_PLL2ON_CL_BIT    26
#define RCC_CR_PLLRDY_BIT       25
#define RCC_CR_PLLON_BIT        24
#define RCC_CR_CSSON_BIT        19
#define RCC_CR_HSEBYP_BIT       18
#define RCC_CR_HSERDY_BIT       17
#define RCC_CR_HSEON_BIT        16
#define RCC_CR_HSICAL_START     8
#define RCC_CR_HSICAL_MASK      0x0000ff00
#define RCC_CR_HSITRIM_START    3
#define RCC_CR_HSITRIM_MASK     0x000000f8
#define RCC_CR_HSIRDY_BIT       1
#define RCC_CR_HSION_BIT        0

#define RCC_CFGR_OFFSET 0x04
#define RCC_CFGR_MCO_START       24
#define RCC_CFGR_MCO_MASK        0x07000000
#define RCC_CFGR_MCO_CL_MASK     0x0f000000
#define RCC_CFGR_USBPRE_BIT      22
#define RCC_CFGR_OTGFSPRE_CL_BIT 22
#define RCC_CFGR_PLLMUL_START    18
#define RCC_CFGR_PLLMUL_LENGTH   4
#define RCC_CFGR_PLLXTPRE_BIT    17
#define RCC_CFGR_PLLSRC_BIT      16
#define RCC_CFGR_ADCPRE_START    14
#define RCC_CFGR_ADCPRE_MASK     0x0000c000
#define RCC_CFGR_PPRE2_START     11
#define RCC_CFGR_PPRE2_MASK      0x00003c00
#define RCC_CFGR_PPRE1_START     8
#define RCC_CFGR_PPRE1_MASK      0x00000700
#define RCC_CFGR_HPRE_START      4
#define RCC_CFGR_HPRE_MASK       0x000000f0
#define RCC_CFGR_SWS_START       2
#define RCC_CFGR_SWS_MASK        0x0000000c
#define RCC_CFGR_SW_START        0
#define RCC_CFGR_SW_MASK         0x00000003

#define RCC_CIR_OFFSET 0x08

#define RCC_APB2RSTR_OFFSET 0x0c

#define RCC_APB2RSTR_TIM11RST_BIT       21
#define RCC_APB2RSTR_TIM10RST_BIT       20
#define RCC_APB2RSTR_TIM9RST_BIT        19
#define RCC_APB2RSTR_ADC3RST_BIT        15
#define RCC_APB2RSTR_USART1RST_BIT      14
#define RCC_APB2RSTR_TIM8RST_BIT        13
#define RCC_APB2RSTR_SPI1RST_BIT        12
#define RCC_APB2RSTR_TIM1RST_BIT        11
#define RCC_APB2RSTR_ADC2RST_BIT        10
#define RCC_APB2RSTR_ADC1RST_BIT        9
#define RCC_APB2RSTR_IOPGRST_BIT        8
#define RCC_APB2RSTR_IOPFRST_BIT        7
#define RCC_APB2RSTR_IOPERST_BIT        6
#define RCC_APB2RSTR_IOPDRST_BIT        5
#define RCC_APB2RSTR_IOPCRST_BIT        4
#define RCC_APB2RSTR_IOPBRST_BIT        3
#define RCC_APB2RSTR_IOPARST_BIT        2
#define RCC_APB2RSTR_AFIORST_BIT        0

#define RCC_APB2RSTR_TIM11RST           (1U << RCC_APB2RSTR_TIM11RST_BIT)
#define RCC_APB2RSTR_TIM10RST           (1U << RCC_APB2RSTR_TIM10RST_BIT)
#define RCC_APB2RSTR_TIM9RST            (1U << RCC_APB2RSTR_TIM9RST_BIT)
#define RCC_APB2RSTR_ADC3RST            (1U << RCC_APB2RSTR_ADC3RST_BIT)
#define RCC_APB2RSTR_USART1RST          (1U << RCC_APB2RSTR_USART1RST_BIT)
#define RCC_APB2RSTR_TIM8RST            (1U << RCC_APB2RSTR_TIM8RST_BIT)
#define RCC_APB2RSTR_SPI1RST            (1U << RCC_APB2RSTR_SPI1RST_BIT)
#define RCC_APB2RSTR_TIM1RST            (1U << RCC_APB2RSTR_TIM1RST_BIT)
#define RCC_APB2RSTR_ADC2RST            (1U << RCC_APB2RSTR_ADC2RST_BIT)
#define RCC_APB2RSTR_ADC1RST            (1U << RCC_APB2RSTR_ADC1RST_BIT)
#define RCC_APB2RSTR_IOPGRST            (1U << RCC_APB2RSTR_IOPGRST_BIT)
#define RCC_APB2RSTR_IOPFRST            (1U << RCC_APB2RSTR_IOPFRST_BIT)
#define RCC_APB2RSTR_IOPERST            (1U << RCC_APB2RSTR_IOPERST_BIT)
#define RCC_APB2RSTR_IOPDRST            (1U << RCC_APB2RSTR_IOPDRST_BIT)
#define RCC_APB2RSTR_IOPCRST            (1U << RCC_APB2RSTR_IOPCRST_BIT)
#define RCC_APB2RSTR_IOPBRST            (1U << RCC_APB2RSTR_IOPBRST_BIT)
#define RCC_APB2RSTR_IOPARST            (1U << RCC_APB2RSTR_IOPARST_BIT)
#define RCC_APB2RSTR_AFIORST            (1U << RCC_APB2RSTR_AFIORST_BIT)

#define RCC_APB1RSTR_OFFSET 0x10

#define RCC_APB1RSTR_DACRST_BIT         29
#define RCC_APB1RSTR_PWRRST_BIT         28
#define RCC_APB1RSTR_BKPRST_BIT         27
#define RCC_APB1RSTR_CANRST_BIT         25
#define RCC_APB1RSTR_USBRST_BIT         23
#define RCC_APB1RSTR_I2C2RST_BIT        22
#define RCC_APB1RSTR_I2C1RST_BIT        21
#define RCC_APB1RSTR_UART5RST_BIT       20
#define RCC_APB1RSTR_UART4RST_BIT       19
#define RCC_APB1RSTR_USART3RST_BIT      18
#define RCC_APB1RSTR_USART2RST_BIT      17
#define RCC_APB1RSTR_SPI3RST_BIT        15
#define RCC_APB1RSTR_SPI2RST_BIT        14
#define RCC_APB1RSTR_WWDRST_BIT         11
#define RCC_APB1RSTR_TIM14RST_BIT       8
#define RCC_APB1RSTR_TIM13RST_BIT       7
#define RCC_APB1RSTR_TIM12RST_BIT       6
#define RCC_APB1RSTR_TIM7RST_BIT        5
#define RCC_APB1RSTR_TIM6RST_BIT        4
#define RCC_APB1RSTR_TIM5RST_BIT        3
#define RCC_APB1RSTR_TIM4RST_BIT        2
#define RCC_APB1RSTR_TIM3RST_BIT        1
#define RCC_APB1RSTR_TIM2RST_BIT        0

#define RCC_APB1RSTR_DACRST             (1U << RCC_APB1RSTR_DACRST_BIT)
#define RCC_APB1RSTR_PWRRST             (1U << RCC_APB1RSTR_PWRRST_BIT)
#define RCC_APB1RSTR_BKPRST             (1U << RCC_APB1RSTR_BKPRST_BIT)
#define RCC_APB1RSTR_CANRST             (1U << RCC_APB1RSTR_CANRST_BIT)
#define RCC_APB1RSTR_USBRST             (1U << RCC_APB1RSTR_USBRST_BIT)
#define RCC_APB1RSTR_I2C2RST            (1U << RCC_APB1RSTR_I2C2RST_BIT)
#define RCC_APB1RSTR_I2C1RST            (1U << RCC_APB1RSTR_I2C1RST_BIT)
#define RCC_APB1RSTR_UART5RST           (1U << RCC_APB1RSTR_UART5RST_BIT)
#define RCC_APB1RSTR_UART4RST           (1U << RCC_APB1RSTR_UART4RST_BIT)
#define RCC_APB1RSTR_USART3RST          (1U << RCC_APB1RSTR_USART3RST_BIT)
#define RCC_APB1RSTR_USART2RST          (1U << RCC_APB1RSTR_USART2RST_BIT)
#define RCC_APB1RSTR_SPI3RST            (1U << RCC_APB1RSTR_SPI3RST_BIT)
#define RCC_APB1RSTR_SPI2RST            (1U << RCC_APB1RSTR_SPI2RST_BIT)
#define RCC_APB1RSTR_WWDRST             (1U << RCC_APB1RSTR_WWDRST_BIT)
#define RCC_APB1RSTR_TIM14RST           (1U << RCC_APB1RSTR_TIM14RST_BIT)
#define RCC_APB1RSTR_TIM13RST           (1U << RCC_APB1RSTR_TIM13RST_BIT)
#define RCC_APB1RSTR_TIM12RST           (1U << RCC_APB1RSTR_TIM12RST_BIT)
#define RCC_APB1RSTR_TIM7RST            (1U << RCC_APB1RSTR_TIM7RST_BIT)
#define RCC_APB1RSTR_TIM6RST            (1U << RCC_APB1RSTR_TIM6RST_BIT)
#define RCC_APB1RSTR_TIM5RST            (1U << RCC_APB1RSTR_TIM5RST_BIT)
#define RCC_APB1RSTR_TIM4RST            (1U << RCC_APB1RSTR_TIM4RST_BIT)
#define RCC_APB1RSTR_TIM3RST            (1U << RCC_APB1RSTR_TIM3RST_BIT)
#define RCC_APB1RSTR_TIM2RST            (1U << RCC_APB1RSTR_TIM2RST_BIT)

#define RCC_AHBENR_OFFSET 0x14
#define RCC_AHBENR_DMA1EN_BIT 1
#define RCC_AHBENR_SRAMEN_BIT 4
#define RCC_AHBENR_FLITFEN_BIT 16
#define RCC_AHBENR_CRCEN_BIT 64

#define RCC_APB2ENR_OFFSET 0x18
#define RCC_APB2ENR_ADC3EN_BIT   15
#define RCC_APB2ENR_USART1EN_BIT 14
#define RCC_APB2ENR_TIM8EN_BIT   13
#define RCC_APB2ENR_SPI1EN_BIT   12
#define RCC_APB2ENR_TIM1EN_BIT   11
#define RCC_APB2ENR_ADC2EN_BIT   10
#define RCC_APB2ENR_ADC1EN_BIT   9
#define RCC_APB2ENR_IOPGEN_BIT   8
#define RCC_APB2ENR_IOPFEN_BIT   7
#define RCC_APB2ENR_IOPEEN_BIT   6
#define RCC_APB2ENR_IOPDEN_BIT   5
#define RCC_APB2ENR_IOPCEN_BIT   4
#define RCC_APB2ENR_IOPBEN_BIT   3
#define RCC_APB2ENR_IOPAEN_BIT   2
#define RCC_APB2ENR_AFIOEN_BIT   0

#define RCC_APB1ENR_OFFSET 0x1c
#define RCC_APB1ENR_DACEN_BIT    29
#define RCC_APB1ENR_PWREN_BIT    28
#define RCC_APB1ENR_BKPEN_BIT    27
#define RCC_APB1ENR_CAN2EN_BIT   26
#define RCC_APB1ENR_CAN1EN_BIT   25
#define RCC_APB1ENR_USBEN_BIT    23
#define RCC_APB1ENR_I2C2EN_BIT   22
#define RCC_APB1ENR_I2C1EN_BIT   21
#define RCC_APB1ENR_USART5EN_BIT 20
#define RCC_APB1ENR_USART4EN_BIT 19
#define RCC_APB1ENR_USART3EN_BIT 18
#define RCC_APB1ENR_USART2EN_BIT 17
#define RCC_APB1ENR_SPI3EN_BIT   15
#define RCC_APB1ENR_SPI2EN_BIT   14
#define RCC_APB1ENR_WWDGEN_BIT   11
#define RCC_APB1ENR_TIM7EN_BIT   5
#define RCC_APB1ENR_TIM6EN_BIT   4
#define RCC_APB1ENR_TIM5EN_BIT   3
#define RCC_APB1ENR_TIM4EN_BIT   2
#define RCC_APB1ENR_TIM3EN_BIT   1
#define RCC_APB1ENR_TIM2EN_BIT   0

#define RCC_BDCR_OFFSET 0x20
#define RCC_BDCR_RTCEN_BIT 15
#define RCC_BDCR_RTCSEL_START 8
#define RCC_BDCR_RTCSEL_MASK 0x00000300
#define RCC_BDCR_LSERDY_BIT 1
#define RCC_BDCR_LSEON_BIT 0

#define RCC_CSR_OFFSET 0x24
#define RCC_CSR_LPWRRSTF_BIT	31
#define RCC_CSR_WWDGRSTF_BIT	30
#define RCC_CSR_IWDGRSTF_BIT	29
#define RCC_CSR_LSIRDY_BIT 	1
#define RCC_CSR_LSION_BIT 	0

#define RCC_AHBRSTR 0x28

#define RCC_CFGR2_OFFSET 0x2c
#define RCC_CFGR2_I2S3SRC_BIT    18
#define RCC_CFGR2_I2S2SRC_BIT    17
#define RCC_CFGR2_PREDIV1SRC_BIT 16
#define RCC_CFGR2_PLL3MUL_START  12
#define RCC_CFGR2_PLL3MUL_MASK   0x0000f000
#define RCC_CFGR2_PLL2MUL_START  8
#define RCC_CFGR2_PLL2MUL_MASK   0x00000f00
#define RCC_CFGR2_PREDIV2_START  4
#define RCC_CFGR2_PREDIV2_MASK   0x000000f0
#define RCC_CFGR2_PREDIV_START   0
#define RCC_CFGR2_PREDIV_MASK    0x0000000f
#define RCC_CFGR2_PLLXTPRE_BIT   0

#define PLLSRC_HSI_SELECTED 0
#define PLLSRC_HSE_SELECTED 1

#define SW_HSI_SELECTED 0
#define SW_HSE_SELECTED 1
#define SW_PLL_SELECTED 2

struct Stm32Rcc {
    /* Inherited */
    SysBusDevice busdev;

    /* Properties */
    uint32_t osc_freq;
    uint32_t osc32_freq;

    Clock *sysclk;

    /* Private */
    MemoryRegion iomem;

    /* Register Values */
    uint32_t
        RCC_APB1ENR,
        RCC_APB2ENR,
        RCC_AHBENR;

    /* Register Field Values */
    uint32_t
        RCC_CFGR_PLLMUL,
        RCC_CFGR_PLLXTPRE,
        RCC_CFGR_PLLSRC,
        RCC_CFGR_PPRE1,
        RCC_CFGR_PPRE2,
        RCC_CFGR_HPRE,
        RCC_CFGR_SW,
        RTC_SEL;

    /* Bit CSR register values */
    bool
	RCC_CSR_LPWRRSTFb,
	RCC_CSR_WWDGRSTFb,
	RCC_CSR_IWDGRSTFb;

    Clk
        HSICLK,
        HSECLK,
        LSECLK,
        LSICLK,
        SYSCLK,
        PLLXTPRECLK,
        PLLCLK,
        HCLK, /* Output from AHB Prescaler */
        PCLK1, /* Output from APB1 Prescaler */
        PCLK2, /* Output from APB2 Prescaler */
        HSE_DIV128,
        PERIPHCLK[STM32_PERIPH_COUNT];

    qemu_irq irq;
};



/* HELPER FUNCTIONS */

/* Enable the peripheral clock if the specified bit is set in the value. */
static void stm32_rcc_periph_enable(
                    Stm32Rcc *s,
                    uint32_t new_value,
                    bool init,
                    int periph,
                    uint32_t bit_pos)
{
    clktree_set_enabled(s->PERIPHCLK[periph], new_value & BIT(bit_pos));
}





/* REGISTER IMPLEMENTATION */

/* Read the configuration register. */
static uint32_t stm32_rcc_RCC_CR_read(Stm32Rcc *s)
{
    /* Get the status of the clocks. */
    int pllon_bit = clktree_is_enabled(s->PLLCLK) ? 1 : 0;
    int hseon_bit = clktree_is_enabled(s->HSECLK) ? 1 : 0;
    int hsion_bit = clktree_is_enabled(s->HSICLK) ? 1 : 0;

    /* build the register value based on the clock states.  If a clock is on,
     * then its ready bit is always set.
     */
    return pllon_bit << RCC_CR_PLLRDY_BIT |
           pllon_bit << RCC_CR_PLLON_BIT |
           hseon_bit << RCC_CR_HSERDY_BIT |
           hseon_bit << RCC_CR_HSEON_BIT |
           hsion_bit << RCC_CR_HSIRDY_BIT |
           hsion_bit << RCC_CR_HSION_BIT;
}

/* Write the Configuration Register.
 * This updates the states of the corresponding clocks.  The bit values are not
 * saved - when the register is read, its value will be built using the clock
 * states.
 */
static void stm32_rcc_RCC_CR_write(Stm32Rcc *s, uint32_t new_value, bool init)
{
    bool new_pllon, new_hseon, new_hsion;

    new_pllon = new_value & BIT(RCC_CR_PLLON_BIT);
    if((clktree_is_enabled(s->PLLCLK) && !new_pllon) &&
       s->RCC_CFGR_SW == SW_PLL_SELECTED) {
        stm32_hw_warn("PLL cannot be disabled while it is selected as the system clock.");
    }
    clktree_set_enabled(s->PLLCLK, new_pllon);

    new_hseon = new_value & BIT(RCC_CR_HSEON_BIT);
    if((clktree_is_enabled(s->HSECLK) && !new_hseon) &&
       (s->RCC_CFGR_SW == SW_HSE_SELECTED ||
        (s->RCC_CFGR_SW == SW_PLL_SELECTED && s->RCC_CFGR_PLLSRC == PLLSRC_HSE_SELECTED)
       )
      ) {
        stm32_hw_warn("HSE oscillator cannot be disabled while it is driving the system clock.");
    }
    clktree_set_enabled(s->HSECLK, new_hseon);

    new_hsion = new_value & BIT(RCC_CR_HSION_BIT);
    if((clktree_is_enabled(s->HSICLK) && !new_hsion) &&
       (s->RCC_CFGR_SW == SW_HSI_SELECTED ||
        (s->RCC_CFGR_SW == SW_PLL_SELECTED && s->RCC_CFGR_PLLSRC == PLLSRC_HSI_SELECTED)
       )
      ) {
        stm32_hw_warn("HSI oscillator cannot be disabled while it is driving the system clock.");
    }
    clktree_set_enabled(s->HSICLK, new_hsion);
}


static uint32_t stm32_rcc_RCC_CFGR_read(Stm32Rcc *s)
{
    return (s->RCC_CFGR_PLLMUL << RCC_CFGR_PLLMUL_START) |
           (s->RCC_CFGR_PLLXTPRE << RCC_CFGR_PLLXTPRE_BIT) |
           (s->RCC_CFGR_PLLSRC << RCC_CFGR_PLLSRC_BIT) |
           (s->RCC_CFGR_PPRE2 << RCC_CFGR_PPRE2_START) |
           (s->RCC_CFGR_PPRE1 << RCC_CFGR_PPRE1_START) |
           (s->RCC_CFGR_HPRE << RCC_CFGR_HPRE_START) |
           (s->RCC_CFGR_SW << RCC_CFGR_SW_START) |
           (s->RCC_CFGR_SW << RCC_CFGR_SWS_START);
}


static void stm32_rcc_RCC_CFGR_write(Stm32Rcc *s, uint32_t new_value, bool init)
{
    uint32_t new_PLLMUL, new_PLLXTPRE, new_PLLSRC;

    /* PLLMUL */
    new_PLLMUL = extract32(new_value,
                           RCC_CFGR_PLLMUL_START,
                           RCC_CFGR_PLLMUL_LENGTH);
    if(!init) {
          if(clktree_is_enabled(s->PLLCLK) &&
           (new_PLLMUL != s->RCC_CFGR_PLLMUL)) {
               stm32_hw_warn("Can only change PLLMUL while PLL is disabled");
          }
    }
    assert(new_PLLMUL <= 0xf);
    if(new_PLLMUL == 0xf) {
        clktree_set_scale(s->PLLCLK, 16, 1);
    } else {
        clktree_set_scale(s->PLLCLK, new_PLLMUL + 2, 1);
    }
    s->RCC_CFGR_PLLMUL = new_PLLMUL;

    /* PLLXTPRE */
    new_PLLXTPRE = extract32(new_value, RCC_CFGR_PLLXTPRE_BIT, 1);
    if(!init) {
        if(clktree_is_enabled(s->PLLCLK) &&
           (new_PLLXTPRE != s->RCC_CFGR_PLLXTPRE)) {
            stm32_hw_warn("Can only change PLLXTPRE while PLL is disabled");
        }
    }
    clktree_set_selected_input(s->PLLXTPRECLK, new_PLLXTPRE);
    s->RCC_CFGR_PLLXTPRE = new_PLLXTPRE;

    /* PLLSRC */
    new_PLLSRC = extract32(new_value, RCC_CFGR_PLLSRC_BIT, 1);
    if(!init) {
        if(clktree_is_enabled(s->PLLCLK) &&
           (new_PLLSRC != s->RCC_CFGR_PLLSRC)) {
            stm32_hw_warn("Can only change PLLSRC while PLL is disabled");
        }
    }
    clktree_set_selected_input(s->PLLCLK, new_PLLSRC);
    s->RCC_CFGR_PLLSRC = new_PLLSRC;

    /* PPRE2 */
    s->RCC_CFGR_PPRE2 = (new_value & RCC_CFGR_PPRE2_MASK) >> RCC_CFGR_PPRE2_START;
    if(s->RCC_CFGR_PPRE2 < 0x4) {
        clktree_set_scale(s->PCLK2, 1, 1);
    } else {
        clktree_set_scale(s->PCLK2, 1, 2 * (s->RCC_CFGR_PPRE2 - 3));
    }

    /* PPRE1 */
    s->RCC_CFGR_PPRE1 = (new_value & RCC_CFGR_PPRE1_MASK) >> RCC_CFGR_PPRE1_START;
    if(s->RCC_CFGR_PPRE1 < 4) {
        clktree_set_scale(s->PCLK1, 1, 1);
    } else {
        clktree_set_scale(s->PCLK1, 1, 2 * (s->RCC_CFGR_PPRE1 - 3));
    }

    /* HPRE */
    s->RCC_CFGR_HPRE = (new_value & RCC_CFGR_HPRE_MASK) >> RCC_CFGR_HPRE_START;
    if(s->RCC_CFGR_HPRE < 8) {
        clktree_set_scale(s->HCLK, 1, 1);
    } else {
        clktree_set_scale(s->HCLK, 1, 2 * (s->RCC_CFGR_HPRE - 7));
    }

    /* SW */
    s->RCC_CFGR_SW = (new_value & RCC_CFGR_SW_MASK) >> RCC_CFGR_SW_START;
    switch(s->RCC_CFGR_SW) {
        case 0x0:
        case 0x1:
        case 0x2:
            clktree_set_selected_input(s->SYSCLK, s->RCC_CFGR_SW);
            break;
        default:
            hw_error("Invalid input selected for SYSCLK");
            break;
    }
}

/* Write the APB2 peripheral clock enable register
 * Enables/Disables the peripheral clocks based on each bit. */
static void stm32_rcc_RCC_APB2ENR_write(Stm32Rcc *s, uint32_t new_value,
                                        bool init)
{
    stm32_rcc_periph_enable(s, new_value, init, STM32_ADC1,
                            RCC_APB2ENR_ADC1EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_ADC2,
                            RCC_APB2ENR_ADC2EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_UART1,
                            RCC_APB2ENR_USART1EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_GPIOE,
                            RCC_APB2ENR_IOPEEN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_GPIOD,
                            RCC_APB2ENR_IOPDEN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_GPIOC,
                            RCC_APB2ENR_IOPCEN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_GPIOB,
                            RCC_APB2ENR_IOPBEN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_GPIOA,
                            RCC_APB2ENR_IOPAEN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_AFIO_PERIPH,
                            RCC_APB2ENR_AFIOEN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_GPIOG,
                            RCC_APB2ENR_IOPGEN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_GPIOF,
                            RCC_APB2ENR_IOPFEN_BIT);

    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM1,
                            RCC_APB2ENR_TIM1EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM8,
                            RCC_APB2ENR_TIM8EN_BIT);

    s->RCC_APB2ENR = new_value & 0x0000fffd;
}

/* Write the AHBENR peripheral clock enable register
 * Enables/Disables the peripheral clocks based on each bit. */
static void stm32_rcc_RCC_AHBENR_write(Stm32Rcc *s, uint32_t new_value,
                                        bool init)
{
    stm32_rcc_periph_enable(s, new_value, init, STM32_CRC,
                            RCC_AHBENR_CRCEN_BIT);

    stm32_rcc_periph_enable(s, new_value, init, STM32_DMA1,
                            RCC_AHBENR_DMA1EN_BIT);

    s->RCC_AHBENR = new_value & 0x0000fffd;
}

/* Write the APB1 peripheral clock enable register
 * Enables/Disables the peripheral clocks based on each bit. */
static void stm32_rcc_RCC_APB1ENR_write(Stm32Rcc *s, uint32_t new_value,
                    bool init)
{
    stm32_rcc_periph_enable(s, new_value, init, STM32_UART5,
                            RCC_APB1ENR_USART5EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_UART4,
                            RCC_APB1ENR_USART4EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_UART3,
                            RCC_APB1ENR_USART3EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_UART2,
                            RCC_APB1ENR_USART2EN_BIT);

    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM2,
                            RCC_APB1ENR_TIM2EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM3,
                            RCC_APB1ENR_TIM3EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM4,
                            RCC_APB1ENR_TIM4EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM5,
                            RCC_APB1ENR_TIM5EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM6,
                            RCC_APB1ENR_TIM6EN_BIT);
    stm32_rcc_periph_enable(s, new_value, init, STM32_TIM7,
                            RCC_APB1ENR_TIM7EN_BIT);

    s->RCC_APB1ENR = new_value & 0x00005e7d;
}

static uint32_t stm32_rcc_RCC_BDCR_read(Stm32Rcc *s)
{
    int lseon_bit = clktree_is_enabled(s->LSECLK) ? 1 : 0;
    int RTCEN_bit = clktree_is_enabled(s->PERIPHCLK[STM32_RTC]) ? 1 : 0;
    return lseon_bit << RCC_BDCR_LSERDY_BIT |
           lseon_bit << RCC_BDCR_LSEON_BIT  |
           s->RTC_SEL << RCC_BDCR_RTCSEL_START | 
           RTCEN_bit  << RCC_BDCR_RTCEN_BIT;
}

static void stm32_rcc_RCC_BDCR_write(Stm32Rcc *s, uint32_t new_value, bool init)
{
     clktree_set_enabled(s->LSECLK,new_value & BIT(RCC_BDCR_LSEON_BIT));    
    /* select input CLOCK for RTC  */
    /* RTCSEL =(0,1,2,3) => intput CLK=(0,LSE,LSI,HSE/128)
       see datasheet page 151*/ 
    s->RTC_SEL=((new_value & RCC_BDCR_RTCSEL_MASK) >> RCC_BDCR_RTCSEL_START);
    clktree_set_selected_input(s->PERIPHCLK[STM32_RTC],s->RTC_SEL-1);
    /* enable RTC CLOCK if RTCEN = 1 and RTCSEL !=0 */ 
    if(s->RTC_SEL){
    clktree_set_enabled(s->PERIPHCLK[STM32_RTC],
                       (new_value >> RCC_BDCR_RTCEN_BIT)&0x01);
    }
}

/* Works the same way as stm32_rcc_RCC_CR_read */
static uint32_t stm32_rcc_RCC_CSR_read(Stm32Rcc *s)
{
    int lseon_bit = clktree_is_enabled(s->LSICLK) ? 1 : 0;
    int iwdg_flag = s->RCC_CSR_IWDGRSTFb;

    return lseon_bit << RCC_CSR_LSIRDY_BIT |
           lseon_bit << RCC_CSR_LSION_BIT |
	   iwdg_flag << RCC_CSR_IWDGRSTF_BIT;
}

/* Works the same way as stm32_rcc_RCC_CR_write */
static void stm32_rcc_RCC_CSR_write(Stm32Rcc *s, uint32_t new_value, bool init)
{
    clktree_set_enabled(s->LSICLK, new_value & BIT(RCC_CSR_LSION_BIT));
}

static uint64_t stm32_rcc_readw(void *opaque, hwaddr offset)
{
    Stm32Rcc *s = (Stm32Rcc *)opaque;

    switch (offset) {
        case RCC_CR_OFFSET:
            return stm32_rcc_RCC_CR_read(s);
        case RCC_CFGR_OFFSET:
            return stm32_rcc_RCC_CFGR_read(s);
        case RCC_CIR_OFFSET:
            return 0;
        case RCC_APB2RSTR_OFFSET:
            STM32_NOT_IMPL_REG(offset, 4);
            return 0;
        case RCC_APB1RSTR_OFFSET:
            STM32_NOT_IMPL_REG(offset, 4);
            return 0;
        case RCC_AHBENR_OFFSET:
            return s->RCC_AHBENR;
        case RCC_APB2ENR_OFFSET:
            return s->RCC_APB2ENR;
        case RCC_APB1ENR_OFFSET:
            return s->RCC_APB1ENR;
        case RCC_BDCR_OFFSET:
            return stm32_rcc_RCC_BDCR_read(s);
        case RCC_CSR_OFFSET:
            return stm32_rcc_RCC_CSR_read(s);
        case RCC_AHBRSTR:
            STM32_NOT_IMPL_REG(offset, 4);
            return 0;
        case RCC_CFGR2_OFFSET:
            STM32_NOT_IMPL_REG(offset, 4);
            return 0;
        default:
            STM32_BAD_REG(offset, 4);
            return 0;
    }
    return 0;
}


static void stm32_rcc_writew(void *opaque, hwaddr offset,
                          uint64_t value)
{
    Stm32Rcc *s = (Stm32Rcc *)opaque;

    switch(offset) {
        case RCC_CR_OFFSET:
            stm32_rcc_RCC_CR_write(s, value, false);
            break;
        case RCC_CFGR_OFFSET:
            stm32_rcc_RCC_CFGR_write(s, value, false);
            break;
        case RCC_CIR_OFFSET:
            /* Allow a write but don't take any action */
            break;
        case RCC_APB2RSTR_OFFSET:
            STM32_NOT_IMPL_REG(offset, 4);
            break;
        case RCC_APB1RSTR_OFFSET:
            STM32_NOT_IMPL_REG(offset, 4);
            break;
        case RCC_AHBENR_OFFSET:
            stm32_rcc_RCC_AHBENR_write(s, value, false);
            break;
        case RCC_APB2ENR_OFFSET:
            stm32_rcc_RCC_APB2ENR_write(s, value, false);
            break;
        case RCC_APB1ENR_OFFSET:
            stm32_rcc_RCC_APB1ENR_write(s, value, false);
            break;
        case RCC_BDCR_OFFSET:
            stm32_rcc_RCC_BDCR_write(s, value, false);
            break;
        case RCC_CSR_OFFSET:
            stm32_rcc_RCC_CSR_write(s, value, false);
            break;
        case RCC_AHBRSTR:
            STM32_NOT_IMPL_REG(offset, 4);
            break;
        case RCC_CFGR2_OFFSET:
            STM32_NOT_IMPL_REG(offset, 4);
            break;
        default:
            STM32_BAD_REG(offset, 4);
            break;
    }
}

static uint64_t stm32_rcc_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    switch(size) {
        case 4:
            return stm32_rcc_readw(opaque, offset);
        default:
            STM32_NOT_IMPL_REG(offset, size);
            return 0;
    }
}

static void stm32_rcc_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    switch(size) {
        case 4:
            stm32_rcc_writew(opaque, offset, value);
            break;
        default:
            STM32_NOT_IMPL_REG(offset, size);
            break;
    }
}

static const MemoryRegionOps stm32_rcc_ops = {
    .read = stm32_rcc_read,
    .write = stm32_rcc_write,
    .endianness = DEVICE_NATIVE_ENDIAN
};


static void stm32_rcc_reset(DeviceState *dev)
{
    Stm32Rcc *s = STM32_RCC(dev);

    stm32_rcc_RCC_CR_write(s, 0x00000083, true);
    stm32_rcc_RCC_CFGR_write(s, 0x00000000, true);
    stm32_rcc_RCC_APB2ENR_write(s, 0x00000000, true);
    stm32_rcc_RCC_APB1ENR_write(s, 0x00000000, true);
    stm32_rcc_RCC_AHBENR_write(s, 0x00000000, true);
    stm32_rcc_RCC_BDCR_write(s, 0x00000000, true);
    stm32_rcc_RCC_CSR_write(s, 0x0c000000, true);
}

/* IRQ handler to handle updates to the HCLK frequency.
 * This updates the SysTick scales. */
static void stm32_rcc_hclk_upd_irq_handler(void *opaque, int n, int level)
{
    Stm32Rcc *s = (Stm32Rcc *)opaque;
    int system_clock_scale;
    uint32_t hclk_freq, ext_ref_freq;

    hclk_freq = clktree_get_output_freq(s->HCLK);

    /* Only update the scales if the frequency is not zero. */
    if(hclk_freq > 0) {
        ext_ref_freq = hclk_freq / 8;

        /* Update the scales - these are the ratio of QEMU clock ticks
         * (which is an unchanging number independent of the CPU frequency) to
         * system/external clock ticks.
         */
        system_clock_scale = NANOSECONDS_PER_SECOND / hclk_freq;
        //external_ref_clock_scale = NANOSECONDS_PER_SECOND / ext_ref_freq;
        clock_set_ns(s->sysclk, system_clock_scale);
        //clock_propagate(s->sysclk);

#ifdef DEBUG_STM32_RCC
    DPRINTF("Cortex SYSTICK frequency set to %lu Hz (scale set to %d).\n",
                (unsigned long)hclk_freq, system_clock_scale);
    DPRINTF("Cortex SYSTICK ext ref frequency set to %lu Hz "
              "\n",
              (unsigned long)ext_ref_freq );
#endif
    }
}







/* PUBLIC FUNCTIONS */

void stm32_rcc_check_periph_clk(Stm32Rcc *s, stm32_periph_t periph)
{
    Clk clk = s->PERIPHCLK[periph];

    assert(clk != NULL);

    if(!clktree_is_enabled(clk)) {
        /* I assume writing to a peripheral register while the peripheral clock
         * is disabled is a bug and give a warning to unsuspecting programmers.
         * When I made this mistake on real hardware the write had no effect.
         */
        stm32_hw_warn("Warning: You are attempting to use the %s peripheral while "
                 "its clock is disabled.\n", stm32_periph_name(periph));
    }
}

void stm32_rcc_set_periph_clk_irq(
        Stm32Rcc *s,
        stm32_periph_t periph,
        qemu_irq periph_irq)
{
    Clk clk = s->PERIPHCLK[periph];
    assert(clk != NULL);
    clktree_adduser(clk, periph_irq);
}

uint32_t stm32_rcc_get_periph_freq(
        Stm32Rcc *s,
        stm32_periph_t periph)
{
    Clk clk;

    clk = s->PERIPHCLK[periph];

    assert(clk != NULL);

    return clktree_get_output_freq(clk);
}

void stm32_RCC_CSR_write(Stm32Rcc *s, uint32_t new_value, bool init)
{
    s->RCC_CSR_LPWRRSTFb = new_value >> RCC_CSR_LPWRRSTF_BIT;
    s->RCC_CSR_WWDGRSTFb = new_value >> RCC_CSR_WWDGRSTF_BIT;
    s->RCC_CSR_IWDGRSTFb = new_value >> RCC_CSR_IWDGRSTF_BIT;
}

/* DEVICE INITIALIZATION */
void stm32_rcc_set_sysclk(Stm32Rcc* rcc, Clock *sysclk)
{
    rcc->sysclk = sysclk;
}
/* Set up the clock tree */
static void stm32_rcc_init_clk(Stm32Rcc *s)
{
    int i;
    qemu_irq *hclk_upd_irq =
            qemu_allocate_irqs(stm32_rcc_hclk_upd_irq_handler, s, 1);
    Clk HSI_DIV2, HSE_DIV2;

    /* Make sure all the peripheral clocks are null initially.
     * This will be used for error checking to make sure
     * an invalid clock is not referenced (not all of the
     * indexes will be used).
     */
    for(i = 0; i < STM32_PERIPH_COUNT; i++) {
        s->PERIPHCLK[i] = NULL;
    }

    /* Initialize clocks */
    /* Source clocks are initially disabled, which represents
     * a disabled oscillator.  Enabling the clock represents
     * turning the clock on.
     */
     printf("osc: %d\n", s->osc_freq);
    s->HSICLK = clktree_create_src_clk("HSI", HSI_FREQ, false);
    s->LSICLK = clktree_create_src_clk("LSI", LSI_FREQ, false);
    s->HSECLK = clktree_create_src_clk("HSE", s->osc_freq, false);
    s->LSECLK = clktree_create_src_clk("LSE", s->osc32_freq, false);

     /* for RTCCLK */
    s->HSE_DIV128 = clktree_create_clk("HSE/128", 1, 128, true, CLKTREE_NO_MAX_FREQ, 0,
                        s->HSECLK, NULL);    

    HSI_DIV2 = clktree_create_clk("HSI/2", 1, 2, true, CLKTREE_NO_MAX_FREQ, 0,
                        s->HSICLK, NULL);
    HSE_DIV2 = clktree_create_clk("HSE/2", 1, 2, true, CLKTREE_NO_MAX_FREQ, 0,
                        s->HSECLK, NULL);

    s->PLLXTPRECLK = clktree_create_clk("PLLXTPRE", 1, 1, true, CLKTREE_NO_MAX_FREQ, CLKTREE_NO_INPUT,
                        s->HSECLK, HSE_DIV2, NULL);
    /* PLLCLK contains both the switch and the multiplier, which are shown as
     * two separate components in the clock tree diagram.
     */
    s->PLLCLK = clktree_create_clk("PLLCLK", 0, 1, false, 72000000, CLKTREE_NO_INPUT,
                        HSI_DIV2, s->PLLXTPRECLK, NULL);

    s->SYSCLK = clktree_create_clk("SYSCLK", 1, 1, true, 72000000, CLKTREE_NO_INPUT,
                        s->HSICLK, s->HSECLK, s->PLLCLK, NULL);

    s->HCLK = clktree_create_clk("HCLK", 0, 1, true, 72000000, 0,
                        s->SYSCLK, NULL);
    clktree_adduser(s->HCLK, hclk_upd_irq[0]);

    s->PCLK1 = clktree_create_clk("PCLK1", 0, 1, true, 36000000, 0,
                        s->HCLK, NULL);
    s->PCLK2 = clktree_create_clk("PCLK2", 0, 1, true, 72000000, 0,
                        s->HCLK, NULL);

    /* Peripheral clocks */
    s->PERIPHCLK[STM32_GPIOA] = clktree_create_clk("GPIOA", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_GPIOB] = clktree_create_clk("GPIOB", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_GPIOC] = clktree_create_clk("GPIOC", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_GPIOD] = clktree_create_clk("GPIOD", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_GPIOE] = clktree_create_clk("GPIOE", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_GPIOF] = clktree_create_clk("GPIOF", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_GPIOG] = clktree_create_clk("GPIOG", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);

    s->PERIPHCLK[STM32_AFIO_PERIPH] = clktree_create_clk("AFIO", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);

    s->PERIPHCLK[STM32_UART1] = clktree_create_clk("UART1", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_UART2] = clktree_create_clk("UART2", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_UART3] = clktree_create_clk("UART3", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_UART4] = clktree_create_clk("UART4", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_UART5] = clktree_create_clk("UART5", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);

    s->PERIPHCLK[STM32_TIM1] = clktree_create_clk("TIM1", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_TIM2] = clktree_create_clk("TIM2", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_TIM3] = clktree_create_clk("TIM3", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_TIM4] = clktree_create_clk("TIM4", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_TIM5] = clktree_create_clk("TIM5", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_TIM6] = clktree_create_clk("TIM6", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_TIM7] = clktree_create_clk("TIM7", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_TIM8] = clktree_create_clk("TIM8", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_ADC1] = clktree_create_clk("ADC1", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_ADC2] = clktree_create_clk("ADC2", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK2, NULL);
    s->PERIPHCLK[STM32_RTC]  = clktree_create_clk("RTC", 1, 1, false, CLKTREE_NO_MAX_FREQ,-1,
                              s->LSECLK,s->LSICLK,s->HSE_DIV128, NULL);
    s->PERIPHCLK[STM32_DAC]  = clktree_create_clk("DAC", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->PCLK1, NULL);
    s->PERIPHCLK[STM32_CRC]  = clktree_create_clk("CRC", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->HCLK, NULL);
    s->PERIPHCLK[STM32_DMA1]  = clktree_create_clk("DMA1", 1, 1, false, CLKTREE_NO_MAX_FREQ, 0, s->HCLK, NULL);
}






static void stm32_rcc_init(Object *obj)
{
    SysBusDevice *dev= SYS_BUS_DEVICE(obj);
    Stm32Rcc *s = STM32_RCC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &stm32_rcc_ops, s,
                          "rcc", 0x3FF);

    sysbus_init_mmio(dev, &s->iomem);

    sysbus_init_irq(dev, &s->irq);

    //stm32_rcc_init_clk(s);

    return ;
}

static void stm32_rcc_realize(DeviceState *dev, Error **errp)
{
    Stm32Rcc *s = STM32_RCC(dev);
    
    
    stm32_rcc_init_clk(s);
}


static Property stm32_rcc_properties[] = {
    DEFINE_PROP_UINT32("osc_freq", Stm32Rcc, osc_freq, 0),
    DEFINE_PROP_UINT32("osc32_freq", Stm32Rcc, osc32_freq, 0),
    DEFINE_PROP_END_OF_LIST()
};


static void stm32_rcc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    //SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    dc->reset = stm32_rcc_reset;
    dc->realize = stm32_rcc_realize;
    device_class_set_props(dc, stm32_rcc_properties);
}

static TypeInfo stm32_rcc_info = {
    .name  = "stm32-rcc",
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(Stm32Rcc),
    .instance_init = stm32_rcc_init,
    .class_init = stm32_rcc_class_init
};

static void stm32_rcc_register_types(void)
{
    type_register_static(&stm32_rcc_info);
}

type_init(stm32_rcc_register_types)
