#pragma once
#include "tlp.h"

typedef struct pcie_backend_ops {
    int (*send)(const pcie_tlp_t *t);
    int (*recv)(pcie_tlp_t *t);
    void (*close)(void);
} pcie_backend_ops_t;

const pcie_backend_ops_t *pcie_backend_dummy(void);
const pcie_backend_ops_t *pcie_backend_fpga(void);
const pcie_backend_ops_t *pcie_backend_armds(void);
const pcie_backend_ops_t *pcie_backend_xgig(void);
const pcie_backend_ops_t *pcie_backend_pci(void);
