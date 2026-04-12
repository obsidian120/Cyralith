#include "network.h"
#include "storage.h"
#include "string.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

#define NET_PERSIST_MAGIC 0x3154454EU
#define NET_PERSIST_VERSION 3U
#define NET_PERSIST_SECTORS 2U
#define NET_PERSIST_LBA 28U

#define PCI_CONFIG_ADDRESS 0xCF8U
#define PCI_CONFIG_DATA 0xCFCU
#define NIC_MAX 8U
#define INVALID_NIC_INDEX ((size_t)-1)

#define PCI_COMMAND 0x04U
#define PCI_BAR0 0x10U
#define PCI_INTERRUPT_LINE 0x3CU

#define E1000_REG_CTRL 0x0000U
#define E1000_REG_STATUS 0x0008U
#define E1000_REG_IMC 0x00D8U
#define E1000_REG_TCTL 0x0400U
#define E1000_REG_TIPG 0x0410U
#define E1000_REG_RAL0 0x5400U
#define E1000_REG_RAH0 0x5404U
#define E1000_REG_TDBAL 0x3800U
#define E1000_REG_TDBAH 0x3804U
#define E1000_REG_TDLEN 0x3808U
#define E1000_REG_TDH 0x3810U
#define E1000_REG_TDT 0x3818U

#define E1000_CTRL_RST 0x04000000UL
#define E1000_STATUS_LU 0x00000002UL
#define E1000_TCTL_EN 0x00000002UL
#define E1000_TCTL_PSP 0x00000008UL
#define E1000_TX_CMD_EOP 0x01U
#define E1000_TX_CMD_IFCS 0x02U
#define E1000_TX_CMD_RS 0x08U
#define E1000_TX_STATUS_DD 0x01U
#define E1000_TX_DESC_COUNT 8U
#define E1000_TX_BUF_SIZE 2048U

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int checksum;
    unsigned int reserved;
} network_snapshot_header_t;

typedef struct {
    char hostname[32];
    char ip[24];
    char gateway[24];
    unsigned int dhcp_enabled;
    unsigned int ready;
    unsigned int preferred_driver;
    unsigned int reserved[7];
} network_snapshot_payload_t;

typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint32_t bar0;
    uint8_t irq_line;
    char name[48];
    char hint[64];
} nic_info_t;

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} e1000_tx_desc_t;

static char g_hostname[32] = {'c','y','r','a','l','i','t','h','\0'};
static char g_ip[24] = {'1','2','7','.','0','.','0','.','1','\0'};
static char g_gateway[24] = {'-','\0'};
static int g_dhcp_enabled = 0;
static int g_ready = 1;
static unsigned char persist_buffer[NET_PERSIST_SECTORS * STORAGE_SECTOR_SIZE];
static nic_info_t g_nics[NIC_MAX];
static size_t g_nic_count = 0U;
static int g_driver_active = 0;
static int g_driver_kind = NETWORK_DRIVER_NONE;
static int g_driver_preference = NETWORK_DRIVER_PCNET;
static int g_link_up = 0;
static size_t g_active_nic_index = INVALID_NIC_INDEX;
static volatile uint32_t* g_e1000_mmio = (volatile uint32_t*)0;
static char g_driver_name[48] = {'p','c','i',' ','s','c','a','n',' ','o','n','l','y','\0'};
static char g_mac_text[24] = {'u','n','a','v','a','i','l','a','b','l','e','\0'};
static uint8_t g_mac[6] = {0U, 0U, 0U, 0U, 0U, 0U};
static char g_backend_text[64] = {'C','y','r','a','l','i','t','h','N','e','t',' ','+',' ','P','C','I',' ','s','c','a','n','\0'};
static uint32_t g_tx_tail = 0U;
static e1000_tx_desc_t g_tx_desc[E1000_TX_DESC_COUNT] __attribute__((aligned(16)));
static unsigned char g_tx_buffers[E1000_TX_DESC_COUNT][E1000_TX_BUF_SIZE] __attribute__((aligned(16)));

static void copy_limited(char* dst, const char* src, size_t max) {
    size_t i = 0U;
    if (max == 0U) {
        return;
    }
    while (src[i] != '\0' && i + 1U < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void append_limited(char* dst, size_t max, const char* src) {
    size_t pos = kstrlen(dst);
    size_t i = 0U;
    if (pos >= max) {
        return;
    }
    while (src[i] != '\0' && pos + 1U < max) {
        dst[pos++] = src[i++];
    }
    dst[pos] = '\0';
}

static void zero_bytes(unsigned char* dst, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = 0U;
    }
}

static void copy_bytes(unsigned char* dst, const unsigned char* src, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = src[i];
    }
}

