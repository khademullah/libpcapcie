#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "pcapcie/backend.h"
#include "pcapcie/pcapcie.h"

// Xgig TCP transport backend
// This backend sends serialized TLPs to an Xgig/PlayerPro TCP endpoint.
// Adjust XGIG_HOST and XGIG_PORT to match your board connection.

#define XGIG_HOST "8.8.8.8"
#define XGIG_PORT 1234
#define XGIG_SEND_TIMEOUT_SEC 5
#define XGIG_RECV_TIMEOUT_SEC 1

static int xgig_sock = -1;
static struct sockaddr_in xgig_addr;

static int xgig_connect(void)
{
    if (xgig_sock >= 0) {
        return 0;
    }

    pcie_log(PCIE_LOG_INFO, "[xgig] Connecting to %s:%d", XGIG_HOST, XGIG_PORT);

    xgig_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (xgig_sock < 0) {
        pcie_log(PCIE_LOG_ERROR, "[xgig] Socket creation failed: %s", strerror(errno));
        return -1;
    }

    memset(&xgig_addr, 0, sizeof(xgig_addr));
    xgig_addr.sin_family = AF_INET;
    xgig_addr.sin_port = htons(XGIG_PORT);

    if (inet_pton(AF_INET, XGIG_HOST, &xgig_addr.sin_addr) <= 0) {
        pcie_log(PCIE_LOG_ERROR, "[xgig] Invalid address: %s", XGIG_HOST);
        close(xgig_sock);
        xgig_sock = -1;
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = XGIG_SEND_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(xgig_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(xgig_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(xgig_sock, (struct sockaddr *)&xgig_addr, sizeof(xgig_addr)) < 0) {
        pcie_log(PCIE_LOG_ERROR, "[xgig] Connection failed: %s", strerror(errno));
        close(xgig_sock);
        xgig_sock = -1;
        return -1;
    }

    pcie_log(PCIE_LOG_INFO, "[xgig] Connected to %s:%d", XGIG_HOST, XGIG_PORT);
    return 0;
}

static int xgig_send(const pcie_tlp_t *t)
{
    if (!t) {
        pcie_log(PCIE_LOG_ERROR, "[xgig] NULL TLP pointer");
        return -1;
    }

    if (xgig_connect() != 0) {
        return -1;
    }

    uint8_t header[16];
    header[0] = (uint8_t)t->type;
    uint16_t requester = htons(t->requester_id);
    uint16_t completer = htons(t->completer_id);
    uint16_t length = htons(t->length);
    uint32_t addr_lo = htonl((uint32_t)(t->mem.addr & 0xFFFFFFFF));
    uint32_t addr_hi = htonl((uint32_t)(t->mem.addr >> 32));

    memcpy(&header[1], &requester, sizeof(requester));
    memcpy(&header[3], &completer, sizeof(completer));
    header[5] = t->tag;
    memcpy(&header[6], &length, sizeof(length));
    memcpy(&header[8], &addr_lo, sizeof(addr_lo));
    memcpy(&header[12], &addr_hi, sizeof(addr_hi));

    ssize_t sent = send(xgig_sock, header, sizeof(header), 0);
    if (sent != (ssize_t)sizeof(header)) {
        pcie_log(PCIE_LOG_ERROR, "[xgig] Failed to send header: %s", strerror(errno));
        return -1;
    }

    if (t->length > 0) {
        if (!t->mem.data) {
            pcie_log(PCIE_LOG_ERROR, "[xgig] TLP payload missing");
            return -1;
        }
        sent = send(xgig_sock, t->mem.data, t->length, 0);
        if (sent != (ssize_t)t->length) {
            pcie_log(PCIE_LOG_ERROR, "[xgig] Failed to send payload: %s", strerror(errno));
            return -1;
        }
    }

    pcie_log(PCIE_LOG_INFO, "[xgig] TLP sent type=%d addr=0x%lx len=%u", t->type, t->mem.addr, t->length);
    return 0;
}

static int xgig_recv(pcie_tlp_t *t)
{
    (void)t;
    return -1;
}

static void xgig_close(void)
{
    if (xgig_sock >= 0) {
        close(xgig_sock);
        xgig_sock = -1;
    }
    pcie_log(PCIE_LOG_INFO, "[xgig] closed");
}

static const pcie_backend_ops_t xgig_ops = {
    .send  = xgig_send,
    .recv  = xgig_recv,
    .close = xgig_close
};

const pcie_backend_ops_t *pcie_backend_xgig(void)
{
    return &xgig_ops;
}
