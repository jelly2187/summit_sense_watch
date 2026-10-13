#pragma once

// Print firmware, chip, reset, Flash and PSRAM information once at startup.
void system_monitor_print_boot_info(void);

// Run indefinitely in the calling FreeRTOS task; emits one SYS record per second.
void system_monitor_run_heartbeat(void);