static unsigned int checksum_bytes(const unsigned char* data, size_t count) {
    size_t i;
    unsigned int sum = 0U;
    for (i = 0U; i < count; ++i) {
        sum = (sum << 5U) - sum + (unsigned int)data[i];
    }
    return sum;
}

static void hex_byte(char* out, uint8_t value) {
    static const char map[] = "0123456789ABCDEF";
    out[0] = map[(value >> 4U) & 0x0FU];
    out[1] = map[value & 0x0FU];
}

static void format_mac_text(const uint8_t mac[6], char* out, size_t max) {
    size_t i;
    size_t pos = 0U;
    if (out == (char*)0 || max < 18U) {
        return;
    }
    for (i = 0U; i < 6U; ++i) {
        char pair[2];
        hex_byte(pair, mac[i]);
        if (pos + 3U >= max) {
            break;
        }
        out[pos++] = pair[0];
        out[pos++] = pair[1];
        if (i + 1U < 6U) {
            out[pos++] = ':';
        }
    }
    out[pos] = '\0';
}

static void network_reset_runtime_state(void) {
    g_driver_active = 0;
    g_driver_kind = NETWORK_DRIVER_NONE;
    g_link_up = 0;
    g_active_nic_index = INVALID_NIC_INDEX;
    g_e1000_mmio = (volatile uint32_t*)0;
    g_tx_tail = 0U;
    zero_bytes((unsigned char*)g_mac, sizeof(g_mac));
    copy_limited(g_driver_name, "pci scan only", sizeof(g_driver_name));
    copy_limited(g_mac_text, "unavailable", sizeof(g_mac_text));
    copy_limited(g_backend_text, "CyralithNet + PCI scan", sizeof(g_backend_text));
}

const char* network_driver_label(int kind) {
    switch (kind) {
        case NETWORK_DRIVER_INTEL: return "Intel PRO/1000 compatibility";
        case NETWORK_DRIVER_E1000: return "Intel e1000 pilot";
        case NETWORK_DRIVER_PCNET: return "AMD PCnet safe attach";
        case NETWORK_DRIVER_RTL8139: return "Realtek RTL8139 safe attach";
        case NETWORK_DRIVER_VIRTIO: return "Virtio-net preview attach";
        case NETWORK_DRIVER_NONE:
        default: return "off";
    }
}

static void update_backend_name(void) {
    if (g_driver_active != 0) {
        copy_limited(g_backend_text, "CyralithNet + ", sizeof(g_backend_text));
        append_limited(g_backend_text, sizeof(g_backend_text), network_driver_label(g_driver_kind));
        return;
    }
    if (g_nic_count > 0U) {
        copy_limited(g_backend_text, "CyralithNet + PCI scan (intel/e1000/pcnet/rtl8139/virtio)", sizeof(g_backend_text));
        return;
    }
    copy_limited(g_backend_text, "CyralithNet + PCI scan", sizeof(g_backend_text));
}

static uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000UL
        | ((uint32_t)bus << 16U)
        | ((uint32_t)slot << 11U)
        | ((uint32_t)func << 8U)
        | ((uint32_t)offset & 0xFCU);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000UL
        | ((uint32_t)bus << 16U)
        | ((uint32_t)slot << 11U)
        | ((uint32_t)func << 8U)
        | ((uint32_t)offset & 0xFCU);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t value = pci_read_dword(bus, slot, func, offset);
    return (uint16_t)((value >> ((offset & 2U) * 8U)) & 0xFFFFU);
}

static void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t dword = pci_read_dword(bus, slot, func, offset);
    uint32_t shift = ((uint32_t)(offset & 2U)) * 8U;
    dword &= ~(0xFFFFUL << shift);
    dword |= ((uint32_t)value << shift);
    pci_write_dword(bus, slot, func, offset, dword);
}

static void pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t command = pci_read_word(bus, slot, func, PCI_COMMAND);
    command = (uint16_t)(command | 0x0006U);
    pci_write_word(bus, slot, func, PCI_COMMAND, command);
}

