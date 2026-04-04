#pragma once

/*
 * DVF101 hardware register definitions
 *
 * base addresses and peripheral register offsets for the DSPG DVF101 SoC
 * as used in the Grandstream HT8xx series.
 */

/* peripheral base addresses */
#define DVF101_CMU_BASE         0x05300000
#define DVF101_SYSCFG_BASE      0x05200000
#define DVF101_GPIO_BASE        0x05000000
#define DVF101_UART1_BASE       0x05a00000
#define DVF101_UART2_BASE       0x05b00000
#define DVF101_UART3_BASE       0x05c00000
#define DVF101_UART4_BASE       0x06800000

/* ---------- CMU (clock management unit) ---------- */

/* per-peripheral clock/reset control registers (offset from CMU_BASE) */
#define CMU_UART1_CTRL          0x202c
#define CMU_UART2_CTRL          0x2030
#define CMU_UART3_CTRL          0x2034
#define CMU_UART4_CTRL          0x2038

/* clock ctrl register bits (common layout for leaf peripherals) */
#define CMU_CTRL_APB_CLK_EN     (1 << 4)

/* legacy clock enable register (still functional on DVF101) */
#define CMU_SWCLKEN1            0x0058

/* PLL registers — PLL_BASE(n) = 0x1000 + (n-1) * 0x14 */
#define CMU_PLL1_UNIT_CTRL      0x1000
#define CMU_PLL1_CFG1           0x100c
#define CMU_PLL1_CFG2           0x1010
#define CMU_PLL2_UNIT_CTRL      0x1014
#define CMU_PLL2_RST_CNT        0x1018
#define CMU_PLL2_LOCK_CNT       0x101c
#define CMU_PLL2_CFG1           0x1020
#define CMU_PLL2_CFG2           0x1024

/* MCU clock control */
#define CMU_MCU_CTRL            0x2000
#define CMU_MCU_DIV_VAL         0x2004
#define CMU_MCU_AXI_DIV_VAL     0x2008

/* reset/status */
#define CMU_RST_CTRL            0x0000

/* ---------- SYSCFG ---------- */

#define SYSCFG_GCR0             0x00
#define SYSCFG_GCR1             0x04
#define SYSCFG_CHIP_ID          0x1c
#define SYSCFG_CHIP_REV         0x20

/* IO mux registers */
#define SYSCFG_IOM1             0x200
#define SYSCFG_IOM2             0x204
#define SYSCFG_IOM3             0x208
#define SYSCFG_IOM4             0x20c
#define SYSCFG_IOM5             0x210
#define SYSCFG_IOM6             0x214
#define SYSCFG_IOM7             0x218
#define SYSCFG_IOM8             0x21c
#define SYSCFG_IOM9             0x220
#define SYSCFG_IOM10            0x224
#define SYSCFG_IOM11            0x228

/* ---------- UART ---------- */

#define UART_CTL                0x00
#define UART_CFG                0x04
#define UART_INT_DIV            0x08
#define UART_FRAC_DIV           0x0c
#define UART_TX_FIFO_WM         0x10
#define UART_RX_FIFO_WM         0x14
#define UART_FIFO_ISR           0x18
#define UART_FIFO_IER           0x1c
#define UART_FIFO_ICR           0x20
#define UART_TX_FIFO_LVL        0x24
#define UART_RX_FIFO_LVL        0x28
#define UART_TX_DATA            0x2c
#define UART_RX_DATA            0x30
#define UART_STAT               0x34

#define UART_CFG_8N1            (1 << 11)   /* 8 data bits, no parity, break detect */
#define UART_FIFO_SIZE          128
#define UART_STAT_RX_EMPTY      (1 << 8)
#define UART_STAT_TX_FULL       (1 << 1)
