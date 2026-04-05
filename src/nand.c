/*
 * DSPG DMW96 NAND flash controller — minimal read-only driver
 *
 * ported from the GPL u-boot NFC driver (dmw96_nfc.c) by DSP Group.
 * stripped down to just what virgil needs: init, identify, page read
 * with hardware BCH ECC.
 */

#include <stdint.h>
#include <gsfw/libc.h>
#include <virgil/common.h>
#include <virgil/hw.h>
#include <virgil/nand.h>
#include <virgil/romapi.h>
#include <virgil/uart.h>

/* ---- DMA buffer ---- */

#define NAND_MAX_PAGESIZE   4096
#define NAND_MAX_OOBSIZE    256

/*
 * use bottom of DRAM for the NFC DMA buffer. the .bss.dma region at
 * 0x00200000 (from the DVF99 linker script) is not CPU-accessible on DVF101.
 * DRAM starts at 0x40000000, well below the U-Boot load address at 0x41000000.
 */
#define DMA_BUF_ADDR        0x40000000
static uint8_t* const dma_buf = (uint8_t*)DMA_BUF_ADDR;

/* ---- driver state ---- */

static nand_geo_t geo;

/* ECC error code queue (fetched during transfer, fixed after) */
#define MAX_ECC_CODES       128
static uint32_t ecc_codes[MAX_ECC_CODES];
static int ecc_numcodes;

/* REV1 chien search coefficients (32 entries) */
static const uint32_t chien_codes[] = {
    0x0B19, 0x14E2, 0x0D8D, 0x01E8, 0x0D7D, 0x1B24, 0x1351, 0x14AE,
    0x1EA6, 0x0E12, 0x1512, 0x09E9, 0x1365, 0x0AE3, 0x1E4A, 0x11B8,
    0x14A1, 0x11ED, 0x1C13, 0x0CE5, 0x0D18, 0x02C4, 0x11B9, 0x0FBC,
    0x0D62, 0x1A03, 0x0772, 0x1346, 0x1980, 0x1DF5, 0x1140, 0x1D45,
};

/* ---- register access ---- */

static inline uint32_t nfc_readl(uint32_t reg)
{
    return readl(DVF101_NFC_BASE + reg);
}

static inline void nfc_writel(uint32_t val, uint32_t reg)
{
    writel(val, DVF101_NFC_BASE + reg);
}

/* ---- low-level helpers ---- */

static void nfc_clear_status(void)
{
    nfc_writel(0xFFFFFFFF, NFC_STS_CLR);
}

static void nfc_start(void)
{
    nfc_clear_status();
    nfc_writel(1, NFC_START);
}

static void nfc_ecc_fetch(void)
{
    uint32_t status = nfc_readl(NFC_STATUS);

    /* non-correctable error — craft a marker and flush */
    if (status & NFC_ST_ECC_NC_ERR) {
        uint32_t seq = (status >> 12) & 0xF;
        ecc_codes[0] = BCH_NC_MASK | (seq << 13);
        ecc_numcodes = 1;
        nfc_writel(NFC_ST_ECC_NC_ERR, NFC_STS_CLR);
        while (nfc_readl(NFC_STATUS) & NFC_ST_ERR_FOUND)
            nfc_readl(NFC_BCH);
        return;
    }

    while (nfc_readl(NFC_STATUS) & NFC_ST_ERR_FOUND) {
        if (ecc_numcodes < MAX_ECC_CODES)
            ecc_codes[ecc_numcodes++] = nfc_readl(NFC_BCH);
        else
            nfc_readl(NFC_BCH);
    }
}

static void nfc_wait_done(int has_ecc)
{
    uint32_t done = NFC_ST_TRANS_DONE;

    /* only wait for AHB_DONE if the sequence has a data stage */
    if (nfc_readl(NFC_SEQUENCE) & NFC_SEQ_RW_EN)
        done |= NFC_ST_AHB_DONE;
    if (has_ecc)
        done |= NFC_ST_ECC_DONE;

    uint32_t reg;
    do {
        reg = nfc_readl(NFC_STATUS);
        if (reg & (NFC_ST_ECC_NC_ERR | NFC_ST_ERR_FOUND))
            nfc_ecc_fetch();
    } while ((reg & done) != done);
}

/* ---- simple commands (reset, readid) ---- */