static void nic_add(uint16_t vendor, uint16_t device, uint8_t bus, uint8_t slot, uint8_t func, uint32_t bar0, uint8_t irq_line, const char* name, const char* hint) {
    nic_info_t* nic;
    if (g_nic_count >= NIC_MAX) {
        return;
    }
    nic = &g_nics[g_nic_count++];
    nic->vendor = vendor;
    nic->device = device;
    nic->bus = bus;
    nic->slot = slot;
    nic->func = func;
    nic->bar0 = bar0;
    nic->irq_line = irq_line;
    copy_limited(nic->name, name, sizeof(nic->name));
    copy_limited(nic->hint, hint, sizeof(nic->hint));
}

static void nic_scan(void) {
    uint8_t bus;
    g_nic_count = 0U;
    for (bus = 0U; bus < 0xFFU; ++bus) {
        uint8_t slot;
        for (slot = 0U; slot < 32U; ++slot) {
            uint8_t func;
            for (func = 0U; func < 8U; ++func) {
                uint32_t id = pci_read_dword(bus, slot, func, 0x00U);
                uint16_t vendor = (uint16_t)(id & 0xFFFFU);
                uint16_t device = (uint16_t)((id >> 16U) & 0xFFFFU);
                uint32_t class_reg;
                uint8_t base_class;
                uint8_t subclass;
                uint32_t bar0;
                uint8_t irq_line;
                if (vendor == 0xFFFFU) {
                    if (func == 0U) {
                        break;
                    }
                    continue;
                }
                class_reg = pci_read_dword(bus, slot, func, 0x08U);
                base_class = (uint8_t)((class_reg >> 24U) & 0xFFU);
                subclass = (uint8_t)((class_reg >> 16U) & 0xFFU);
                if (base_class != 0x02U) {
                    continue;
                }
                bar0 = pci_read_dword(bus, slot, func, PCI_BAR0);
                irq_line = (uint8_t)(pci_read_dword(bus, slot, func, PCI_INTERRUPT_LINE) & 0xFFU);
                if (vendor == 0x10ECU && device == 0x8139U) {
                    nic_add(vendor, device, bus, slot, func, bar0, irq_line, "Realtek RTL8139", "supported: use 'netdriver rtl8139' then 'netup' for safe attach");
                    continue;
                }
                if (vendor == 0x8086U && device == 0x100EU) {
                    nic_add(vendor, device, bus, slot, func, bar0, irq_line, "Intel PRO/1000 82540EM", "supported: use 'netup' for the e1000 pilot driver");
                    continue;
                }
                if (vendor == 0x1022U && device == 0x2000U) {
                    nic_add(vendor, device, bus, slot, func, bar0, irq_line, "AMD PCnet/Am79C973", "supported: use 'netdriver pcnet' then 'netup' for safe attach");
                    continue;
                }
                if (vendor == 0x1AF4U && (device == 0x1000U || device == 0x1041U)) {
                    nic_add(vendor, device, bus, slot, func, bar0, irq_line, "Virtio network adapter", "preview: use 'netdriver virtio' then 'netup' for detect-only attach");
                    continue;
                }
                if (subclass == 0x00U) {
                    nic_add(vendor, device, bus, slot, func, bar0, irq_line, "Ethernet controller", "network device detected, but no dedicated driver mapped yet");
                } else {
                    nic_add(vendor, device, bus, slot, func, bar0, irq_line, "Network controller", "network device detected, but no dedicated driver mapped yet");
                }
            }
        }
    }
    update_backend_name();
}

static volatile uint32_t* e1000_mmio_base(void) {
    return g_e1000_mmio;
}

static uint32_t e1000_read(uint32_t reg) {
    volatile uint32_t* base = e1000_mmio_base();
    if (base == (volatile uint32_t*)0) {
        return 0U;
    }
    return base[reg / 4U];
}

static void e1000_write(uint32_t reg, uint32_t value) {
    volatile uint32_t* base = e1000_mmio_base();
    if (base == (volatile uint32_t*)0) {
        return;
    }
    base[reg / 4U] = value;
}

static void e1000_delay(unsigned int rounds) {
    unsigned int i;
    for (i = 0U; i < rounds; ++i) {
        (void)e1000_read(E1000_REG_STATUS);
    }
}

static int find_e1000_index(void) {
    size_t i;
    for (i = 0U; i < g_nic_count; ++i) {
        if (g_nics[i].vendor == 0x8086U && g_nics[i].device == 0x100EU) {
            return (int)i;
        }
    }
    return -1;
}

static int find_pcnet_index(void) {
    size_t i;
    for (i = 0U; i < g_nic_count; ++i) {
        if (g_nics[i].vendor == 0x1022U && g_nics[i].device == 0x2000U) {
            return (int)i;
        }
    }
    return -1;
}

