#include "hal.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/rng.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/systick.h>


/* 24 MHz */
const struct rcc_clock_scale benchmarkclock = {
  .pllm = 8, //VCOin = HSE / PLLM = 1 MHz
  .plln = 192, //VCOout = VCOin * PLLN = 192 MHz
  .pllp = 8, //PLLCLK = VCOout / PLLP = 24 MHz (low to have 0WS)
  .pllq = 4, //PLL48CLK = VCOout / PLLQ = 48 MHz (required for USB, RNG)
  .pllr = 0,
  .hpre = RCC_CFGR_HPRE_DIV_NONE,
  .ppre1 = RCC_CFGR_PPRE_DIV_2,
  .ppre2 = RCC_CFGR_PPRE_DIV_NONE,
  .pll_source = RCC_CFGR_PLLSRC_HSE_CLK,
  .voltage_scale = PWR_SCALE1,
  .flash_config = FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_LATENCY_0WS,
  .ahb_frequency = 24000000,
  .apb1_frequency = 12000000,
  .apb2_frequency = 24000000,
};

/* 100 MHz */
const struct rcc_clock_scale fastclock = {
  .pllm = 4, //VCOin = HSE / PLLM = 2 MHz
  .plln = 100, //VCOout = VCOin * PLLN = 200 MHz
  .pllp = 2, //PLLCLK = VCOout / PLLP = 100 MHz (max on STM32F413)
  .pllq = 6, //PLL48CLK = VCOout / PLLQ = 33.3 MHz (USB OTG FS requires exactly 48, <= 48 for RNG)
  .pllr = 0,
  .hpre = RCC_CFGR_HPRE_DIV_NONE,
  .ppre1 = RCC_CFGR_PPRE_DIV_2,
  .ppre2 = RCC_CFGR_PPRE_DIV_NONE,
  .pll_source = RCC_CFGR_PLLSRC_HSE_CLK,
  .voltage_scale = PWR_SCALE1,
  .flash_config = FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_LATENCY_3WS,
  .ahb_frequency = 100000000,
  .apb1_frequency = 50000000,
  .apb2_frequency = 100000000,
};

static void clock_setup(const enum clock_mode clock)
{
  switch(clock)
  {
    case CLOCK_BENCHMARK:
      rcc_clock_setup_pll(&benchmarkclock);
      break;
    case CLOCK_FAST:
    default:
      rcc_clock_setup_pll(&fastclock);
      break;
  }

  rcc_periph_clock_enable(RCC_GPIOG);
  rcc_periph_clock_enable(RCC_USART6);
  rcc_periph_clock_enable(RCC_DMA1);
  rcc_periph_clock_enable(RCC_RNG);

  flash_prefetch_enable();
}

static void gpio_setup(void)
{
  gpio_mode_setup(GPIOG, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO14);
  gpio_set_af(GPIOG, GPIO_AF8, GPIO14);
}
static void usart_setup(int baud)
{
  usart_set_baudrate(USART6, baud);
  usart_set_databits(USART6, 8);
  usart_set_stopbits(USART6, USART_STOPBITS_1);
  usart_set_mode(USART6, USART_MODE_TX);
  usart_set_parity(USART6, USART_PARITY_NONE);
  usart_set_flow_control(USART6, USART_FLOWCONTROL_NONE);

  usart_enable(USART6);
}

static void systick_setup(void)
{
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_set_reload(16777215);
  systick_interrupt_enable();
  systick_counter_enable();
}

static void send_USART_str(const char* in)
{
  int i;
  for(i = 0; in[i] != 0; i++) {
    usart_send_blocking(USART6, *(unsigned char *)(in+i));
  }
  usart_send_blocking(USART6, '\n');
}
void hal_setup(const enum clock_mode clock)
{
  clock_setup(clock);
  gpio_setup();
  usart_setup(115200);
  systick_setup();
  rng_enable();
}
void hal_send_str(const char* in)
{
  send_USART_str(in);
}

static volatile unsigned long long overflowcnt = 0;
void sys_tick_handler(void)
{
  ++overflowcnt;
}
uint64_t hal_get_time()
{
  while (true) {
    unsigned long long before = overflowcnt;
    unsigned long long result = (before + 1) * 16777216llu - systick_get_value();
    if (overflowcnt == before) {
      return result;
    }
  }
}
