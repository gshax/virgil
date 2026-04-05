#pragma once

#include <stddef.h>
#include <virgil/common.h>
#include <virgil/error.h>

typedef enum virgil_chain_type {
    virgil_chain_type_unknown = 0,
    virgil_chain_type_gs
} virgil_chain_type_t;

typedef struct virgil_chain_detection {
    /* start of raw image in memory */
    void* raw_image;
    /* detected image format */
    virgil_chain_type_t type;
    /* expected size of entire image, including header */
    size_t full_size;
    /* entrypoint, or NULL if not ready */
    void* entrypoint;
} virgil_chain_detection_t;

/* detect image format, determine full load length */
extern virgil_error_t chainload_detect(void* header, virgil_chain_detection_t* fingerprint);

/* get human-readable name for format */
extern const char* chainload_format_name(virgil_chain_type_t type);

/* validate image content against header */
extern virgil_error_t chainload_validate(virgil_chain_detection_t* fingerprint);

/* move image to its requested location in DRAM */
extern virgil_error_t chainload_move(virgil_chain_detection_t* fingerprint);

/* high-level chainload preparation call */
extern virgil_error_t chainload_prepare(void* image, virgil_chain_detection_t* fingerprint);

/* jump to entrypoint! */
extern virgil_error_t chainload_boot(virgil_chain_detection_t* fingerprint);
