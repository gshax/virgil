#pragma once

#include <virgil/romapi.h>

/*
 * DDR3 initialization for DVF101 (HT818)
 *
 * initializes the Synopsys Denali DDR PHY and controller for the
 * on-board Hynix H5TQ2G63DFR (DDR3-1066, 2Gbit, BL8, CL7, 533 MHz).
 *
 * register values extracted from HT818 bootastic via ghidra RE.
 *
 * returns 0 on success, -1 on memory test failure.
 */
int ddr_init(const dspg_dvf101_bootrom_api_t* rom);