static int find_rtl8139_index(void) {
    size_t i;
    for (i = 0U; i < g_nic_count; ++i) {
        if (g_nics[i].vendor == 0x10ECU && g_nics[i].device == 0x8139U) {
            return (int)i;
        }
    }
    return -1;
}

static int find_virtio_index(void) {
    size_t i;
    for (i = 0U; i < g_nic_count; ++i) {
        if (g_nics[i].vendor == 0x1AF4U && (g_nics[i].device == 0x1000U || g_nics[i].device == 0x1041U)) {
            return (int)i;
        }
    }
    return -1;
}

static void e1000_capture_mac(void) {
    uint32_t ral = e1000_read(E1000_REG_RAL0);
    uint32_t rah = e1000_read(E1000_REG_RAH0);
    g_mac[0] = (uint8_t)(ral & 0xFFU);
    g_mac[1] = (uint8_t)((ral >> 8U) & 0xFFU);
    g_mac[2] = (uint8_t)((ral >> 16U) & 0xFFU);
    g_mac[3] = (uint8_t)((ral >> 24U) & 0xFFU);
    g_mac[4] = (uint8_t)(rah & 0xFFU);
    g_mac[5] = (uint8_t)((rah >> 8U) & 0xFFU);
    format_mac_text(g_mac, g_mac_text, sizeof(g_mac_text));
}

static void e1000_setup_tx(void) {
    size_t i;
    for (i = 0U; i < E1000_TX_DESC_COUNT; ++i) {
        g_tx_desc[i].addr = (uint32_t)(uintptr_t)&g_tx_buffers[i][0];
        g_tx_desc[i].length = 0U;
        g_tx_desc[i].cso = 0U;
        g_tx_desc[i].cmd = 0U;
        g_tx_desc[i].status = E1000_TX_STATUS_DD;
        g_tx_desc[i].css = 0U;
        g_tx_desc[i].special = 0U;
    }
    g_tx_tail = 0U;
    e1000_write(E1000_REG_TDBAL, (uint32_t)(uintptr_t)&g_tx_desc[0]);
    e1000_write(E1000_REG_TDBAH, 0U);
    e1000_write(E1000_REG_TDLEN, (uint32_t)sizeof(g_tx_desc));
    e1000_write(E1000_REG_TDH, 0U);
    e1000_write(E1000_REG_TDT, 0U);
    e1000_write(E1000_REG_TIPG, 0x0060200AU);
    e1000_write(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (15U << 4U) | (64U << 12U));
}

void network_init(void) {
    g_ready = 1;
    (void)network_load_persistent();
    network_reset_runtime_state();
    nic_scan();
}

const char* network_backend_name(void) {
    return g_backend_text;
}

const char* network_hostname(void) {
    return g_hostname;
}

const char* network_ip(void) {
    return g_ip;
}

const char* network_gateway(void) {
    return g_gateway;
}

int network_dhcp_enabled(void) {
    return g_dhcp_enabled;
}

int network_is_ready(void) {
    return g_ready;
}

int network_set_hostname(const char* hostname) {
    if (hostname == (const char*)0 || hostname[0] == '\0' || kstrlen(hostname) >= sizeof(g_hostname)) {
        return -1;
    }
    copy_limited(g_hostname, hostname, sizeof(g_hostname));
    (void)network_save_persistent();
    return 0;
}

int network_set_ip(const char* ip) {
    if (ip == (const char*)0 || ip[0] == '\0' || kstrlen(ip) >= sizeof(g_ip)) {
        return -1;
    }
    copy_limited(g_ip, ip, sizeof(g_ip));
    g_dhcp_enabled = 0;
    (void)network_save_persistent();
    return 0;
}

int network_set_gateway(const char* gateway) {
    if (gateway == (const char*)0 || gateway[0] == '\0' || kstrlen(gateway) >= sizeof(g_gateway)) {
        return -1;
    }
    copy_limited(g_gateway, gateway, sizeof(g_gateway));
    (void)network_save_persistent();
    return 0;
}

int network_set_dhcp(int enabled) {
    g_dhcp_enabled = enabled != 0 ? 1 : 0;
    if (g_dhcp_enabled != 0) {
        copy_limited(g_ip, "10.0.2.15", sizeof(g_ip));
        copy_limited(g_gateway, "10.0.2.2", sizeof(g_gateway));
    }
    (void)network_save_persistent();
    return 0;
}

