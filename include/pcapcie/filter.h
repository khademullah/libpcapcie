#pragma once
#include "tlp.h"

typedef struct {
    uint32_t mask;
    uint32_t value;
} pcie_filter_t;

#define PCIE_MATCH_TYPE(t) \
    (pcie_filter_t){ .mask = 0xFF, .value = (t) }

int pcie_filter_match(const pcie_filter_t *f,
                      const pcie_tlp_t *t);