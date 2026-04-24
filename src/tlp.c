#include "pcapcie/tlp.h"
#include <stddef.h>

pcie_tlp_t pcie_tlp_mem_write(uint64_t addr,
                             uint16_t length,
                             uint8_t *data)
{
    pcie_tlp_t t = {0};
    t.type = PCIE_TLP_MEM_WRITE;
    t.length = length;
    t.mem.addr = addr;
    t.mem.data = data;
    return t;
}

pcie_tlp_t pcie_tlp_cfg_write(uint32_t addr,
                             uint16_t length,
                             uint8_t *data)
{
    pcie_tlp_t t = {0};
    t.type = PCIE_TLP_CFG_WRITE;
    t.length = length;
    t.mem.addr = addr;
    t.mem.data = data;
    return t;
}

pcie_tlp_t pcie_tlp_cfg_read(uint32_t addr)
{
    pcie_tlp_t t = {0};
    t.type = PCIE_TLP_CFG_READ;
    t.length = 0;  // No data for read requests
    t.mem.addr = addr;
    t.mem.data = NULL;
    return t;
}