int network_ping(const char* target, char* out, size_t max) {
    if (out == (char*)0 || max == 0U || target == (const char*)0 || target[0] == '\0') {
        return -1;
    }
    out[0] = '\0';
    if (kstrcmp(target, "127.0.0.1") == 0 || kstrcmp(target, "localhost") == 0) {
        append_limited(out, max, "PING ");
        append_limited(out, max, target);
        append_limited(out, max, ": loopback reachable (1 ms)");
        return 0;
    }
    append_limited(out, max, "PING ");
    append_limited(out, max, target);
    if (g_driver_active != 0) {
        append_limited(out, max, ": driver is active, link ");
        append_limited(out, max, g_link_up != 0 ? "up" : "not confirmed");
        if (g_driver_kind == NETWORK_DRIVER_E1000) {
            append_limited(out, max, ", use 'netprobe' for a raw TX test");
        } else if (g_driver_kind == NETWORK_DRIVER_INTEL) {
            append_limited(out, max, ", compatibility mode is up");
        } else {
            append_limited(out, max, ", safe attach only");
        }
        return 1;
    }
    if (g_nic_count > 0U) {
        append_limited(out, max, ": NIC detected (");
        append_limited(out, max, g_nics[0].name);
        append_limited(out, max, "), start 'netup' for the first driver step");
    } else {
        append_limited(out, max, ": groundwork ready, but no detected NIC/driver yet");
    }
    return 1;
}

int network_persistence_available(void) {
    storage_init();
    return storage_available();
}

