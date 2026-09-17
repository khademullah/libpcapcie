#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pcapcie/backend.h"
#include "pcapcie/pcapcie.h"

static uint32_t dummy_parse_u32(const char *value, uint32_t def)
{
    char *end = NULL;
    unsigned long v;

    if (!value || !value[0]) {
        return def;
    }

    v = strtoul(value, &end, 0);
    return end && *end == '\0' ? (uint32_t)v : def;
}

static void dummy_tolower_copy(char *out, size_t out_size, const char *in)
{
    size_t i = 0;

    if (!out || out_size == 0 || !in) {
        return;
    }

    while (in[i] != '\0' && i + 1 < out_size) {
        out[i] = (char)tolower((unsigned char)in[i]);
        i++;
    }
    out[i] = '\0';
}

static int dummy_profile_from_name(const char *name,
                                  uint32_t *max_speed,
                                  uint32_t *neg_speed,
                                  uint32_t *max_width,
                                  uint32_t *neg_width)
{
    char key[32];
    uint32_t speed = 0;
    uint32_t width = 0;

    if (!name || !max_speed || !neg_speed || !max_width || !neg_width) {
        return -1;
    }

    dummy_tolower_copy(key, sizeof(key), name);
    if (strcmp(key, "gen5x1") == 0 || strcmp(key, "5x1") == 0) {
        speed = 5; width = 1;
    } else if (strcmp(key, "gen6x4") == 0 || strcmp(key, "6x4") == 0) {
        speed = 6; width = 4;
    } else if (strcmp(key, "gen7x8") == 0 || strcmp(key, "7x8") == 0) {
        speed = 7; width = 8;
    } else if (strcmp(key, "gen8x16") == 0 || strcmp(key, "8x16") == 0) {
        speed = 8; width = 16;
    } else if (strcmp(key, "gen8x8") == 0 || strcmp(key, "8x8") == 0) {
        speed = 8; width = 8;
    } else if (sscanf(key, "gen%ux%u", &speed, &width) == 2) {
        /* valid */
    } else if (sscanf(key, "%ux%u", &speed, &width) == 2) {
        /* valid */
    } else {
        return -1;
    }

    if (speed < 1 || speed > 8 || width < 1 || width > 16) {
        return -1;
    }

    *max_speed = speed;
    *neg_speed = speed;
    *max_width = width;
    *neg_width = width;
    return 0;
}

static int dummy_device_info(pcie_device_info_t *info)
{
    uint16_t vendor_id = (uint16_t)dummy_parse_u32(getenv("PCIE_DUMMY_VENDOR_ID"), 0x1AE0);
    uint16_t device_id = (uint16_t)dummy_parse_u32(getenv("PCIE_DUMMY_DEVICE_ID"), 0x0001);
    uint16_t class_code = (uint16_t)dummy_parse_u32(getenv("PCIE_DUMMY_CLASS_CODE"), 0x060400);
    uint8_t rev_id = (uint8_t)dummy_parse_u32(getenv("PCIE_DUMMY_REVISION"), 0x01);

    if (!info) {
        return -1;
    }

    memset(info, 0, sizeof(*info));
    info->vendor_id = vendor_id;
    info->device_id = device_id;
    info->class_code = class_code;
    info->revision_id = rev_id;
    info->has_pcie_capability = 1;

    pcie_log(PCIE_LOG_INFO,
             "[dummy] Device vendor=0x%04x device=0x%04x class=0x%06x rev=0x%02x pcie_cap=yes",
             info->vendor_id,
             info->device_id,
             info->class_code,
             info->revision_id);
    return 0;
}

static int dummy_link_status(pcie_link_status_t *status)
{
    uint32_t max_speed_code = dummy_parse_u32(getenv("PCIE_DUMMY_MAX_SPEED"), 0);
    uint32_t neg_speed_code = dummy_parse_u32(getenv("PCIE_DUMMY_NEG_SPEED"), 0);
    uint32_t max_width = dummy_parse_u32(getenv("PCIE_DUMMY_MAX_WIDTH"), 0);
    uint32_t neg_width = dummy_parse_u32(getenv("PCIE_DUMMY_NEG_WIDTH"), 0);
    const char *profile_name = getenv("PCIE_DUMMY_PROFILE");

    if (!status) {
        return -1;
    }

    if (profile_name && profile_name[0] != '\0') {
        uint32_t profile_max_speed = 0;
        uint32_t profile_neg_speed = 0;
        uint32_t profile_max_width = 0;
        uint32_t profile_neg_width = 0;

        if (dummy_profile_from_name(profile_name,
                                    &profile_max_speed,
                                    &profile_neg_speed,
                                    &profile_max_width,
                                    &profile_neg_width) == 0) {
            max_speed_code = profile_max_speed;
            neg_speed_code = profile_neg_speed;
            max_width = profile_max_width;
            neg_width = profile_neg_width;
            pcie_log(PCIE_LOG_INFO,
                     "[dummy] Active profile '%s' -> %s/%u lanes",
                     profile_name,
                     pcie_link_speed_name(pcie_link_speed_from_code((uint8_t)max_speed_code)),
                     max_width);
        } else {
            pcie_log(PCIE_LOG_WARN,
                     "[dummy] Unknown profile '%s'; using explicit env values if present",
                     profile_name);
        }
    }

    if (max_speed_code == 0) {
        max_speed_code = 5;
    }
    if (neg_speed_code == 0) {
        neg_speed_code = max_speed_code;
    }
    if (max_width == 0) {
        max_width = 16;
    }
    if (neg_width == 0) {
        neg_width = max_width;
    }

    memset(status, 0, sizeof(*status));
    status->max_link_speed = pcie_link_speed_from_code((uint8_t)max_speed_code);
    status->negotiated_link_speed = pcie_link_speed_from_code((uint8_t)neg_speed_code);
    status->max_link_width = (uint8_t)(max_width > 0 ? max_width : 1);
    status->negotiated_link_width = (uint8_t)(neg_width > 0 ? neg_width : 1);
    status->max_gen = pcie_gen_from_speed_code((uint8_t)max_speed_code);
    status->negotiated_gen = pcie_gen_from_speed_code((uint8_t)neg_speed_code);

    pcie_log(PCIE_LOG_INFO,
             "[dummy] Link status: max=%s/%u lanes, negotiated=%s/%u lanes",
             pcie_link_speed_name(status->max_link_speed),
             status->max_link_width,
             pcie_link_speed_name(status->negotiated_link_speed),
             status->negotiated_link_width);
    return 0;
}

static int dummy_send(const pcie_tlp_t *t)
{
     pcie_log(PCIE_LOG_DEBUG,
             "[dummy] TX TLP type=%d len=%u",
             t->type, t->length);
    return 0;
}

static int dummy_recv(pcie_tlp_t *t)
{
    static int count = 0;

    if (count >= 1000)
        return -1;

    memset(t, 0, sizeof(*t));
    t->type = PCIE_TLP_MEM_READ;
    t->length = 4;
    t->mem.addr = 0x1000 + count * 4;

    count++;
    return 0;
}

static void dummy_close(void)
{
    fprintf(stderr, "[dummy] close\n");
}

static const pcie_backend_ops_t ops = {
    .send  = dummy_send,
    .recv  = dummy_recv,
    .link_status = dummy_link_status,
    .device_info = dummy_device_info,
    .close = dummy_close
};

const pcie_backend_ops_t *pcie_backend_dummy(void)
{
    return &ops;
}
