#include <stdio.h>
#include "pcapcie/pcapcie.h"

void on_mem_read(pcie_tlp_t *t, void *user)
{
    printf("sniffed MRd addr=0x%lx len=%u\n",
           t->mem.addr, t->length);
}

int main(void)
{
    pcie_ctx_t *ctx = pcie_open("dummy");

    pcie_sniff(ctx,
               PCIE_MATCH_TYPE(PCIE_TLP_MEM_READ),
               on_mem_read,
               NULL);

    uint8_t data[4] = {0xde, 0xad, 0xbe, 0xef};
    pcie_tlp_t wr =
        pcie_tlp_mem_write(0x80000000, 4, data);

    pcie_send(ctx, &wr);
    pcie_loop(ctx);
    pcie_close(ctx);
    return 0;
}