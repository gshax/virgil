#include <gsfw/firmware/family/ht8xx_dvf101.h>
#include <gsfw/libc.h>
#include <virgil/chainload.h>
#include <virgil/uart.h>

virgil_error_t chainload_detect(void* header, virgil_chain_detection_t* fingerprint) {
    /* initialize as raw/invalid */
    fingerprint->raw_image = header;
    fingerprint->type = virgil_chain_type_unknown;
    fingerprint->full_size = 0;
    fingerprint->entrypoint = NULL;

    /* try Grandstream image */
    ht8_dvf101_image_hdr_t* gs_hdr = header;
    if (gs_hdr->magic == GS_HT8_DVF101_IMG_MAGIC) {
        fingerprint->type = virgil_chain_type_gs;
        fingerprint->full_size = gs_hdr->size_image;
    }

    return fingerprint->type == virgil_chain_type_unknown ? VIRGIL_UNKNOWN_FORMAT : VIRGIL_OK;
}

const char* chainload_format_name(virgil_chain_type_t type) {
    switch (type) {
        case virgil_chain_type_unknown:
            return "unknown/raw";
        case virgil_chain_type_gs:
            return "grandstream format";
        default:
            return "<invalid format>";
    }
}

virgil_error_t chainload_validate(virgil_chain_detection_t* fingerprint) {
    if (!fingerprint->raw_image) { return VIRGIL_ORDER_OF_OPERATIONS; }
    switch (fingerprint->type) {
        case virgil_chain_type_gs: {
            ht8_dvf101_image_hdr_t* gs_hdr = fingerprint->raw_image;
            char* body = fingerprint->raw_image + gs_hdr->start;
            if (gs_sum((uint16_t*)body, gs_hdr->size) != gs_hdr->checksum) {
                return VIRGIL_INVALID_IMAGE;
            }
            return VIRGIL_OK;
        }
        default:
            break;
    }
    return VIRGIL_UNKNOWN_FORMAT;
}

virgil_error_t chainload_move(virgil_chain_detection_t* fingerprint) {
    if (!fingerprint->raw_image) { return VIRGIL_ORDER_OF_OPERATIONS; }
    switch (fingerprint->type) {
        case virgil_chain_type_gs: {
            ht8_dvf101_image_hdr_t* gs_hdr = fingerprint->raw_image;
            memmove(fingerprint->raw_image, fingerprint->raw_image + gs_hdr->start, gs_hdr->size);
            fingerprint->entrypoint = fingerprint->raw_image;
            /* raw image is no longer available */
            fingerprint->raw_image = NULL;
            return VIRGIL_OK;
        }
        default:
            break;
    }
    return VIRGIL_UNKNOWN_FORMAT;
}

virgil_error_t chainload_prepare(void* image, virgil_chain_detection_t* fingerprint) {
    virgil_error_t status;

    /* detect image format */
    status = chainload_detect(image, fingerprint);
    if (status) { return status; }
    
    /* verify checksum */
    status = chainload_validate(fingerprint);
    if (status) { return status; }

    /* move image to correct location */
    status = chainload_move(fingerprint);
    if (status) { return status; }

    return VIRGIL_OK;
}

virgil_error_t chainload_boot(virgil_chain_detection_t* fingerprint) {
    if (!fingerprint->entrypoint) { return VIRGIL_ORDER_OF_OPERATIONS; }
    switch (fingerprint->type) {
        case virgil_chain_type_gs:
            uart_exit();
            ((void (*)(void))fingerprint->entrypoint)();
            goto panic;
        panic:
            panic("entrypoint returned");
        default:
            break;
    }
    return VIRGIL_UNKNOWN_FORMAT;
}
