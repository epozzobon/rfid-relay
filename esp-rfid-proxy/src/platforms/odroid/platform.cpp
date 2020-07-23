#include "platform.h"
#include "textui.h"

#include <stdarg.h>
#include <Arduino.h>


#define LED_STATUS 2
#define LED_BACKLIGHT 14

#define SOUND_OUT_P 26
#define SOUND_OUT_M 25

#define JOY_Y 35
#define JOY_X 34
#define BTN_MENU 13
#define BTN_VOLUME 0
#define BTN_SELECT 27
#define BTN_START 39
#define BTN_B 33
#define BTN_A 32

#define LOG_ERROR 0x1f
#define LOG_NORMAL 0x0f
#define LOG_SUCCESS 0x2f

#define LOG_WINDOW_C0 20
#define LOG_WINDOW_R0 0
#define LOG_WINDOW_C1 40
#define LOG_WINDOW_R1 30
#define LOW_WINDOW_W ((LOG_WINDOW_C1) - (LOG_WINDOW_C0))
#define LOW_WINDOW_H ((LOG_WINDOW_R1) - (LOG_WINDOW_R0))


struct buttons_state {
  bool up;
  bool down;
  bool left;
  bool right;
  bool menu;
  bool volume;
  bool start;
  bool select;
  bool b;
  bool a;
};


void platform_setup(void) {
  pinMode(LED_STATUS, OUTPUT);
  adcAttachPin(JOY_X);
  adcAttachPin(JOY_Y);
  pinMode(BTN_MENU, INPUT);
  pinMode(BTN_VOLUME, INPUT);
  pinMode(BTN_B, INPUT);
  pinMode(BTN_A, INPUT);
  pinMode(BTN_SELECT, INPUT);
  pinMode(BTN_START, INPUT);

  digitalWrite(LED_STATUS, LOW);

  Serial.println("Initializing LCD...");
  textui_init();
  Serial.println("LCD initialized!");
  textui_clear(0xf0, ' ');
  textui_clear_rect(LOG_WINDOW_R0, LOG_WINDOW_C0, LOG_WINDOW_R1, LOG_WINDOW_C1, 0x0f, ' ');
  textui_printf(0, 0, 0xf0, "NFC RELAY - SERVER");
  Serial.println("Done.");
}


static int lcd_log_printf(uint8_t color, const char *format, ...) {
  va_list arg;
  int ret;
  textui_scroll(LOG_WINDOW_R0, LOG_WINDOW_C0, LOG_WINDOW_R1, LOG_WINDOW_C1);
  textui_printf(LOG_WINDOW_R1-1, LOG_WINDOW_C0, 0x0f, "                    ");
  va_start(arg, format);
  ret = textui_vprintf(LOG_WINDOW_R1-1, LOG_WINDOW_C0, color, format, arg);
  va_end(arg);
  return ret;
}


void platform_report(event_type event, ...) {
  char c;
  va_list arg;
  va_start(arg, event);

  switch (event) {
    case event_client_connected:
      lcd_log_printf(LOG_NORMAL, "Client connected.");
      break;

    case event_response_received:
      lcd_log_printf(LOG_NORMAL, "Response received.");
      break;

    case event_udp_received:
      lcd_log_printf(LOG_NORMAL, "UDP packet incoming.");
      break;

    case event_client_lost:
      lcd_log_printf(LOG_NORMAL, "UDP client lost.");
      break;

    case event_chameleon_on:
      lcd_log_printf(LOG_NORMAL, "Chameleon ON");
      break;

    case event_chameleon_off:
      lcd_log_printf(LOG_NORMAL, "Chameleon OFF");
      break;

    case event_chameleon_keep_alive:
      lcd_log_printf(LOG_NORMAL, "Chameleon KA");
      break;

    case event_challenge_received:
      lcd_log_printf(LOG_NORMAL, "Challenge RECV");
      break;

    case event_chameleon_app_reset:
      lcd_log_printf(LOG_NORMAL, "Chameleon RESET");
      break;

    case event_chameleon_sent_garbage:
      c = va_arg(arg, int);
      lcd_log_printf(LOG_NORMAL, "Chameleon ? %02x", (int) c);
      break;

    default:
      lcd_log_printf(LOG_NORMAL, "Event %d", event);
      break;
  }
  
  va_end(arg);
}


static struct buttons_state last_buttons = {};
static int last_joy_y = 0;
static int last_joy_x = 0;
static void handle_buttons() {
  struct buttons_state buttons;
  int joy_y = analogRead(JOY_Y);
  int joy_x = analogRead(JOY_X);
  buttons.a = digitalRead(BTN_A);
  buttons.b = digitalRead(BTN_B);
  buttons.start = digitalRead(BTN_START);
  buttons.select = digitalRead(BTN_SELECT);
  buttons.menu = digitalRead(BTN_MENU);
  buttons.volume = digitalRead(BTN_VOLUME);

  buttons.up = 0;
  buttons.down = 0;
  buttons.left = 0;
  buttons.right = 0;

  // joy_x and joy_y might take long to stabilize on a value
  if (joy_y != 0) {
    if (joy_y > 1500 && abs(last_joy_y - joy_y) < 30) {
      buttons.up = (joy_y > 3800);
      buttons.down = (joy_y < 2000);
    }
  }
  last_joy_y = joy_y;
  if (joy_x != 0) {
    if (joy_x > 1500 && abs(last_joy_x - joy_x) < 30) {
      buttons.left = (joy_x > 3800);
      buttons.right = (joy_x < 2000);
    }
  }
  last_joy_x = joy_x;

  if (buttons.up && !last_buttons.up) {
    set_victim(get_available_victims()[0]);
  }

  if (buttons.down && !last_buttons.down) {
    set_victim(get_available_victims()[1]);
  }
  last_buttons = buttons;
}


void platform_poll() {
  unsigned int time_since_last_udp = get_time_since_last_udp();
  bool chameleon_busy = is_chameleon_busy();
  unsigned int challenge_len = get_challenge_len();
  unsigned long time_cha = get_time_challenge();
  unsigned long time_res = get_time_response();

  digitalWrite(LED_STATUS, chameleon_busy ? HIGH : LOW);

  if (time_cha != 0) {
    textui_printf(2, 0, 0xf0, "C@ %-16lu", time_cha);
  }
  if (time_res != 0) {
    textui_printf(3, 0, 0xf0, "R@ %-16lu", time_res);
    textui_printf(4, 0, 0xf0, "Delay: %-13lu", time_res - time_cha);
  }
  textui_printf(27, 0, 0xf0, "Victim: %-10s", (unsigned int) get_victim()->name);
  textui_putc(29, 0, time_since_last_udp < 100 ? 0x6f :
      (time_since_last_udp < 5000 ? 0x3f : 0x00), 'U');
  textui_putc(29, 2, chameleon_busy ? 0x1f : 0x00, 'B');
  textui_putc(29, 4, challenge_len > 0 ? 0x2f : 0x00, 'L');

  handle_buttons();

  // Rotating bar animation to check if the screen freezes
  textui_putc(0, 19, 0xf0, "-\\|/"[(millis() >> 7) & 3]);
}

int platform_get_pin(enum chameleon_pin pin) {
  switch (pin) {
    case cham_pin_reset: return 4;
    case cham_pin_uart_tx: return 12;
    case cham_pin_uart_rx: return 15;
    default: return -1;
  }
}

