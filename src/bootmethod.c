#include <gsfw/libc.h>
#include <virgil/bootmethod.h>
#include <virgil/chainload.h>
#include <virgil/common.h>
#include <virgil/hw.h>
#include <virgil/nand.h>
#include <virgil/romapi.h>
#include <virgil/uart.h>
#include <virgil/xmodem.h>

/* NAND partition offsets */
#define UBOOT_OFFSET    0x040000
#define UBOOT2_OFFSET   0x180000

static const uint32_t nand_slot_offsets[] = { UBOOT_OFFSET, UBOOT2_OFFSET };

virgil_error_t boot_xmodem(void) {
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

virgil_error_t boot_nand(int slot) {
    if (slot < 0 || slot > 1)
        return VIRGIL_GENERIC_ERROR;

    uint32_t offset = nand_slot_offsets[slot];
    void* load_addr = (void*)DVF_UBOOT_LOAD_ADDR;
    const nand_geo_t* geo = nand_geometry();
    virgil_error_t status;

    uart_puts("\nloading from nand offset 0x");
    uart_puthex(offset);
    uart_puts("...\n");

    /* step 1: read first page to get image header */
    status = nand_read(offset, load_addr, geo->page_size);
    if (status) {
        uart_puts("nand read error: ");
        uart_puts(error_string(status));
        uart_puts("\n");
        return status;
    }

    /* step 2: detect image format and get full size */
    virgil_chain_detection_t fingerprint;
    status = chainload_detect(load_addr, &fingerprint);
    if (status) {
        uart_puts("image detect: ");
        uart_puts(error_string(status));
        uart_puts("\n");
        return status;
    }

    uart_puts("detected ");
    uart_puts(chainload_format_name(fingerprint.type));
    uart_puts(", size=0x");
    uart_puthex(fingerprint.full_size);
    uart_puts("\n");

    /* step 3: load the rest of the image */
    if (fingerprint.full_size > geo->page_size) {
        uint32_t remaining = fingerprint.full_size - geo->page_size;
        status = nand_read(offset + geo->page_size,
                           (uint8_t*)load_addr + geo->page_size,
                           remaining);
        if (status) {
            uart_puts("nand read error: ");
            uart_puts(error_string(status));
            uart_puts("\n");
            return status;
        }
    }

    /* step 4: prepare (re-detects + validates + moves — redundant detect is ok) */
    uart_puts("preparing image... ");
    status = chainload_prepare(load_addr, &fingerprint);
    uart_puts(error_string(status));
    uart_puts("!\n");
    if (status)
        return status;

    /* step 5: boot! */
    uart_puts("booting...\n");
    status = chainload_boot(&fingerprint);

    uart_puts(error_string(status));
    uart_puts("!\n");
    return status;
}
