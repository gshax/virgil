#pragma once

#include <virgil/error.h>

/* xmodem boot method implementation */
extern virgil_error_t boot_xmodem(void);

/* nand boot method — slot 0 = uboot (0x40000), slot 1 = uboot2 (0x180000) */
extern virgil_error_t boot_nand(int slot);
