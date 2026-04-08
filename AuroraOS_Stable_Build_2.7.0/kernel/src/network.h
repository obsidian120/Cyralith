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
int network_driver_active(void);
const char* network_driver_name(void);
const char* network_mac_address(void);
int network_link_up(void);
int network_bring_up(void);
int network_send_probe(void);

#endif
