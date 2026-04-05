#pragma once

typedef enum virgil_chain_type {
    virgil_chain_gs,
    virgil_chain_unknown = 99
} virgil_chain_type_t;

extern virgil_chain_type_t chainload_detect(void* load_addr);
extern int chainload_checksum(virgil_chain_type_t type, void* load_addr, int received_size);
extern void* chainload_memmove(virgil_chain_type_t type, void* load_addr);
