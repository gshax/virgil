/*
 * DDR3 init for DVF101 / Hynix H5TQ2G63DFR-H9C
 *
 * register values reverse engineered from HT818 bootastic
 * (bootrom_bootastic:bootastic_dram_init @ 0x08000024)
 *
 * hardware: Synopsys Denali DDR PHY @ 0x07200000
 *           DDR controller          @ 0x07100000
 *           CMU peripheral CTRL     @ 0x05302000
 *
 * DDR config: DDR3-1066, 2Gbit (256MB), x16, BL8, CL7, 533 MHz
 */

#include <virgil/common.h>
#include <virgil/hw.h>
#include <virgil/ddr.h>
#include <virgil/uart.h>

#define DDR_PHY_BASE    0x07200000
#define DDR_CTL_BASE    0x07100000
#define CMU_PERIPH_BASE 0x05302000

/* CMU peripheral CTRL register offsets (from CMU_BASE + 0x2000) */
#define CMU_P_DDR3_CTRL  0xe8
#define CMU_P_LCDC_CTRL  0x5c
#define CMU_P_SDIO_CTRL  0xec
#define CMU_P_EMMC_CTRL  0xf4
#define CMU_P_ETH_CTRL   0xe0
#define CMU_P_GPU_CTRL   0xd8

/* PLL4 register offsets (no defines in hw.h since it's DDR-specific) */
#define CMU_PLL4_UNIT_CTRL  0x103c
#define CMU_PLL4_CFG1       0x1048
#define CMU_PLL4_CFG2       0x104c

/*
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

/* ---- PLL setup ---- */

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

/*
 * configure PLL2 (DDR data clock) and PLL4 (DDR PHY reference).
 *
 * the bootrom locks both PLLs but leaves their outputs gated and at
 * default frequencies. we reprogram CFG1/CFG2 to the correct DDR
 * frequencies and ungate.
 *
 * PLL1 (CPU clock) is left untouched — changing it would break the
 * bootrom's UART baud rate.
 *
 * values from running HT818 u-boot (stock bootastic cmu_setup output).
 */
static void ddr_pll_init(void)
{
	/* PLL2: DDR data clock */
	pll_reprogram(DVF101_CMU_BASE + CMU_PLL2_UNIT_CTRL,
		      DVF101_CMU_BASE + CMU_PLL2_CFG1,
		      DVF101_CMU_BASE + CMU_PLL2_CFG2,
		      0x0100000d, 0x1102de19);

	/* PLL4: DDR PHY reference clock */
	pll4_reprogram(0x0100000d, 0x12042519);
}

/* ---- DDR clock gating ---- */

/*
 * enable DDR clock domains and AHB clocks for DDR bus masters.
 * reproduced from bootastic FUN_08000da4.
 */
static void ddr_clock_init(void)
{
	unsigned long reg;

	/* enable DDR3 clock domains: bit 12 first, then bits 8-12 */
	reg = readl(CMU_PERIPH_BASE + CMU_P_DDR3_CTRL);
	writel(reg | 0x1000, CMU_PERIPH_BASE + CMU_P_DDR3_CTRL);
	writel(reg | 0x1f00, CMU_PERIPH_BASE + CMU_P_DDR3_CTRL);

	/* enable AHB clocks for bus masters sharing the DDR bus */
	reg = readl(CMU_PERIPH_BASE + CMU_P_LCDC_CTRL);
	writel(reg | 0x20, CMU_PERIPH_BASE + CMU_P_LCDC_CTRL);

	reg = readl(CMU_PERIPH_BASE + CMU_P_SDIO_CTRL);
	writel(reg | 0x20, CMU_PERIPH_BASE + CMU_P_SDIO_CTRL);

	reg = readl(CMU_PERIPH_BASE + CMU_P_EMMC_CTRL);
	writel(reg | 0x20, CMU_PERIPH_BASE + CMU_P_EMMC_CTRL);

	reg = readl(CMU_PERIPH_BASE + CMU_P_ETH_CTRL);
	writel(reg | 0x20, CMU_PERIPH_BASE + CMU_P_ETH_CTRL);

	reg = readl(CMU_PERIPH_BASE + CMU_P_GPU_CTRL);
	writel(reg | 0x40, CMU_PERIPH_BASE + CMU_P_GPU_CTRL);

	/* clear bit 4 of DDR3_CTRL */
	reg = readl(CMU_PERIPH_BASE + CMU_P_DDR3_CTRL);
	writel((reg & ~0x10) | 0x1f00, CMU_PERIPH_BASE + CMU_P_DDR3_CTRL);
}

