#include "pcapcie/tlp.h"

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