#include <virgil/error.h>

const char* error_string(virgil_error_t error) {
    switch (error) {
        case VIRGIL_OK:
            return "ok";
        case VIRGIL_GENERIC_ERROR:
            return "generic error";
        case VIRGIL_UNKNOWN_FORMAT:
            return "unknown format";
        case VIRGIL_INVALID_IMAGE:
            return "invalid image";
        case VIRGIL_ORDER_OF_OPERATIONS:
            return "order of operations violation";
        case VIRGIL_XMODEM_ERROR:
            return "xmodem transfer error";
        case VIRGIL_DDR_INIT:
            return "ddr initialization failed";
        default:
            return "unknown error";
    }
}
