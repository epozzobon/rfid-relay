#pragma once

#include <Arduino.h>
#include <WiFi.h>

#define RELAY_SIDE_CLIENT 1
#define RELAY_SIDE_SERVER 2

#define RELAY_SIDE RELAY_SIDE_SERVER

typedef struct victim {
  const char *name;
  const uint8_t *aid;
  size_t aid_len;
  size_t aid_resp_len;
} victim_t;

enum chameleon_pin {
  cham_pin_uart_tx,
  cham_pin_uart_rx,
  cham_pin_reset
};

enum message_type {
  // Message types for UDP communication
  mtype_flow_control,
  mtype_challenge,
  mtype_response,
  mtype_unknown
};

enum event_type {
  event_client_connected,
  event_response_received,
  event_udp_received,
  event_webpage_served,
  event_client_lost,

  event_chameleon_on,
  event_chameleon_off,
  event_chameleon_keep_alive,
  event_challenge_received,
  event_chameleon_app_reset,
  event_chameleon_sent_garbage,
};


unsigned long get_time_since_last_udp(void);
bool is_chameleon_busy(void);
unsigned int get_challenge_len(void);
unsigned long get_time_challenge(void);
unsigned long get_time_response(void);
const victim_t *get_victim(void);
const victim_t **get_available_victims(void);
void set_victim(const victim_t *v);