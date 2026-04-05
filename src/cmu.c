/*
 * CMU (clock management unit) setup for DVF101
 *
 * configures PLLs and peripheral clocks to match the stock bootastic
 * cmu_setup output. register values from a running HT818 u-boot.
 *
 * PLL_UNIT_CTRL bit map (shared across PLL1-4):
 *   bit 0  = PRE_DIV_EN
 *   bit 1  = POST_DIV_EN
 *   bit 2  = output gate (1 = gated/disabled)
 *   bit 7  = CLK_IN_USE (read-only, 1 = clock output active)
 *   bit 14 = PRE_DIV_EN_STAT
 *   bit 15 = POST_DIV_EN_STAT
 *
 * PLL4 lacks pre/post dividers and uses a simpler enable sequence.
 */

#include <virgil/common.h>
#include <virgil/hw.h>
#include <virgil/cmu.h>
#include <virgil/uart.h>

/* PLL4 register offsets */
#define CMU_PLL4_UNIT_CTRL  0x103c
#define CMU_PLL4_CFG1       0x1048
#define CMU_PLL4_CFG2       0x104c

/*
 * reprogram a PLL with pre/post dividers (PLL1-3 style).
 * sequence: gate output → write config → enable dividers → ungate → wait.
 */
static void pll_reprogram(unsigned long unit_ctrl, unsigned long cfg1_addr,
			   unsigned long cfg2_addr, unsigned long cfg1,
			   unsigned long cfg2)
{
	unsigned long val;

	/* gate output if PLL is currently active */
	if (readl(unit_ctrl) & 0x80) {
		writel(readl(unit_ctrl) | 4, unit_ctrl);
		waitfor(!(readl(unit_ctrl) & 0x80));
	}

	/* write PLL configuration */
	writel(cfg1, cfg1_addr);
	writel(cfg2, cfg2_addr);

	/* enable pre-divider if needed */
	val = readl(unit_ctrl);
	if (!(val & 0x4000)) {
		writel(val | 1, unit_ctrl);
		waitfor(readl(unit_ctrl) & 0x4000);
		val = readl(unit_ctrl);
	}

	/* enable post-divider if needed */
	if (!(val & 0x8000)) {
		writel(val | 2, unit_ctrl);
		waitfor(readl(unit_ctrl) & 0x8000);
		val = readl(unit_ctrl);
	}

	/* ungate output and wait for clock to go live */
	writel(val & ~4, unit_ctrl);
	waitfor(readl(unit_ctrl) & 0x80);
}

/*
 * reprogram PLL4 (no pre/post dividers — simpler sequence).
 */
static void pll4_reprogram(unsigned long cfg1, unsigned long cfg2)
{
	unsigned long ctrl = DVF101_CMU_BASE + CMU_PLL4_UNIT_CTRL;

	/* gate if active */
	if (readl(ctrl) & 0x80) {
		writel(readl(ctrl) | 4, ctrl);
		waitfor(!(readl(ctrl) & 0x80));
	}

	/* write config, ungate, wait for lock */
	writel(cfg1, DVF101_CMU_BASE + CMU_PLL4_CFG1);
	writel(cfg2, DVF101_CMU_BASE + CMU_PLL4_CFG2);
	writel(readl(ctrl) & ~4, ctrl);
	waitfor(readl(ctrl) & 0x80);
}

void cmu_init(const dspg_dvf101_bootrom_api_t *rom)
{
	/*
	 * match the stock bootastic cmu_setup sequence:
	 *   1. disable UART (drain + off)
	 *   2. notify bootrom: clocks about to change
	 *   3. reprogram PLLs
	 *   4. notify bootrom: clocks changed
	 *   5. reinit UART at new pclk
	 */

	uart_exit();

	/*
	 * notify bootrom before clock change.
	 *
	 * the DVF101 bootastic's cmu_setup calls:
	 *   start: clkchg_post(1, old_pclk),  clkchg_pre(2, 0)
	 *   end:   clkchg_pre(1, new_pclk),   clkchg_post(2, 1)
	 *
	 * arg 1 = pclk notification (bootrom recalibrates timer/UART)
	 * arg 2 = sysclk change signal (0 = starting, 1 = done)
	 */
	unsigned long old_pclk = uart_get_pclk();
	if (rom->clkchg_post)
		rom->clkchg_post(1, old_pclk);
	if (rom->clkchg_pre)
		rom->clkchg_pre(2, 0);

	/*
	 * PLL1: CPU core clock.
	 *
	 * the bootrom already configured PLL1 at 1 GHz (25 MHz × 40)
	 * but left its output gated. we just ungate it — don't change
	 * the frequency config.
	 *
	 * ungating switches pclk from mainclk/dividers (6.25 MHz) to
	 * PLL1_out/dividers (1 GHz / 4 = 250 MHz). uart_init must be
	 * called after to reconfigure baud dividers.
	 *
	 * note: the CFG2 value read from running u-boot (0x11028010)
	 * differs from the bootrom's written value (0x11002801) — this
	 * appears to be a post-lock hardware readback, not a different
	 * config. writing it back would change the PLL frequency.
	 */
	{
		unsigned long ctrl = DVF101_CMU_BASE + CMU_PLL1_UNIT_CTRL;
		unsigned long val = readl(ctrl);
		writel(val & ~4, ctrl);
		waitfor(readl(ctrl) & 0x80);
	}

	/* PLL2: DDR data clock */
	pll_reprogram(DVF101_CMU_BASE + CMU_PLL2_UNIT_CTRL,
		      DVF101_CMU_BASE + CMU_PLL2_CFG1,
		      DVF101_CMU_BASE + CMU_PLL2_CFG2,
		      0x0100000d, 0x1102de19);

	/* PLL4: DDR PHY reference clock */
	pll4_reprogram(0x0100000d, 0x12042519);

	/* notify bootrom after clock change */
	unsigned long new_pclk = uart_get_pclk();
	if (rom->clkchg_pre)
		rom->clkchg_pre(1, new_pclk);
	if (rom->clkchg_post)
		rom->clkchg_post(2, 1);

	/* reinit UART — pclk changed from ~6.25 MHz to ~250 MHz */
	uart_init(115200);
}
