#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "pcapcie/backend.h"
#include "pcapcie/pcapcie.h"

// PCI config backend: use the Linux PCI sysfs config file to perform config reads/writes.
// Change PCI_DEVICE_ID to the local PCI device you want to access.

#define PCI_DEVICE_ID "0000:01:00.0"
#define PCI_CONFIG_PATH "/sys/bus/pci/devices/%s/config"

#define MAX_COMPLETIONS 16

typedef struct {
    pcie_tlp_t tlp;
    uint8_t data[256];
} completion_t;

typedef struct {
    completion_t completions[MAX_COMPLETIONS];
    int head;
    int tail;
    int count;
} pci_backend_state_t;

static pci_backend_state_t pci_state = {0};

static int pci_enqueue_completion(const pcie_tlp_t *cpl) {
    if (pci_state.count >= MAX_COMPLETIONS) {
        pcie_log(PCIE_LOG_ERROR, "[pci] Completion queue full");
        return -1;
    }
    completion_t *comp = &pci_state.completions[pci_state.tail];
    memcpy(&comp->tlp, cpl, sizeof(pcie_tlp_t));
    if (cpl->length > 0 && cpl->mem.data) {
        memcpy(comp->data, cpl->mem.data, cpl->length);
        comp->tlp.mem.data = comp->data;
    }
    pci_state.tail = (pci_state.tail + 1) % MAX_COMPLETIONS;
    pci_state.count++;
    return 0;
}

static int pci_dequeue_completion(pcie_tlp_t *t) {
    if (pci_state.count == 0) {
        return -1;
    }
    completion_t *comp = &pci_state.completions[pci_state.head];
    memcpy(t, &comp->tlp, sizeof(pcie_tlp_t));
    if (t->length > 0) {
        t->mem.data = malloc(t->length);
        if (!t->mem.data) {
            pcie_log(PCIE_LOG_ERROR, "[pci] Memory allocation failed");
            return -1;
        }
        memcpy(t->mem.data, comp->data, t->length);
    }
    pci_state.head = (pci_state.head + 1) % MAX_COMPLETIONS;
    pci_state.count--;
    return 0;
}

static int pci_access_config(uint64_t offset, void *buf, size_t len, int is_write)
{
    char path[256];
    snprintf(path, sizeof(path), PCI_CONFIG_PATH, PCI_DEVICE_ID);

    int fd = open(path, is_write ? O_RDWR : O_RDONLY);
    if (fd < 0) {
        pcie_log(PCIE_LOG_ERROR, "[pci] Failed to open %s: %s", path, strerror(errno));
        return -1;
    }

    if (lseek(fd, offset, SEEK_SET) < 0) {
        pcie_log(PCIE_LOG_ERROR, "[pci] lseek failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    ssize_t result;
    if (is_write) {
        result = write(fd, buf, len);
    } else {
        result = read(fd, buf, len);
    }

    if (result != (ssize_t)len) {
        pcie_log(PCIE_LOG_ERROR, "[pci] %s config access failed: %s",
                 is_write ? "write" : "read",
                 result < 0 ? strerror(errno) : "short transfer");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int pci_send(const pcie_tlp_t *t)
{
    if (!t) {
        pcie_log(PCIE_LOG_ERROR, "[pci] NULL TLP pointer");
        return -1;
    }

    if (t->type == PCIE_TLP_CFG_WRITE) {
        if (t->length == 0 || !t->mem.data) {
            pcie_log(PCIE_LOG_ERROR, "[pci] CFG write requires payload data");
            return -1;
        }
        pcie_log(PCIE_LOG_INFO, "[pci] CFG write offset=0x%lx len=%u", t->mem.addr, t->length);
        return pci_access_config(t->mem.addr, t->mem.data, t->length, 1);
    }

    if (t->type == PCIE_TLP_CFG_READ) {
        uint16_t length = t->length ? t->length : 4;
        uint8_t data[256] = {0};
        if (length > sizeof(data)) {
            pcie_log(PCIE_LOG_ERROR, "[pci] CFG read length too large: %u", length);
            return -1;
        }
        pcie_log(PCIE_LOG_INFO, "[pci] CFG read offset=0x%lx len=%u", t->mem.addr, length);
        if (pci_access_config(t->mem.addr, data, length, 0) != 0) {
            return -1;
        }
        printf("[pci] CFG read result:");
        for (int i = 0; i < length; ++i) {
            printf(" %02x", data[i]);
        }
        printf("\n");
        return 0;
    }

    pcie_log(PCIE_LOG_ERROR, "[pci] Unsupported TLP type %d", t->type);
    return -1;
}

static int pci_recv(pcie_tlp_t *t)
{
    (void)t;
    return -1;
}

static void pci_close(void)
{
    pcie_log(PCIE_LOG_INFO, "[pci] closed");
}

static const pcie_backend_ops_t pci_ops = {
    .send  = pci_send,
    .recv  = pci_recv,
    .close = pci_close
};

const pcie_backend_ops_t *pcie_backend_pci(void)
{
    return &pci_ops;
}