static void nfc_simple_command(uint8_t cmd, int column, int len)
{
    uint32_t seq = NFC_SEQ_CMD1_EN | NFC_SEQ_WAIT1_EN |
                   NFC_SEQ_CHIP_SEL(0) | NFC_SEQ_RDY_EN | NFC_SEQ_RDY_SEL(0);

    if (len)
        seq |= NFC_SEQ_RW_EN | NFC_SEQ_DATA_READ;
    if (column >= 0)
        seq |= NFC_SEQ_ADD1_EN;

    nfc_writel(seq, NFC_SEQUENCE);
    nfc_writel(column >= 0 ? column : 0, NFC_ADDR_COL);
    nfc_writel(0, NFC_DCOUNT);
    nfc_writel(0, NFC_TIMEOUT);
    nfc_writel(len, NFC_FTSR);
    nfc_writel(cmd, NFC_CMD1);
    nfc_writel((uint32_t)dma_buf, NFC_DPR);

    nfc_start();
    nfc_wait_done(0);
}

/* ---- ECC correction ---- */

static int count_zero_bits(uint8_t val)
{
    int n = 0;
    for (int i = 0; i < 8; i++)
        if (!(val & (1 << i)))
            n++;
    return n;
}

static int ecc_size(int mode)
{
    static const int bits[] = { 1, 4, 8, 16 };
    if (mode == 0) return 3;
    return (bits[mode] * 13 + 7) / 8;
}

static int page_is_erased(void)
{
    uint8_t dirty = dma_buf[geo.page_size + geo.dirtypos];
    if (dirty == 0xFF) return 1;
    if (dirty == 0x00) return 0;

    int zero_bits = 0;
    uint8_t *ecc_area = &dma_buf[geo.page_size + geo.n1];
    int ecc_len = ecc_size(geo.ecc_mode);
    for (int i = 0; i < ecc_len; i++)
        if (ecc_area[i] != 0xFF)
            zero_bits += count_zero_bits(ecc_area[i]);

    static const int ecc_bits[] = { 1, 4, 8, 16 };
    return zero_bits <= ecc_bits[geo.ecc_mode];
}

static int is_corrupted_blank(void)
{
    int zero_bits = 0;
    for (uint32_t i = 0; i < geo.page_size + geo.n1; i++)
        if (dma_buf[i] != 0xFF)
            zero_bits += count_zero_bits(dma_buf[i]);

    static const int ecc_bits[] = { 1, 4, 8, 16 };
    return zero_bits <= ecc_bits[geo.ecc_mode];
}

static void ecc_fix_bit(int sequence, int location)
{
    int bit = location & 7;
    int byte = location >> 3;

    if (byte >= VP_SIZE) {
        byte += geo.page_size - VP_SIZE;
        byte += sequence * (geo.n1 + geo.n2);
    } else {
        byte += sequence * VP_SIZE;
    }

    if ((uint32_t)byte >= geo.page_size + geo.oob_size)
        return;

    dma_buf[byte] ^= (1 << bit);
}

static int ecc_fix_bch(void)
{
    int stat = 0;
    int vector_length = (4096 + (geo.n1 * 8));
    static const int bits[] = { 1, 4, 8, 16 };
    vector_length += bits[geo.ecc_mode] * 13 - 1;

    while (ecc_numcodes) {
        uint32_t bch = ecc_codes[ecc_numcodes - 1];
        int sequence = (bch >> 13) & 0xF;
        int location = bch & 0x1FFF;
        location = vector_length - location;

        if ((bch & BCH_NC_MASK) || location < 0) {
            ecc_numcodes = 0;
            return -1;
        }

        ecc_fix_bit(sequence, location);
        stat++;
        ecc_numcodes--;
    }

    return stat;
}

static int ecc_fix_errors(void)
{
    nfc_ecc_fetch();

    if (page_is_erased()) {
        ecc_numcodes = 0;
        if (is_corrupted_blank())
            memset(dma_buf, 0xFF, geo.page_size + geo.oob_size);
        return 0;
    }

    return ecc_fix_bch();
}

/* ---- bootrom version detection (same as u-boot dmw96_nfc_bootrom_version) ---- */

static int bootrom_version(void)
{
    if (*(volatile uint32_t*)0x00005464 == 0x0A302E31) return 1;
    if (*(volatile uint32_t*)0x000112E4 == 0x0A302E32) return 2;
    return 3;
}

/* ---- hardware init ---- */

static void nfc_clock_init(void)
{
    unsigned long reg = DVF101_CMU_BASE + CMU_NFC_CTRL;
    uint32_t val;

    /* release from reset (clear bit 0) */
    val = readl(reg);
    val &= ~CMU_CTRL_RESET;
    writel(val, reg);

    /* enable APB + AHB clocks */
    val = readl(reg);
    val |= CMU_CTRL_APB_CLK_EN | CMU_CTRL_AHB_CLK_EN;
    writel(val, reg);
}

