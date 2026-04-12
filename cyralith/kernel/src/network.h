#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>

void network_init(void);
const char* network_backend_name(void);
const char* network_hostname(void);
const char* network_ip(void);
const char* network_gateway(void);
int network_dhcp_enabled(void);
int network_is_ready(void);
int network_set_hostname(const char* hostname);
int network_set_ip(const char* ip);
int network_set_gateway(const char* gateway);
int network_set_dhcp(int enabled);
int network_ping(const char* target, char* out, size_t max);
int network_persistence_available(void);
int network_save_persistent(void);
int network_load_persistent(void);
size_t network_nic_count(void);
const char* network_nic_name(size_t index);
const char* network_nic_driver_hint(size_t index);
void network_nic_location(size_t index, char* out, size_t max);
void network_rescan(void);
#define NETWORK_DRIVER_NONE 0
#define NETWORK_DRIVER_INTEL 1
#define NETWORK_DRIVER_E1000 2
#define NETWORK_DRIVER_PCNET 3
#define NETWORK_DRIVER_RTL8139 4
#define NETWORK_DRIVER_VIRTIO 5

int network_driver_active(void);
int network_driver_kind(void);
int network_driver_preference(void);
int network_set_driver_preference(int driver_kind);
const char* network_driver_preference_name(void);
const char* network_driver_name(void);
const char* network_driver_label(int kind);
int network_cycle_driver_preference(void);
const char* network_mac_address(void);
int network_link_up(void);
int network_bring_up(void);
int network_bring_up_driver(int driver_kind);
int network_driver_shutdown(void);
int network_send_probe(void);

#endif
