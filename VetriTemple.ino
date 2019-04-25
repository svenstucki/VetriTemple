/*

  - Select LOLIN(WEMOS) D1 R2 & mini + defaults
  - Serial out is 74880 baud on boot, then 115200
  - Install PubSub library (Sketch -> Include Library -> Manage Libraries -> search PubSub, author is Nick O'Leary)
    docs: https://pubsubclient.knolleary.net/index.html
  - Prepare config.h according to template

  - Use http://www.hivemq.com/demos/websocket-client/ for MQTT debugging

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "pins.h"
#include "config.h"


WiFiClient wifiClient;
PubSubClient client(wifiClient);

long lastMsg = 0;
char msg[50];
int count = 0;

int microstep = 4;


void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  Serial.begin(115200);
  delay(100);

  setup_stepper(microstep);
  setup_wifi();
  delay(100);

  client.setServer(CFG_MQTT_SERVER, 1883);
  client.setCallback(callback);
  delay(100);
}

void setup_stepper(int microstep) {
  int ms;
  const int allPins[] = {
    P_nEN,
    P_MS1, P_MS2, P_MS3,
    P_nRST, P_nSLP,
    P_STEP, P_DIR,
  };

  // setup pins as output
  for (int i = 0; i < sizeof(allPins)/sizeof(*allPins); i++) {
    pinMode(allPins[i], OUTPUT);
  }

  digitalWrite(P_nEN, LOW);
  digitalWrite(P_nSLP, HIGH);

  switch (microstep) {
    case 2:
      // half step
      ms = 0x1;
      break;
    case 4:
      // quarter step
      ms = 0x2;
      break;
    case 8:
      // eighth step
      ms = 0x3;
      break;
    case 16:
      // sixteenth step
      ms = 0x7;
      break;
    case 1:
    default:
      // full step
      ms = 0x0;
  }
  digitalWrite(P_MS1, !!(ms & 0x1));
  digitalWrite(P_MS2, !!(ms & 0x2));
  digitalWrite(P_MS3, !!(ms & 0x4));

  digitalWrite(P_STEP, LOW);
  digitalWrite(P_DIR, LOW);

  // reset
  digitalWrite(P_nRST, LOW);
  digitalWrite(P_nRST, HIGH);
}

void setup_wifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(CFG_WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(CFG_WIFI_SSID, CFG_WIFI_PWD);

  Serial.println();
  Serial.print("Waiting for wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("Wifi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void do_steps(int direction, int steps) {
  digitalWrite(P_DIR, direction);
  digitalWrite(P_nEN, LOW);

  steps *= microstep;

  for (int i = 0; i < steps; i++) {
    digitalWrite(P_STEP, HIGH);
    delayMicroseconds(750);
    digitalWrite(P_STEP, LOW);
    delayMicroseconds(750);

    // prevent soft WDT reset
    if (i % 64 == 0) {
      yield();
    }
  }

  digitalWrite(P_nEN, HIGH);
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg >= 60000) {
    lastMsg = now;

    // increase and publish counter
    count++;
    snprintf (msg, 50, "hello #%ld", count);
    Serial.println(msg);
    client.publish("hello", msg);
  }
}


void callback(const char *topic, byte *payload, unsigned int length) {
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String topicStr = topic;
  if (topicStr == "/reward") {
    // distribute reward
    Serial.println("Distributing reward...");
    do_steps(HIGH, 460);
    client.publish("/reward/done", "1");
  } else if (topicStr == "/move") {
    // move a fixed number of steps
    Serial.println("Turning disk...");
    long steps = topicStr.toInt();
    if (steps > 0) {
      do_steps(HIGH, steps);
    } else if (steps < 0) {
      // reverse direction
      do_steps(LOW, -steps);
    }
  }
}

void reconnect() {
  // loop until (re-)connected
  while (!client.connected()) {
    // create random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    Serial.print("Attempting MQTT connection as ");
    Serial.print(clientId.c_str());
    Serial.print(": ");

    // attempt to connect
    if (client.connect(clientId.c_str(), CFG_MQTT_USER, CFG_MQTT_PASSWORD)) {
      Serial.println("connected.");

      client.subscribe("/reward");
      client.subscribe("/move");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
