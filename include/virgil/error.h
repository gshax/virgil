#pragma once

typedef enum virgil_error {
    VIRGIL_OK,
    VIRGIL_GENERIC_ERROR,
    VIRGIL_UNKNOWN_FORMAT,
    VIRGIL_INVALID_IMAGE,
    VIRGIL_ORDER_OF_OPERATIONS,
    VIRGIL_XMODEM_ERROR,
    VIRGIL_DDR_INIT,
    VIRGIL_NAND_INIT,
    VIRGIL_NAND_ECC
} virgil_error_t;

extern const char* error_string(virgil_error_t error);
