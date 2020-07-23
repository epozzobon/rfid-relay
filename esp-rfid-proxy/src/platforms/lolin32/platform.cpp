#include "platform.h"
#include <Arduino.h>

#define LED_UDP_RX 22  /* BLUE, Onboard */
#define LED_CHA_ON 33  /* GREEN */
#define LED_CHA_APP 25  /* BLUE */
#define LED_CHALLENGE 27  /* RED */
#define LED_OTHER 26  /* YELLOW */

void platform_setup(void) {
  pinMode(LED_CHALLENGE, OUTPUT);
  pinMode(LED_CHA_ON, OUTPUT);
  pinMode(LED_CHA_APP, OUTPUT);
  pinMode(LED_UDP_RX, OUTPUT);
  pinMode(LED_OTHER, OUTPUT);
  digitalWrite(LED_UDP_RX, HIGH);
  digitalWrite(LED_CHA_ON, HIGH);
  digitalWrite(LED_CHA_APP, HIGH);
  digitalWrite(LED_CHALLENGE, HIGH);
  digitalWrite(LED_OTHER, HIGH);
}

void platform_report(event_type event, ...) {
  switch (event) {
    case event_chameleon_on:
      digitalWrite(LED_CHA_ON, LOW);
      digitalWrite(LED_CHA_APP, HIGH);
      break;
    case event_chameleon_off:
      digitalWrite(LED_CHA_ON, HIGH);
      digitalWrite(LED_CHA_APP, HIGH);
      break;
    default:
      /* nothing to do */
      break;
  }
}

void platform_poll() {
  unsigned int time_since_last_udp = get_time_since_last_udp();
  bool chameleon_busy = is_chameleon_busy();
  unsigned int challenge_len = get_challenge_len();

  digitalWrite(LED_UDP_RX, time_since_last_udp < 100 ? LOW : HIGH);
  digitalWrite(LED_CHA_APP, chameleon_busy ? LOW : HIGH);
  digitalWrite(LED_CHALLENGE, challenge_len > 0 ? LOW : HIGH);
}

int platform_get_pin(enum chameleon_pin pin) {
  switch (pin) {
    case cham_pin_reset: return 4;
    case cham_pin_uart_tx: return 17;
    case cham_pin_uart_rx: return 16;
    default: return -1;
  }
}
