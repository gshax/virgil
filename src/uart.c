/*
 * UART driver for DVF101
 *
 * the DSPG UART is a custom (non-16550) design with a 128-byte FIFO.
 * register layout is shared across DVF99/DVF97/DVF101.
 *
 * when entering from bootrom UART xmodem boot, UART1 is already fully
 * initialized at 115200 8N1 -- uart_init() can be called to reconfigure
 * if needed, but uart_putc/puts will work immediately.
 */

#include <virgil/common.h>
#include <virgil/hw.h>
#include <virgil/uart.h>

#define UART_BASE DVF101_UART1_BASE

/*
 * enable the UART1 APB clock via the per-peripheral CMU ctrl register.
 *
 * on DVF101, each peripheral has its own CTRL register in the CMU at
 * offset 0x2000+. for UART, bit 4 gates the APB clock.
 *
 * note: the bootrom already enables this for xmodem boot, so this is
 * mostly needed if we've been through a clock reconfiguration.
 */
static void uart_clk_enable(void)
{
	unsigned long reg = readl(DVF101_CMU_BASE + CMU_UART1_CTRL);
	reg |= CMU_CTRL_APB_CLK_EN;
	writel(reg, DVF101_CMU_BASE + CMU_UART1_CTRL);
}

/*
 * the UART peripheral clock (pclk) on DVF101 derives from the raw
 * oscillator, NOT from PLL1 output. the chain is:
 *
 *   mainclk → PLL1_POST_DIV → MCU_DIV → AXI_SYS_DIV → pclk
 *
 * PLL1 feeds the CPU core but the APB bus divider chain starts from
 * the oscillator input. confirmed by register dump on HT818:
 *   mainclk = 25 MHz, AXI_SYS_DIV = 3+1 = 4, pclk = 6.25 MHz
 *   UART INT_DIV=3 FRAC_DIV=6 → 54 * 115200 = 6,220,800 Hz ✓
 *
 * mainclk selection: SYSCFG_GCR0 bits [5:4].
 *   default (not 0x10) → 25 MHz
 *   0x10              → 50 MHz
 */
static unsigned long uart_get_pclk(void)
{
	unsigned long gcr0 = readl(DVF101_SYSCFG_BASE + SYSCFG_GCR0);
	unsigned long mainclk = ((gcr0 & 0x30) == 0x10) ? 50000000ul : 25000000ul;

	/* PLL1 post-divider (passthrough for peripheral bus) */
	unsigned long pll1_ctrl = readl(DVF101_CMU_BASE + CMU_PLL1_UNIT_CTRL);
	unsigned long post_div = (pll1_ctrl >> 24) & 0x3f;

	/* MCU main divider */
	unsigned long mcu_div = readl(DVF101_CMU_BASE + CMU_MCU_DIV_VAL) & 0xf;

	/* AXI system bus divider */
	unsigned long axi_sys_div = (readl(DVF101_CMU_BASE + CMU_MCU_AXI_DIV_VAL) >> 12) & 0x3f;

	return mainclk / (post_div + 1) / (mcu_div + 1) / (axi_sys_div + 1);
}

void uart_init(unsigned int baudrate)
{
	unsigned long pclk, integer, fraction;

	uart_clk_enable();

	/* if UART is currently active, drain TX FIFO first */
	if (readl(UART_BASE + UART_CTL) & 1) {
		while (readl(UART_BASE + UART_TX_FIFO_LVL) > 0)
			;
		/* let the shift register finish */
		for (volatile int i = 0; i < 10000; i++)
			;
	}

	pclk = uart_get_pclk();

	integer  = (pclk / baudrate) >> 4;
	fraction = (pclk / baudrate) & 0xf;

	writel(0, UART_BASE + UART_CTL);                        /* disable */
	writel(UART_CFG_8N1, UART_BASE + UART_CFG);             /* 8N1 */
	writel(integer, UART_BASE + UART_INT_DIV);
	writel(fraction, UART_BASE + UART_FRAC_DIV);
	writel((1<<12)|(1<<9)|(1<<8)|1, UART_BASE + UART_FIFO_ICR); /* clear IRQs */
	writel(UART_FIFO_SIZE / 2, UART_BASE + UART_TX_FIFO_WM);
	writel(UART_FIFO_SIZE / 2, UART_BASE + UART_RX_FIFO_WM);
	writel(1, UART_BASE + UART_CTL);                        /* enable */
}

void uart_putc(char c)
{
	/* LF to CRLF */
	if (c == '\n') {
		uart_putc('\r');
	}

	/* wait for space */
	waitfor(!(readl(UART_BASE + UART_STAT) & UART_STAT_TX_FULL));
	writel(c, UART_BASE + UART_TX_DATA);
}

void uart_puts(const char *s)
{
	while (*s)
		uart_putc(*s++);
}

int uart_tstc(void)
{
	return !(readl(UART_BASE + UART_STAT) & UART_STAT_RX_EMPTY);
}

char uart_getc(void)
{
	unsigned int data;

	waitfor(uart_tstc());

	data = readl(UART_BASE + UART_RX_DATA);
	if (data & 0x600) /* parity or frame error */
		return -1;

	return data & 0xff;
}

void uart_flush_tx(void)
{
	waitfor(readl(UART_BASE + UART_TX_FIFO_LVL) == 0);
}