static void nfc_pinmux_init(void)
{
    unsigned long iom5 = DVF101_SYSCFG_BASE + SYSCFG_IOM5;
    unsigned long iom4 = DVF101_SYSCFG_BASE + SYSCFG_IOM4;
    uint32_t val;

    /* IOM5: 13 NAND pins, each a 2-bit mux field set to 01 */
    val = readl(iom5);
    val &= 0x3F;
    val |= 0x55555540;
    writel(val, iom5);

    /* IOM4: nfld7 at bits [1:0] */
    val = readl(iom4);
    val &= ~3;
    val |= 1;
    writel(val, iom4);
}

static void nfc_drive_strength_init(void)
{
    unsigned long iods_base = DVF101_SYSCFG_BASE + 0x22C;
    uint32_t val;

    /* agpio0: IODS +0x0C, bits [1:0] = 0 */
    val = readl(iods_base + 0x0C);
    val &= ~3;
    writel(val, iods_base + 0x0C);

    /* agpio1-13: IODS +0x10, bits [31:6] = 0 */
    val = readl(iods_base + 0x10);
    val &= 0x3F;
    writel(val, iods_base + 0x10);
}

static void nfc_timing_init(void)
{
    /* all wait ready values = 4 */
    nfc_writel(NFC_WAIT_A(4) | NFC_WAIT_B(4) | NFC_WAIT_C(4) | NFC_WAIT_D(4), NFC_WAIT1);
    nfc_writel(NFC_WAIT_A(4) | NFC_WAIT_B(4) | NFC_WAIT_C(4) | NFC_WAIT_D(4), NFC_WAIT2);
    nfc_writel(NFC_WAIT_A(4) | NFC_WAIT_B(4) | NFC_WAIT_C(4) | NFC_WAIT_D(4), NFC_WAIT3);
    nfc_writel(NFC_WAIT_A(4) | NFC_WAIT_B(4), NFC_WAIT4);

    /* pulse timing: all values = 4 */
    nfc_writel(NFC_PT_RD_LOW(4)  | NFC_PT_RD_HIGH(4) |
               NFC_PT_WR_LOW(4)  | NFC_PT_WR_HIGH(4) |
               NFC_PT_CLE(4)     | NFC_PT_ALE(4),
               NFC_PULSETIME);

    /* load chien search coefficients */
    for (int i = 0; i < 32; i++)
        nfc_writel(chien_codes[i], NFC_CHIEN_SEARCH_1 + i * 4);
}

/* ---- flash identification ---- */

static virgil_error_t nfc_identify(void)
{
    nfc_simple_command(NAND_CMD_READID, 0, 8);

    uint8_t mfr_id = dma_buf[0];
    uint8_t dev_id = dma_buf[1];

    /*
     * decode ONFI-style ID bytes for page/oob/block geometry.
     * byte 3 bits [1:0] = page size: 0=1K, 1=2K, 2=4K, 3=8K
     * byte 3 bit  [2]   = oob per 512: 0=8, 1=16
     * byte 4 bits [1:0] = block size: 0=64K, 1=128K, 2=256K, 3=512K
     */
    uint8_t byte3 = dma_buf[3];
    uint8_t byte4 = dma_buf[4];

    geo.page_size = 1024 << (byte3 & 0x3);
    geo.oob_size = (byte3 & 0x4) ? 16 : 8;
    geo.oob_size *= (geo.page_size / 512);
    uint32_t block_size = (64 * 1024) << (byte4 & 0x3);
    geo.pages_per_block = block_size / geo.page_size;

    /* TODO: derive chip size from device ID instead of hardcoding */
    uint32_t chip_size = 128 * 1024 * 1024;
    geo.block_count = chip_size / block_size;

    /*
     * select ECC mode. non-ONFI large page with 64+ OOB uses BCH4,
     * matching u-boot's fallback path.
     */
    if (geo.oob_size >= 64)
        geo.ecc_mode = NFC_ECC_BCH4;
    else
        geo.ecc_mode = NFC_ECC_HAMMING;

    /* spare area layout based on bootrom version */
    int ver = bootrom_version();
    if (ver < 3) {
        geo.dirtypos = 2;
        geo.n1 = 4;
        geo.n3 = 2;
    } else {
        geo.dirtypos = 1;
        geo.n1 = 2;
        geo.n3 = 1;
    }
    geo.n2 = (geo.oob_size / (geo.page_size >> VP_SIZE_SHIFT)) - geo.n1;

    return VIRGIL_OK;
}

/* ---- page read ---- */

