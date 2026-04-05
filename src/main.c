#include <stdint.h>
#include <gsfw/libc.h>
#include <gsfw/firmware/family/ht8xx_dvf101.h>
#include <virgil/common.h>
#include <virgil/romapi.h>
#include <virgil/hw.h>
#include <virgil/uart.h>
#include <virgil/cmu.h>
#include <virgil/ddr.h>
#include <virgil/nand.h>
#include <virgil/bootmethod.h>

const dspg_bootrom_api_t* rom_api;

#define BOOTSEL_NAND    0
#define BOOTSEL_XMODEM  5

#define NAND_UBOOT_OFFSET   0x040000
#define NAND_UBOOT2_OFFSET  0x180000

/* ---- slot info for boot menu ---- */

typedef struct {
    int valid;
    uint32_t prov_counter;
    uint8_t ver_major;
    uint16_t ver_minor;
    uint8_t ver_revision;
} slot_info_t;

static slot_info_t slots[2];
static int nand_available;

static void probe_nand_slot(int slot, uint32_t offset)
{
    slots[slot].valid = 0;

    /* read just the first page — header is well within one page */
    uint8_t page_buf[2048];
    if (nand_read(offset, page_buf, sizeof(page_buf)))
        return;

    ht8_dvf101_image_hdr_t* hdr = (ht8_dvf101_image_hdr_t*)page_buf;
    if (hdr->magic != GS_HT8_DVF101_FW_MAGIC)
        return;

    slots[slot].valid = 1;
    slots[slot].prov_counter = hdr->prov_counter;
    slots[slot].ver_major = hdr->version.major;
    slots[slot].ver_minor = hdr->version.minor;
    slots[slot].ver_revision = hdr->version.revision;
}

static void print_slot(int slot, const char* name)
{
    char tmp[12];
    uart_puts("  [");
    uart_puts(itoa(slot + 1, tmp, 10));
    uart_puts("] nand: ");
    uart_puts(name);

    if (slots[slot].valid) {
        uart_puts("   (prov=");
        uart_puts(itoa(slots[slot].prov_counter, tmp, 10));
        uart_puts(", ver=");
        uart_puts(itoa(slots[slot].ver_major, tmp, 10));
        uart_puts(".");
        uart_puts(itoa(slots[slot].ver_minor, tmp, 10));
        uart_puts(".");
        uart_puts(itoa(slots[slot].ver_revision, tmp, 10));
        uart_puts(")");
    } else {
        uart_puts("   (invalid)");
    }
    uart_puts("\n");
}

/* compare two slots by provision_counter, then version. returns 0 or 1. */
static int select_best_slot(void)
{
    if (slots[0].valid && !slots[1].valid) return 0;
    if (!slots[0].valid && slots[1].valid) return 1;
    if (!slots[0].valid && !slots[1].valid) return 0;

    /* both valid — compare provision counter */
    if (slots[0].prov_counter > slots[1].prov_counter) return 0;
    if (slots[0].prov_counter < slots[1].prov_counter) return 1;

    /* tie — compare version (major, minor, revision) */
    if (slots[0].ver_major != slots[1].ver_major)
        return slots[0].ver_major > slots[1].ver_major ? 0 : 1;
    if (slots[0].ver_minor != slots[1].ver_minor)
        return slots[0].ver_minor > slots[1].ver_minor ? 0 : 1;
    if (slots[0].ver_revision != slots[1].ver_revision)
        return slots[0].ver_revision > slots[1].ver_revision ? 0 : 1;

    return 0; /* identical — prefer slot 0 */
}

static void boot_menu(int bootsel)
{
    int default_choice;

    if (bootsel == BOOTSEL_XMODEM || !nand_available) {
        /* xmodem mode or nand failed — default to xmodem */
        default_choice = 3;
    } else {
        /* nand mode — pick best slot */
        int best = select_best_slot();
        if (slots[best].valid)
            default_choice = best + 1; /* 1 or 2 */
        else
            default_choice = 3; /* no valid slots */
    }

    while (1) {
        uart_puts("\nboot menu:\n");
        if (nand_available) {
            print_slot(0, "uboot ");
            print_slot(1, "uboot2");
        }
        uart_puts("  [3] xmodem\n\n");

        char tmp[12];
        uart_puts("autobooting [");
        uart_puts(itoa(default_choice, tmp, 10));
        uart_puts("] in 3... ");

        /* 3 second countdown with keypress check */
        int choice = default_choice;
        for (int i = 0; i < 30; i++) {
            rom_api->udelay(100000); /* 100ms */
            if (uart_tstc()) {
                char c = uart_getc();
                if (c == '1' && nand_available && slots[0].valid)
                    choice = 1;
                else if (c == '2' && nand_available && slots[1].valid)
                    choice = 2;
                else if (c == '3')
                    choice = 3;
                else
                    continue;
                break;
            }
        }
        uart_puts("\n");

        virgil_error_t status;
        if (choice == 1 || choice == 2) {
            status = boot_nand(choice - 1);
            uart_puts("nand boot failed: ");
            uart_puts(error_string(status));
            uart_puts("\n");
        } else {
            status = boot_xmodem();
            /* boot_xmodem only returns on error */
        }

        /* on failure, loop back to menu */
    }
}

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

    /* initialize nand */
    uart_puts("initializing nand... ");
    if (nand_init() != VIRGIL_OK) {
        uart_puts("failed!\n");
        nand_available = 0;
    } else {
        uart_puts("ok!\n");
        nand_available = 1;
        probe_nand_slot(0, NAND_UBOOT_OFFSET);
        probe_nand_slot(1, NAND_UBOOT2_OFFSET);
    }

    boot_menu(bootsel);

    /* should never reach here */
    panic("boot menu returned");
}
