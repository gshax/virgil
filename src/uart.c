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

#include <gsfw/libc.h>
#include <virgil/common.h>
#include <virgil/hw.h>
#include <virgil/romapi.h>
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
 * compute the UART peripheral clock (pclk).
 *
 *   PLL1 gated:   mainclk (25 MHz) → dividers → pclk = 6.25 MHz
 *   PLL1 active:  PLL1_out (1 GHz) → dividers → pclk = 250 MHz
 *
 * PLL1 output = mainclk × fbdiv / refdiv, where fbdiv and refdiv
 * are in PLL1_CFG2 bits [15:8] and [7:0] respectively.
 * (verified: bootrom CFG2=0x11002801 → 25M × 40 / 1 = 1 GHz,
 *  / AXI_SYS_DIV(4) = 250 MHz, matches u-boot UART dividers.)
 *
 * note: CFG2 readback may differ from what was written (the PLL
 * hardware modifies some fields after lock). the formula is only
 * reliable for the bootrom's original config. we avoid reprogramming
 * PLL1_CFG2 to sidestep this issue.
 */
unsigned long uart_get_pclk(void)
{
	unsigned long gcr0 = readl(DVF101_SYSCFG_BASE + SYSCFG_GCR0);
	unsigned long mainclk = ((gcr0 & 0x30) == 0x10) ? 50000000ul : 25000000ul;

	unsigned long baseclk;
	unsigned long pll1_ctrl = readl(DVF101_CMU_BASE + CMU_PLL1_UNIT_CTRL);

	if (pll1_ctrl & 0x80) {
		/* PLL1 active — compute output from CFG2 */
		unsigned long cfg2 = readl(DVF101_CMU_BASE + CMU_PLL1_CFG2);
		unsigned long fbdiv = (cfg2 >> 8) & 0xff;
		unsigned long refdiv = cfg2 & 0xff;
		if (refdiv == 0) refdiv = 1;
		baseclk = mainclk * fbdiv / refdiv;
	} else {
		/* PLL1 gated — raw oscillator */
		baseclk = mainclk;
	}

	unsigned long post_div = (pll1_ctrl >> 24) & 0x3f;
	unsigned long mcu_div = readl(DVF101_CMU_BASE + CMU_MCU_DIV_VAL) & 0xf;
	unsigned long axi_sys_div = (readl(DVF101_CMU_BASE + CMU_MCU_AXI_DIV_VAL) >> 12) & 0x3f;

	return baseclk / (post_div + 1) / (mcu_div + 1) / (axi_sys_div + 1);
}

void uart_init(unsigned int baudrate)
{
	unsigned long pclk, integer, fraction;

	uart_clk_enable();

	/* if UART is currently active, drain TX FIFO first */
	if (readl(UART_BASE + UART_CTL) & 1) {
		waitfor(readl(UART_BASE + UART_TX_FIFO_LVL) <= 0);
		/* let the shift register finish */
		rom_api->udelay(25000);
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

void uart_putint(int value, int base)
{
	char tmp[12];
	uart_puts(itoa(value, tmp, base));
}

void uart_putdec(int value)
{
	uart_putint(value, 10);
}

void uart_puthex(int value)
{
	uart_putint(value, 16);
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

void uart_exit(void)
{
	if (!(readl(UART_BASE + UART_CTL) & 1))
		return;

	/* drain TX FIFO */
	waitfor(readl(UART_BASE + UART_TX_FIFO_LVL) == 0);

	/* let the shift register finish the last byte */
	rom_api->udelay(25000);

	/* disable UART */
	writel(0, UART_BASE + UART_CTL);
}
