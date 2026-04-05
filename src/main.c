#include <stdint.h>
#include <gsfw/libc.h>
#include <virgil/common.h>
#include <virgil/romapi.h>
#include <virgil/hw.h>
#include <virgil/uart.h>
#include <virgil/cmu.h>
#include <virgil/ddr.h>
#include <virgil/bootmethod.h>

const dspg_bootrom_api_t* rom_api;

void __entry _start(uint32_t rom_version, const dspg_bootrom_api_t* _rom_api, int bootsel)
{
    rom_api = _rom_api;

    /* drain uart and then immediately do clock init */
    rom_api->udelay(25000);
    cmu_init();

    /* figlet font "Roman", same as "DVF101" ASCII art found in U-Boot ^^ */
    uart_puts(
        "\n"
        "             o8o                       o8o  oooo  \n"
        "             `\"'                       `\"'  `888  \n"
        "oooo    ooo oooo  oooo d8b  .oooooooo oooo   888  \n"
        " `88.  .8'  `888  `888\"\"8P 888' `88b  `888   888  \n"
        "  `88..8'    888   888     888   888   888   888  \n"
        "   `888'     888   888     `88bod8P'   888   888  \n"
        "    `8'     o888o d888b    `8oooooo.  o888o o888o \n"
        "                           d\"     YD              \n"
        "                           \"Y88888P'              \n"
        "\n"
    );

    /* dump rom version and boot selection */
    char tmp[12];
    uart_puts("rom_version = ");
    uart_puts(itoa(rom_version, tmp, 16));
    uart_puts(", bootsel = ");
    uart_puts(itoa(bootsel, tmp, 16));
    uart_puts("\n");

    /* initialize dram */
    uart_puts("initializing ddr... ");
    if (ddr_init() != VIRGIL_OK) {
        uart_puts("failed!\n");
        panic("ddr initialization failed");
    }
    uart_puts("ok!\n");

    /* for now, just try to boot from xmodem forever */
    while (1) {
        boot_xmodem();
    }
}