/* clear bits 0-3 of DDR3_CTRL after PHY configuration */
static void ddr_clock_finalize(void)
{
	unsigned long reg = readl(CMU_PERIPH_BASE + CMU_P_DDR3_CTRL);
	writel(reg & 0xfffffff0, CMU_PERIPH_BASE + CMU_P_DDR3_CTRL);
}

/* ---- DDR PHY init ---- */

/*
 * configure the DDR PHY.
 *
 * register values from HT818 bootastic for H5TQ2G63DFR-H9C @ 533 MHz.
 * the PHY uses a Synopsys Denali architecture; exact field definitions
 * vary by version but the offsets match the DVF99 denali_setup[] layout.
 */
static void ddr_phy_init(void)
{
	unsigned long zero;

	/* wait for PHY idle (bits 28-29 of status register) */
	waitfor((readl(DDR_PHY_BASE + 0x308) & 0x30000000) == 0);

	/* clear PHY status bits 0-1 */
	do {
		zero = readl(DDR_PHY_BASE + 0x308) & 3;
		writel(zero, DDR_PHY_BASE + 0x060);
	} while (zero != 0);

	/* controller config */
	writel(0x01040001, DDR_PHY_BASE + 0x000);
	writel(0x00020000, DDR_PHY_BASE + 0x034);

	/* timing */
	writel(0x0021f000, DDR_PHY_BASE + 0x050);
	writel(0x0081008b, DDR_PHY_BASE + 0x064);
	writel(0x40010105, DDR_PHY_BASE + 0x0d0);
	writel(0x00690101, DDR_PHY_BASE + 0x0d4);
	writel(0x18400054, DDR_PHY_BASE + 0x0dc);
	writel(0x00080000, DDR_PHY_BASE + 0x0e0);
	writel(0x00100000, DDR_PHY_BASE + 0x0e4);
	writel(0x151b2414, DDR_PHY_BASE + 0x100);
	writel(0x00040a1c, DDR_PHY_BASE + 0x104);
	writel(0x0000050e, DDR_PHY_BASE + 0x108);
	writel(0x0000400c, DDR_PHY_BASE + 0x10c);
	writel(0x02040608, DDR_PHY_BASE + 0x110);
	writel(0x06060403, DDR_PHY_BASE + 0x114);
	writel(0x00001005, DDR_PHY_BASE + 0x120);

	/* mode registers / ODT */
	writel(0x01000040, DDR_PHY_BASE + 0x180);
	writel(0x00021e32, DDR_PHY_BASE + 0x184);

	/* address / bank config */
	writel(0x02060005, DDR_PHY_BASE + 0x190);
	writel(0x00020202, DDR_PHY_BASE + 0x194);
	writel(0x07000080, DDR_PHY_BASE + 0x198);
	writel(0x20400004, DDR_PHY_BASE + 0x1a0);
	writel(0x0037006e, DDR_PHY_BASE + 0x1a4);

	/* DQ / DQS */
	writel(0x00001f1f, DDR_PHY_BASE + 0x200);
	writel(0x00080808, DDR_PHY_BASE + 0x204);
	writel(0x07070707, DDR_PHY_BASE + 0x214);
	writel(0x0f070707, DDR_PHY_BASE + 0x218);

	/* PHY calibration */
	writel(0x06000680, DDR_PHY_BASE + 0x240);
	writel(0x00000011, DDR_PHY_BASE + 0x244);
	writel(0x0b071a01, DDR_PHY_BASE + 0x250);
	writel(0x24004b37, DDR_PHY_BASE + 0x25c);
	writel(0xf900cf1f, DDR_PHY_BASE + 0x264);
	writel(0x75000fb2, DDR_PHY_BASE + 0x26c);
	writel(0x00000011, DDR_PHY_BASE + 0x274);
	writel(0x0000000a, DDR_PHY_BASE + 0x278);

	/* per-slice PHY training data */
	writel(0x00010011, DDR_PHY_BASE + 0x36c);
	writel(0x00001085, DDR_PHY_BASE + 0x404);
	writel(0x000050ae, DDR_PHY_BASE + 0x408);
	writel(0x00004122, DDR_PHY_BASE + 0x4b4);
	writel(0x00003280, DDR_PHY_BASE + 0x4b8);
	writel(0x000001c8, DDR_PHY_BASE + 0x564);
	writel(0x00003370, DDR_PHY_BASE + 0x568);
	writel(0x00000001, DDR_PHY_BASE + 0x56c);
	writel(0x0000724f, DDR_PHY_BASE + 0x614);
	writel(0x000030a5, DDR_PHY_BASE + 0x618);
	writel(0x000140c3, DDR_PHY_BASE + 0x6c4);
	writel(0x000072df, DDR_PHY_BASE + 0x6c8);
	writel(0x00110000, DDR_PHY_BASE + 0x754);
	writel(0x00000001, DDR_PHY_BASE + 0x75c);
	writel(0x00000086, DDR_PHY_BASE + 0x760);
	writel(0x0001022f, DDR_PHY_BASE + 0x774);
	writel(0x00003131, DDR_PHY_BASE + 0x778);
	writel(0x00000001, DDR_PHY_BASE + 0x804);
	writel(0x013302d0, DDR_PHY_BASE + 0x808);
	writel(0x00000377, DDR_PHY_BASE + 0x810);
	writel(0x00015263, DDR_PHY_BASE + 0x824);
	writel(0x0000124f, DDR_PHY_BASE + 0x828);
	writel(0x00000001, DDR_PHY_BASE + 0x8b4);
	writel(0x00150583, DDR_PHY_BASE + 0x8b8);
	writel(0x00000005, DDR_PHY_BASE + 0x8bc);
	writel(0x00000500, DDR_PHY_BASE + 0x8c0);
	writel(0x0000604d, DDR_PHY_BASE + 0x8d4);
	writel(0x000011c5, DDR_PHY_BASE + 0x8d8);
	writel(0x00801500, DDR_PHY_BASE + 0x968);
	writel(0x00000005, DDR_PHY_BASE + 0x96c);
	writel(0x000006af, DDR_PHY_BASE + 0x970);

	/* enable per-slice PHY */
	writel(1, DDR_PHY_BASE + 0x490);
	writel(1, DDR_PHY_BASE + 0x540);
	writel(1, DDR_PHY_BASE + 0x5f0);
	writel(1, DDR_PHY_BASE + 0x6a0);
	writel(1, DDR_PHY_BASE + 0x750);
	writel(1, DDR_PHY_BASE + 0x800);
	writel(1, DDR_PHY_BASE + 0x8b0);
	writel(1, DDR_PHY_BASE + 0x960);

	/* final PHY config */
	writel(0x00000008, DDR_PHY_BASE + 0x030);

	/* clear status registers */
	writel(zero, DDR_PHY_BASE + 0x0c0);
	writel(zero, DDR_PHY_BASE + 0x0f0);
	writel(zero, DDR_PHY_BASE + 0x0f4);
	writel(zero, DDR_PHY_BASE + 0x1a8);
	writel(zero, DDR_PHY_BASE + 0x1b0);
	writel(zero, DDR_PHY_BASE + 0x208);
	writel(zero, DDR_PHY_BASE + 0x20c);
	writel(zero, DDR_PHY_BASE + 0x304);
	writel(zero, DDR_PHY_BASE + 0x30c);
	writel(zero, DDR_PHY_BASE + 0x320);
	writel(zero, DDR_PHY_BASE + 0x758);
	writel(zero, DDR_PHY_BASE + 0x80c);
	writel(zero, DDR_PHY_BASE + 0x964);
}

