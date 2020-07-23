#pragma once
#include "relay_server.h"

void platform_setup(void);
void platform_poll(void);
void platform_report(event_type evt, ...);
int platform_get_pin(enum chameleon_pin pin);


