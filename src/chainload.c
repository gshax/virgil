#include <gsfw/firmware/family/ht8xx_dvf101.h>
#include <gsfw/libc.h>
#include <virgil/chainload.h>
#include <virgil/uart.h>

virgil_chain_type_t chainload_detect(void* load_addr) {
    // try Grandstream image
    ht8_dvf101_image_hdr_t* gs_hdr = load_addr;
    if (gs_hdr->magic == GS_HT8_DVF101_FW_MAGIC) {
        return virgil_chain_gs;
    }
    // raw or invalid
    return virgil_chain_unknown;
}

int chainload_checksum(virgil_chain_type_t type, void* load_addr, int received_size) {
    char itoa_tmp[70];

    switch (type) {
        case virgil_chain_gs: {
            ht8_dvf101_image_hdr_t* gs_hdr = load_addr;
            char* body = load_addr + gs_hdr->start;
            return gs_sum((uint16_t*)body, gs_hdr->size) != gs_hdr->checksum;
        }
        default:
            return -1;
    }
}

void* chainload_memmove(virgil_chain_type_t type, void* load_addr) {
    switch (type) {
        case virgil_chain_gs: {
            ht8_dvf101_image_hdr_t* gs_hdr = load_addr;
            memmove(load_addr, load_addr + gs_hdr->start, gs_hdr->size);
            return load_addr;
        }
        default:
            return NULL;
    }
}