/* ---- DDR controller init ---- */

static void ddr_ctl_init(void)
{
	writel(0x00842e02, DDR_CTL_BASE + 0x008);
	writel(0x7ffffff0, DDR_CTL_BASE + 0x054);
	writel(0x0000000b, DDR_CTL_BASE + 0x030);
	writel(0x0022aa5b, DDR_CTL_BASE + 0x018);
	writel(0x04841104, DDR_CTL_BASE + 0x01c);
	writel(0x040168a0, DDR_CTL_BASE + 0x020);
	writel(0x00001840, DDR_CTL_BASE + 0x040);
	writel(0x00000054, DDR_CTL_BASE + 0x044);
	writel(0x00000008, DDR_CTL_BASE + 0x048);
	writel(0x38d48890, DDR_CTL_BASE + 0x034);
	writel(0x008b00d8, DDR_CTL_BASE + 0x038);
	writel(0x10023600, DDR_CTL_BASE + 0x03c);
	writel(0xf200181f, DDR_CTL_BASE + 0x02c);
	writel(0x000101ff, DDR_CTL_BASE + 0x004);

	/* wait for controller init complete */
	waitfor((readl(DDR_CTL_BASE + 0x00c) & 0x1f) == 0x1f);

	/* trigger PHY training */
	writel(1, DDR_PHY_BASE + 0x1b0);
	writel(1, DDR_PHY_BASE + 0x320);

	/* wait for training complete */
	waitfor(readl(DDR_PHY_BASE + 0x324) & 1);

	/* wait for DDR ready */
	waitfor(readl(DDR_PHY_BASE + 0x004) & 1);
}

