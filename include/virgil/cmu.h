#pragma once

/*
 * configure PLLs and clock tree for normal operation.
 *
 * the bootrom locks all PLLs but leaves their outputs gated and at
 * default frequencies. cmu_init reprograms them to match the stock
 * bootastic cmu_setup output (values from running HT818 u-boot).
 *
 * PLL1 changes pclk, so the UART is reinitialized afterward.
 */
void cmu_init();