int network_save_persistent(void) {
    network_snapshot_header_t* header;
    network_snapshot_payload_t* payload;
    size_t offset = sizeof(network_snapshot_header_t);
    size_t payload_size = sizeof(network_snapshot_payload_t);
    unsigned int sector;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (offset + payload_size > sizeof(persist_buffer)) {
        return -1;
    }

    zero_bytes(persist_buffer, sizeof(persist_buffer));
    header = (network_snapshot_header_t*)persist_buffer;
    payload = (network_snapshot_payload_t*)(persist_buffer + offset);

    header->magic = NET_PERSIST_MAGIC;
    header->version = NET_PERSIST_VERSION;
    copy_bytes((unsigned char*)payload->hostname, (const unsigned char*)g_hostname, sizeof(g_hostname));
    copy_bytes((unsigned char*)payload->ip, (const unsigned char*)g_ip, sizeof(g_ip));
    copy_bytes((unsigned char*)payload->gateway, (const unsigned char*)g_gateway, sizeof(g_gateway));
    payload->dhcp_enabled = (unsigned int)g_dhcp_enabled;
    payload->ready = (unsigned int)g_ready;
    payload->preferred_driver = (unsigned int)g_driver_preference;
    header->checksum = checksum_bytes((const unsigned char*)payload, payload_size);

    for (sector = 0U; sector < NET_PERSIST_SECTORS; ++sector) {
        if (storage_write_sector(NET_PERSIST_LBA + sector, persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    return 0;
}

int network_load_persistent(void) {
    network_snapshot_header_t* header = (network_snapshot_header_t*)persist_buffer;
    network_snapshot_payload_t* payload;
    size_t offset = sizeof(network_snapshot_header_t);
    size_t payload_size = sizeof(network_snapshot_payload_t);
    unsigned int checksum;
    unsigned int sector;

    storage_init();
    if (storage_available() == 0) {
        return -1;
    }
    if (offset + payload_size > sizeof(persist_buffer)) {
        return -1;
    }

    for (sector = 0U; sector < NET_PERSIST_SECTORS; ++sector) {
        if (storage_read_sector(NET_PERSIST_LBA + sector, persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }

    if (header->magic != NET_PERSIST_MAGIC || (header->version != 1U && header->version != 2U && header->version != NET_PERSIST_VERSION)) {
        return -1;
    }

    payload = (network_snapshot_payload_t*)(persist_buffer + offset);
    checksum = checksum_bytes((const unsigned char*)payload, payload_size);
    if (checksum != header->checksum) {
        return -1;
    }

    copy_bytes((unsigned char*)g_hostname, (const unsigned char*)payload->hostname, sizeof(g_hostname));
    copy_bytes((unsigned char*)g_ip, (const unsigned char*)payload->ip, sizeof(g_ip));
    copy_bytes((unsigned char*)g_gateway, (const unsigned char*)payload->gateway, sizeof(g_gateway));
    g_dhcp_enabled = payload->dhcp_enabled != 0U ? 1 : 0;
    g_ready = payload->ready != 0U ? 1 : 0;
    if (header->version >= 2U) {
        switch ((int)payload->preferred_driver) {
            case NETWORK_DRIVER_INTEL:
            case NETWORK_DRIVER_E1000:
            case NETWORK_DRIVER_PCNET:
            case NETWORK_DRIVER_RTL8139:
            case NETWORK_DRIVER_VIRTIO:
                g_driver_preference = (int)payload->preferred_driver;
                break;
            default:
                g_driver_preference = header->version >= 3U ? NETWORK_DRIVER_PCNET : NETWORK_DRIVER_E1000;
                break;
        }
    } else {
        g_driver_preference = NETWORK_DRIVER_E1000;
    }
    return 0;
}

size_t network_nic_count(void) {
    return g_nic_count;
}

const char* network_nic_name(size_t index) {
    if (index >= g_nic_count) {
        return "-";
    }
    return g_nics[index].name;
}

const char* network_nic_driver_hint(size_t index) {
    if (index >= g_nic_count) {
        return "-";
    }
    if (g_driver_active != 0 && index == g_active_nic_index) {
        if (g_driver_kind == NETWORK_DRIVER_INTEL) {
            return "Intel compatibility driver active: status + MAC only";
        }
        if (g_driver_kind == NETWORK_DRIVER_E1000) {
            return "e1000 pilot driver active: raw TX probe available";
        }
        if (g_driver_kind == NETWORK_DRIVER_PCNET) {
            return "PCnet safe attach active: MAC visible, no packet engine yet";
        }
        if (g_driver_kind == NETWORK_DRIVER_RTL8139) {
            return "RTL8139 safe attach active: MAC visible, no packet engine yet";
        }
        return "Virtio preview attach active: detection only";
    }
    return g_nics[index].hint;
}

void network_rescan(void) {
    network_reset_runtime_state();
    nic_scan();
}

void network_nic_location(size_t index, char* out, size_t max) {
    nic_info_t* nic;
    char temp[24];
    if (out == (char*)0 || max == 0U) {
        return;
    }
    out[0] = '\0';
    if (index >= g_nic_count) {
        copy_limited(out, "-", max);
        return;
    }
    nic = &g_nics[index];
    copy_limited(out, "PCI ", max);
    temp[0] = '\0';
    temp[0] = (char)('0' + ((nic->bus / 10U) % 10U));
    temp[1] = (char)('0' + (nic->bus % 10U));
    temp[2] = ':';
    temp[3] = (char)('0' + ((nic->slot / 10U) % 10U));
    temp[4] = (char)('0' + (nic->slot % 10U));
    temp[5] = '.';
    temp[6] = (char)('0' + (nic->func % 10U));
    temp[7] = '\0';
    append_limited(out, max, temp);
}

int network_driver_active(void) {
    return g_driver_active;
}

int network_driver_kind(void) {
    return g_driver_kind;
}

int network_driver_preference(void) {
    return g_driver_preference;
}

int network_set_driver_preference(int driver_kind) {
    if (driver_kind != NETWORK_DRIVER_INTEL && driver_kind != NETWORK_DRIVER_E1000 && driver_kind != NETWORK_DRIVER_PCNET && driver_kind != NETWORK_DRIVER_RTL8139 && driver_kind != NETWORK_DRIVER_VIRTIO) {
        return -1;
    }
    g_driver_preference = driver_kind;
    (void)network_save_persistent();
    if (g_driver_active != 0 && g_driver_kind != driver_kind) {
        (void)network_driver_shutdown();
    }
    update_backend_name();
    return 0;
}

const char* network_driver_preference_name(void) {
    return network_driver_label(g_driver_preference);
}

int network_cycle_driver_preference(void) {
    int next = NETWORK_DRIVER_INTEL;
    switch (g_driver_preference) {
        case NETWORK_DRIVER_INTEL: next = NETWORK_DRIVER_E1000; break;
        case NETWORK_DRIVER_E1000: next = NETWORK_DRIVER_PCNET; break;
        case NETWORK_DRIVER_PCNET: next = NETWORK_DRIVER_RTL8139; break;
        case NETWORK_DRIVER_RTL8139: next = NETWORK_DRIVER_VIRTIO; break;
        case NETWORK_DRIVER_VIRTIO:
        default: next = NETWORK_DRIVER_INTEL; break;
    }
    return network_set_driver_preference(next);
}

const char* network_driver_name(void) {
    return g_driver_name;
}

const char* network_mac_address(void) {
    return g_mac_text;
}

int network_link_up(void) {
    return g_link_up;
}

static int io_attach_read_mac(size_t nic_index, const char* driver_name) {
    nic_info_t* nic = &g_nics[nic_index];
    uint32_t bar0;
    uint16_t io_base;
    size_t i;
    if (g_driver_active != 0) {
        return 2;
    }
    pci_enable_device(nic->bus, nic->slot, nic->func);
    bar0 = pci_read_dword(nic->bus, nic->slot, nic->func, PCI_BAR0);
    nic->bar0 = bar0;
    if ((bar0 & 0x1U) == 0U) {
        return -2;
    }
    io_base = (uint16_t)(bar0 & ~0x3U);
    if (io_base == 0U) {
        return -2;
    }
    for (i = 0U; i < 6U; ++i) {
        g_mac[i] = inb((uint16_t)(io_base + (uint16_t)i));
    }
    format_mac_text(g_mac, g_mac_text, sizeof(g_mac_text));
    g_driver_active = 1;
    g_active_nic_index = nic_index;
    g_link_up = 0;
    copy_limited(g_driver_name, driver_name, sizeof(g_driver_name));
    update_backend_name();
    return 0;
}

static int pcnet_safe_bring_up(void) {
    int index = find_pcnet_index();
    if (g_driver_active != 0) {
        return g_driver_kind == NETWORK_DRIVER_PCNET ? 1 : 2;
    }
    if (index < 0) {
        return -1;
    }
    if (io_attach_read_mac((size_t)index, "AMD PCnet safe attach") != 0) {
        return -2;
    }
    g_driver_kind = NETWORK_DRIVER_PCNET;
    return 0;
}

static int rtl8139_safe_bring_up(void) {
    int index = find_rtl8139_index();
    if (g_driver_active != 0) {
        return g_driver_kind == NETWORK_DRIVER_RTL8139 ? 1 : 2;
    }
    if (index < 0) {
        return -1;
    }
    if (io_attach_read_mac((size_t)index, "Realtek RTL8139 safe attach") != 0) {
        return -2;
    }
    g_driver_kind = NETWORK_DRIVER_RTL8139;
    return 0;
}

static int virtio_preview_bring_up(void) {
    int index = find_virtio_index();
    nic_info_t* nic;
    if (g_driver_active != 0) {
        return g_driver_kind == NETWORK_DRIVER_VIRTIO ? 1 : 2;
    }
    if (index < 0) {
        return -1;
    }
    nic = &g_nics[(size_t)index];
    g_driver_active = 1;
    g_driver_kind = NETWORK_DRIVER_VIRTIO;
    g_active_nic_index = (size_t)index;
    g_link_up = 0;
    copy_limited(g_driver_name, "Virtio-net preview attach", sizeof(g_driver_name));
    copy_limited(g_mac_text, "virtio config pending", sizeof(g_mac_text));
    nic->bar0 = pci_read_dword(nic->bus, nic->slot, nic->func, PCI_BAR0);
    update_backend_name();
    return 0;
}

static int intel_compat_bring_up(void) {
    int index = find_e1000_index();
    nic_info_t* nic;
    uint32_t bar0;
    uint32_t status;
    if (g_driver_active != 0) {
        return g_driver_kind == NETWORK_DRIVER_INTEL ? 1 : 2;
    }
    if (index < 0) {
        return -1;
    }
    nic = &g_nics[(size_t)index];
    pci_enable_device(nic->bus, nic->slot, nic->func);
    bar0 = pci_read_dword(nic->bus, nic->slot, nic->func, PCI_BAR0);
    nic->bar0 = bar0;
    if ((bar0 & 0x1U) != 0U) {
        return -2;
    }
    g_e1000_mmio = (volatile uint32_t*)(uintptr_t)(bar0 & ~0x0FU);
    if (g_e1000_mmio == (volatile uint32_t*)0) {
        return -2;
    }
    e1000_write(E1000_REG_IMC, 0xFFFFFFFFUL);
    e1000_capture_mac();
    status = e1000_read(E1000_REG_STATUS);
    g_link_up = (status & E1000_STATUS_LU) != 0U ? 1 : 0;
    g_driver_active = 1;
    g_driver_kind = NETWORK_DRIVER_INTEL;
    g_active_nic_index = (size_t)index;
    copy_limited(g_driver_name, "Intel PRO/1000 compatibility driver", sizeof(g_driver_name));
    update_backend_name();
    return 0;
}

static int e1000_pilot_bring_up(void) {
    int index = find_e1000_index();
    nic_info_t* nic;
    uint32_t bar0;
    uint32_t ctrl;
    uint32_t status;
    if (g_driver_active != 0) {
        return g_driver_kind == NETWORK_DRIVER_E1000 ? 1 : 2;
    }
    if (index < 0) {
        return -1;
    }
    nic = &g_nics[(size_t)index];
    pci_enable_device(nic->bus, nic->slot, nic->func);
    bar0 = pci_read_dword(nic->bus, nic->slot, nic->func, PCI_BAR0);
    nic->bar0 = bar0;
    if ((bar0 & 0x1U) != 0U) {
        return -2;
    }
    g_e1000_mmio = (volatile uint32_t*)(uintptr_t)(bar0 & ~0x0FU);
    if (g_e1000_mmio == (volatile uint32_t*)0) {
        return -2;
    }

    e1000_write(E1000_REG_IMC, 0xFFFFFFFFUL);
    ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | E1000_CTRL_RST);
    e1000_delay(4096U);
    e1000_write(E1000_REG_IMC, 0xFFFFFFFFUL);
    e1000_setup_tx();
    e1000_capture_mac();
    status = e1000_read(E1000_REG_STATUS);
    g_link_up = (status & E1000_STATUS_LU) != 0U ? 1 : 0;
    g_driver_active = 1;
    g_driver_kind = NETWORK_DRIVER_E1000;
    g_active_nic_index = (size_t)index;
    copy_limited(g_driver_name, "Intel e1000 pilot driver", sizeof(g_driver_name));
    update_backend_name();
    return 0;
}

int network_driver_shutdown(void) {
    network_reset_runtime_state();
    update_backend_name();
    return 0;
}

int network_bring_up_driver(int driver_kind) {
    if (driver_kind == NETWORK_DRIVER_INTEL) {
        return intel_compat_bring_up();
    }
    if (driver_kind == NETWORK_DRIVER_E1000) {
        return e1000_pilot_bring_up();
    }
    if (driver_kind == NETWORK_DRIVER_PCNET) {
        return pcnet_safe_bring_up();
    }
    if (driver_kind == NETWORK_DRIVER_RTL8139) {
        return rtl8139_safe_bring_up();
    }
    if (driver_kind == NETWORK_DRIVER_VIRTIO) {
        return virtio_preview_bring_up();
    }
    return -3;
}

int network_bring_up(void) {
    return network_bring_up_driver(g_driver_preference);
}

int network_send_probe(void) {
    unsigned char* frame;
    e1000_tx_desc_t* desc;
    uint32_t index;
    uint32_t next_tail;
    unsigned int wait;
    static const unsigned char payload[] = {
        'C','y','r','a','l','i','t','h',' ','e','1','0','0','0',' ','p','r','o','b','e',' ','f','r','a','m','e','\0'
    };
    size_t payload_len = sizeof(payload) - 1U;
    size_t i;
    if (g_driver_active == 0) {
        return -1;
    }
    if (g_driver_kind != NETWORK_DRIVER_E1000) {
        return -4;
    }
    index = g_tx_tail % E1000_TX_DESC_COUNT;
    desc = &g_tx_desc[index];
    if ((desc->status & E1000_TX_STATUS_DD) == 0U) {
        return -2;
    }
    frame = &g_tx_buffers[index][0];
    for (i = 0U; i < 6U; ++i) {
        frame[i] = 0xFFU;
        frame[6U + i] = g_mac[i];
    }
    frame[12] = 0x88U;
    frame[13] = 0xB5U;
    for (i = 0U; i < payload_len && (14U + i) < 60U; ++i) {
        frame[14U + i] = payload[i];
    }
    while ((14U + i) < 60U) {
        frame[14U + i] = 0U;
        i++;
    }
    desc->length = 60U;
    desc->cmd = (uint8_t)(E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS);
    desc->status = 0U;
    next_tail = (index + 1U) % E1000_TX_DESC_COUNT;
    g_tx_tail = next_tail;
    e1000_write(E1000_REG_TDT, next_tail);
    for (wait = 0U; wait < 1000000U; ++wait) {
        if ((desc->status & E1000_TX_STATUS_DD) != 0U) {
            g_link_up = (e1000_read(E1000_REG_STATUS) & E1000_STATUS_LU) != 0U ? 1 : 0;
            return 0;
        }
    }
    g_link_up = (e1000_read(E1000_REG_STATUS) & E1000_STATUS_LU) != 0U ? 1 : 0;
    return -3;
}
