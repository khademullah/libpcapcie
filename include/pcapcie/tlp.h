#pragma once
#include <stdint.h>

typedef enum {
    PCIE_TLP_MEM_READ  = 0x00,
    PCIE_TLP_MEM_WRITE = 0x01,
    PCIE_TLP_CFG_READ  = 0x04,
    PCIE_TLP_CFG_WRITE = 0x05,
    PCIE_TLP_CPL       = 0x0A
} pcie_tlp_type_t;

typedef struct {
    uint64_t addr;
    uint8_t *data;
} pcie_mem_t;

typedef struct {
    pcie_tlp_type_t type;
    uint16_t requester_id;
    uint16_t completer_id;
    uint8_t  tag;
    uint16_t length;

    union {
        pcie_mem_t mem;
    };
} pcie_tlp_t;

/* Convenience constructors */
pcie_tlp_t pcie_tlp_mem_write(uint64_t addr,
                             uint16_t length,
                             uint8_t *data);
pcie_tlp_t pcie_tlp_cfg_write(uint32_t addr,
                             uint16_t length,
                             uint8_t *data);
pcie_tlp_t pcie_tlp_cfg_read(uint32_t addr);