/* ---- memory test ---- */

#define DDR_TEST_BASE   DVF_UBOOT_LOAD_ADDR
#define DDR_TEST_WORDS  16

static const unsigned long test_pattern[DDR_TEST_WORDS] = {
	0xdeadbeef, 0x12345678, 0x55aa55aa, 0xaa55aa55,
	0x00ff00ff, 0xff00ff00, 0x0f0f0f0f, 0xf0f0f0f0,
	0x01020304, 0x05060708, 0x090a0b0c, 0x0d0e0f10,
	0xfedcba98, 0x76543210, 0xabcdef01, 0x23456789,
};

static int ddr_memtest(void)
{
	volatile unsigned long *ddr = (volatile unsigned long *)DDR_TEST_BASE;

	writel(0, DDR_CTL_BASE + 0x04c);

	for (int retries = 5; retries > 0; retries--) {
		for (int i = 0; i < DDR_TEST_WORDS; i++)
			ddr[i] = test_pattern[i];

		int ok = 1;
		for (int i = 0; i < DDR_TEST_WORDS; i++) {
			if (ddr[i] != test_pattern[i]) {
				ok = 0;
				break;
			}
		}
		if (ok)
			return 0;
	}

	return -1;
}

/* ---- public API ---- */

int ddr_init(const dspg_dvf101_bootrom_api_t* rom)
{
	if (readl(DDR_PHY_BASE + 0x004) & 1)
		return 0;

	uart_puts("ddr init...");

	uart_puts(" pll,");
	ddr_pll_init();
	uart_puts(" clk,");
	ddr_clock_init();
	uart_puts(" phy,");
	ddr_phy_init();
	ddr_clock_finalize();
	rom->udelay(1000);
	uart_puts(" ctl");
	ddr_ctl_init();

	uart_puts("\nmemtest... ");
	return ddr_memtest();
}
