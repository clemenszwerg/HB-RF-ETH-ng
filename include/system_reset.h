#pragma once

typedef void (*restart_eth_pause_fn_t)(void);

void full_system_restart();
void set_flash_pause_enabled(bool enabled);
void register_restart_eth_pause_callback(restart_eth_pause_fn_t cb);
