#include <virgil/bootmethod.h>
#include <virgil/chainload.h>
#include <virgil/common.h>
#include <virgil/hw.h>
#include <virgil/romapi.h>
#include <virgil/uart.h>
#include <virgil/xmodem.h>

virgil_error_t boot_xmodem() {
    void* load_addr = (void*)DVF_UBOOT_LOAD_ADDR;
    uart_puts("\nawaiting xmodem transfer!\n");

    int rcvd = xmodem_rcv(load_addr, 0x1000000);
    rom_api->udelay(100000);
    uart_puts("\n");
    if (rcvd < 0) {
        return VIRGIL_XMODEM_ERROR;
    }

    uart_puts("preparing image... ");
    virgil_chain_detection_t fingerprint;
    virgil_error_t status = chainload_prepare(load_addr, &fingerprint);
    uart_puts(error_string(status));
    uart_puts("!\n");
    if (status) {
        return status;
    }
    uart_puts("valid ");
    uart_puts(chainload_format_name(fingerprint.type));
    uart_puts(" image!\n");

    /* jump to entrypoint! */
    uart_puts("booting...\n");
    status = chainload_boot(&fingerprint);

    /* we shouldnt have gotten this far */
    uart_puts(error_string(status));
    uart_puts("!\n");
    return status;
}
