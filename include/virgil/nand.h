#pragma once

#include <stdint.h>
#include <virgil/error.h>

/*
 * DSPG DMW96 NAND flash controller driver
 *
 * register definitions from the GPL u-boot NFC driver (dmw96_nfc.h).
 * this is a minimal read-only driver for booting U-Boot from NAND.
 */

/* ---- NFC register offsets (from NFC_BASE) ---- */

#define NFC_CTL             0x0000
#define NFC_STATUS          0x0004
#define NFC_STS_CLR         0x0008
#define NFC_INT_EN          0x000C
#define NFC_SEQUENCE        0x0014
#define NFC_ADDR_COL        0x0018
#define NFC_ADDR_ROW        0x001C
#define NFC_CMD1            0x0020
#define NFC_CMD2            0x0024
#define NFC_WAIT1           0x0028
#define NFC_WAIT2           0x002C
#define NFC_WAIT3           0x0030
#define NFC_WAIT4           0x0034
#define NFC_PULSETIME       0x0038
#define NFC_DCOUNT          0x003C
#define NFC_FTSR            0x0040
#define NFC_DPR             0x0044
#define NFC_RDPR            0x0048
#define NFC_TIMEOUT         0x004C
#define NFC_BCH             0x0050
#define NFC_HAMMING         0x0054
#define NFC_START           0x0058
#define NFC_CHIEN_SEARCH_1  0x0068
#define NFC_AHB_CTL         0x00C8

/* ---- NFC_CTL bits ---- */

#define NFC_CTL_ECC_OP_MODE(m)      (((m) & 0xF) << 0)
#define NFC_CTL_CHIEN_CNT_START(v)  (((v) & 0x1FFF) << 16)

/*
 * ECC mode values — these are the array indices from u-boot's eccmode_bits[],
 * written directly into the ECC_OP_MODE register field.
 *   mode 0: hamming (1-bit) — hardware ECC disabled, software handles it
 *   mode 1: BCH4 (4-bit)
 *   mode 2: BCH8 (8-bit)
 *   mode 3: BCH16 (REV1) / BCH12 (others)
 */
#define NFC_ECC_HAMMING     0
#define NFC_ECC_BCH4        1
#define NFC_ECC_BCH8        2
#define NFC_ECC_BCH16       3   /* REV1 */

/* ---- NFC_STATUS bits ---- */

#define NFC_ST_TRANS_DONE   (1 << 0)
#define NFC_ST_ECC_DONE     (1 << 1)
#define NFC_ST_ERR_FOUND    (1 << 2)
#define NFC_ST_RDY_TIMEOUT  (1 << 3)
#define NFC_ST_TRANS_BUSY   (1 << 6)
#define NFC_ST_ECC_BUSY     (1 << 7)
#define NFC_ST_ECC_NC_ERR   (1 << 11)
#define NFC_ST_ECC_NC_SEQ   (0xF << 12)
#define NFC_ST_AHB_ERR      (1 << 16)
#define NFC_ST_AHB_DONE     (1 << 17)

/* ---- NFC_SEQUENCE bits ---- */

#define NFC_SEQ_CMD1_EN     (1 << 0)
#define NFC_SEQ_WAIT0_EN    (1 << 1)
#define NFC_SEQ_ADD1_EN     (1 << 2)
#define NFC_SEQ_ADD2_EN     (1 << 3)
#define NFC_SEQ_ADD3_EN     (1 << 4)
#define NFC_SEQ_ADD4_EN     (1 << 5)
#define NFC_SEQ_ADD5_EN     (1 << 6)
#define NFC_SEQ_WAIT1_EN    (1 << 7)
#define NFC_SEQ_CMD2_EN     (1 << 8)
#define NFC_SEQ_WAIT2_EN    (1 << 9)
#define NFC_SEQ_RW_EN       (1 << 10)
#define NFC_SEQ_DATA_READ   (0 << 11)
#define NFC_SEQ_DATA_WRITE  (1 << 11)
#define NFC_SEQ_DATA_ECC(e) (((e) & 1) << 12)
#define NFC_SEQ_CMD3_EN     (1 << 13)
#define NFC_SEQ_WAIT3_EN    (1 << 14)
#define NFC_SEQ_CMD4_EN     (1 << 15)
#define NFC_SEQ_WAIT4_EN    (1 << 16)
#define NFC_SEQ_READ_ONCE   (1 << 17)
#define NFC_SEQ_CHIP_SEL(c) (((c) & 3) << 18)
#define NFC_SEQ_KEEP_CS     (1 << 20)
#define NFC_SEQ_MODE8       (0 << 21)
#define NFC_SEQ_RDY_EN      (1 << 22)
#define NFC_SEQ_RDY_SEL(s)  (((s) & 1) << 23)
#define NFC_SEQ_CMD5_EN     (1 << 24)
#define NFC_SEQ_WAIT5_EN    (1 << 25)
#define NFC_SEQ_CMD6_EN     (1 << 26)
#define NFC_SEQ_WAIT6_EN    (1 << 27)