virgil_error_t nand_read_page(uint32_t page_addr, void* data)
{
    uint32_t vp_count = geo.page_size >> VP_SIZE_SHIFT;
    uint32_t spare_words = ((geo.n1 + geo.n2 + 3) & ~3UL) >> 2;

    ecc_numcodes = 0;

    /* set ECC control */
    nfc_writel(NFC_CTL_ECC_OP_MODE(geo.ecc_mode) |
               NFC_CTL_CHIEN_CNT_START(0x0E6C),
               NFC_CTL);

    /* build sequence for large-page READ0 */
    uint32_t total_pages = geo.block_count * geo.pages_per_block;
    uint32_t seq = NFC_SEQ_CMD1_EN |
                   NFC_SEQ_ADD1_EN | NFC_SEQ_ADD2_EN |
                   NFC_SEQ_ADD3_EN | NFC_SEQ_ADD4_EN |
                   (total_pages > 65536 ? NFC_SEQ_ADD5_EN : 0) |
                   NFC_SEQ_CMD2_EN | NFC_SEQ_WAIT2_EN |
                   NFC_SEQ_RW_EN   | NFC_SEQ_DATA_READ | NFC_SEQ_DATA_ECC(1) |
                   NFC_SEQ_WAIT1_EN |
                   NFC_SEQ_CMD5_EN | NFC_SEQ_WAIT5_EN |
                   NFC_SEQ_CMD6_EN | NFC_SEQ_WAIT6_EN |
                   NFC_SEQ_CHIP_SEL(0) | NFC_SEQ_RDY_EN | NFC_SEQ_RDY_SEL(0);

    nfc_writel(seq, NFC_SEQUENCE);

    /* address: column=0, row=page_addr */
    nfc_writel(0, NFC_ADDR_COL);
    nfc_writel(NFC_ROW_ADD3(page_addr & 0xFF) |
               NFC_ROW_ADD4((page_addr >> 8) & 0xFF) |
               NFC_ROW_ADD5((page_addr >> 16) & 0xFF),
               NFC_ADDR_ROW);

    /* commands: READ0, READSTART, RNDOUT, RNDOUTSTART */
    nfc_writel(NFC_CMD_1(NAND_CMD_READ0) | NFC_CMD_2(NAND_CMD_READSTART),
               NFC_CMD1);
    nfc_writel(NFC_CMD_5(NAND_CMD_RNDOUT) | NFC_CMD_6(NAND_CMD_RNDOUTSTART),
               NFC_CMD2);

    /* data count: virtual pages with spare area */
    nfc_writel(NFC_DC_VP_SIZE(VP_SIZE) |
               NFC_DC_SPARE_N3(geo.n3) |
               NFC_DC_SPARE_N2(geo.n2) |
               NFC_DC_SPARE_N1(geo.n1) |
               NFC_DC_PAGE_CNT(vp_count - 1),
               NFC_DCOUNT);

    nfc_writel(NFC_FTSR_SIZE(vp_count), NFC_FTSR);

    /* spare words in PULSETIME */
    uint32_t pt = nfc_readl(NFC_PULSETIME);
    pt &= ~NFC_PT_SPARE_W(0x3F);
    pt |= NFC_PT_SPARE_W(spare_words);
    nfc_writel(pt, NFC_PULSETIME);

    /* DMA pointers */
    nfc_writel((uint32_t)dma_buf, NFC_DPR);
    nfc_writel((uint32_t)dma_buf + geo.page_size, NFC_RDPR);

    nfc_start();
    nfc_wait_done(1);
    nfc_writel(1, NFC_STS_CLR);

    /* fix ECC errors */
    int stat = ecc_fix_errors();
    if (stat < 0)
        return VIRGIL_NAND_ECC;

    if (data)
        memcpy(data, dma_buf, geo.page_size);

    return VIRGIL_OK;
}

/* ---- sequential read ---- */

virgil_error_t nand_read(uint32_t byte_offset, void* buf, uint32_t len)
{
    uint8_t* dst = buf;
    uint32_t page = byte_offset / geo.page_size;
    uint32_t page_off = byte_offset % geo.page_size;

    while (len > 0) {
        virgil_error_t err = nand_read_page(page, NULL);
        if (err)
            return err;

        uint32_t avail = geo.page_size - page_off;
        uint32_t chunk = len < avail ? len : avail;
        memcpy(dst, dma_buf + page_off, chunk);

        dst += chunk;
        len -= chunk;
        page++;
        page_off = 0;
    }

    return VIRGIL_OK;
}

/* ---- public API ---- */

const nand_geo_t* nand_geometry(void)
{
    return &geo;
}

virgil_error_t nand_init(void)
{
    nfc_clock_init();
    nfc_pinmux_init();
    nfc_drive_strength_init();
    nfc_timing_init();
    nfc_clear_status();

    /* reset the NAND chip */
    nfc_simple_command(NAND_CMD_RESET, -1, 0);

    /* identify flash and configure ECC layout */
    return nfc_identify();
}
