#include <stdint.h>
#include <gsfw/libc.h>
#include <virgil/common.h>
#include <virgil/romapi.h>
#include <virgil/hw.h>
#include <virgil/uart.h>
#include <virgil/cmu.h>
#include <virgil/ddr.h>
#include <virgil/chainload.h>
#include <virgil/xmodem.h>
#include <gsfw/firmware/shared.h>

void __entry _start(uint32_t rom_version, const dspg_dvf101_bootrom_api_t* bootrom, int bootsel)
{
    /* let the bootrom's "ok" finish draining before we print */
    bootrom->udelay(25000);

    char tmp[12];
    uart_puts("\nvirgil\n");
    uart_puts("rom_version = ");
    uart_puts(itoa(rom_version, tmp, 16));
    uart_puts(", bootsel = ");
    uart_puts(itoa(bootsel, tmp, 16));
    uart_puts("\n");

    uart_puts("cmu init...");
    cmu_init(bootrom);
    uart_puts("ok\n");

    uart_puts("ddr init...");
    if (ddr_init(bootrom) != 0) {
        uart_puts("failed!\n");
        panic("ddr initialization failed");
    }
    uart_puts("ok!!\n\n");

    xmodem_init(bootrom);

    while (1) {
        void* load_addr = (void*)DVF_UBOOT_LOAD_ADDR;
        uart_puts("awaiting xmodem transfer!\n");

        int rcvd = xmodem_rcv(load_addr, 0x1000000);
        bootrom->udelay(100000);
        uart_puts("\n");

        if (rcvd < 0) {
            panic("xmodem transfer error");
        }

        /* detect image format */
        uart_puts("checking image...\n");
        virgil_chain_type_t type = chainload_detect(load_addr);
        if (type == virgil_chain_unknown) {
            uart_puts("unknown image format\n\n");
            continue;
        }

        switch (type) {
            case virgil_chain_gs:
                uart_puts("grandstream image, ");
                break;
            default:
                continue;
        }

        /* verify checksum */
        if (chainload_checksum(type, load_addr, rcvd) != 0) {
            uart_puts("checksum error!\n\n");
            continue;
        }
        uart_puts("checksum ok!\n\n");

        /* move image to correct location */
        void* entrypoint = chainload_memmove(type, load_addr);
        if (!entrypoint) {
            uart_puts("memmove error!\n\n");
            continue;
        }

        /* jump to entrypoint! */
        ((void (*)(void))entrypoint)();
    }
}