/* ---- NFC_CMD helpers ---- */

#define NFC_CMD_1(c)        (((c) & 0xFF) << 0)
#define NFC_CMD_2(c)        (((c) & 0xFF) << 8)
#define NFC_CMD_3(c)        (((c) & 0xFF) << 16)
#define NFC_CMD_4(c)        (((c) & 0xFF) << 24)
#define NFC_CMD_5(c)        (((c) & 0xFF) << 0)
#define NFC_CMD_6(c)        (((c) & 0xFF) << 8)

/* ---- NFC address helpers ---- */

#define NFC_COL_ADD1(a)     (((a) & 0xFF) << 0)
#define NFC_COL_ADD2(a)     (((a) & 0xFF) << 8)
#define NFC_ROW_ADD3(a)     (((a) & 0xFF) << 0)
#define NFC_ROW_ADD4(a)     (((a) & 0xFF) << 8)
#define NFC_ROW_ADD5(a)     (((a) & 0xFF) << 16)

/* ---- NFC_PULSETIME helpers ---- */

#define NFC_PT_RD_LOW(v)    (((v) & 0xF) << 0)
#define NFC_PT_RD_HIGH(v)   (((v) & 0xF) << 4)
#define NFC_PT_WR_LOW(v)    (((v) & 0xF) << 8)
#define NFC_PT_WR_HIGH(v)   (((v) & 0xF) << 12)
#define NFC_PT_CLE(v)       (((v) & 0xF) << 16)
#define NFC_PT_ALE(v)       (((v) & 0xF) << 20)
#define NFC_PT_SPARE_W(v)   (((v) & 0x3F) << 24)

/* ---- NFC_DCOUNT helpers ---- */

#define NFC_DC_VP_SIZE(v)   (((v) & 0x1FFF) << 0)
#define NFC_DC_SPARE_N3(v)  (((v) & 0x7) << 13)
#define NFC_DC_SPARE_N2(v)  (((v) & 0xFF) << 16)
#define NFC_DC_SPARE_N1(v)  (((v) & 0xF) << 24)
#define NFC_DC_PAGE_CNT(v)  (((v) & 0xF) << 28)

/* ---- NFC_FTSR helpers ---- */

#define NFC_FTSR_SIZE(v)    (((v) & 0x1FFF) << 0)

/* ---- NFC_WAIT helpers ---- */

#define NFC_WAIT_A(v)       (((v) & 0x3F) << 0)
#define NFC_WAIT_B(v)       (((v) & 0x3F) << 8)
#define NFC_WAIT_C(v)       (((v) & 0x3F) << 16)
#define NFC_WAIT_D(v)       (((v) & 0x3F) << 24)

/* ---- NAND commands ---- */

#define NAND_CMD_READ0      0x00
#define NAND_CMD_READSTART  0x30
#define NAND_CMD_READID     0x90
#define NAND_CMD_RESET      0xFF
#define NAND_CMD_RNDOUT     0x05
#define NAND_CMD_RNDOUTSTART 0xE0
#define NAND_CMD_STATUS     0x70

/* ---- virtual page constants ---- */

#define VP_SIZE             512
#define VP_SIZE_SHIFT       9

/* BCH non-correctable marker (in our ecc_codes queue) */
#define BCH_NC_MASK         (1 << 31)

/* ---- flash geometry ---- */

typedef struct nand_geo {
    uint32_t page_size;
    uint32_t oob_size;
    uint32_t pages_per_block;
    uint32_t block_count;
    int ecc_mode;
    /* spare area layout (depends on bootrom version) */
    uint8_t n1, n2, n3;
    uint8_t dirtypos;
} nand_geo_t;

/* ---- public API ---- */

virgil_error_t nand_init(void);
virgil_error_t nand_read_page(uint32_t page_addr, void* data);
virgil_error_t nand_read(uint32_t byte_offset, void* buf, uint32_t len);
const nand_geo_t* nand_geometry(void);
