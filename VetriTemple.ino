/*

  - Select LOLIN(WEMOS) D1 R2 & mini + defaults
  - Serial out is 74880 baud on boot, then 115200
  - Install PubSub library (Sketch -> Include Library -> Manage Libraries -> search PubSub, author is Nick O'Leary)

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "pins.h"
#include "config.h"


WiFiClient wifiClient;

PubSubClient client(wifiClient);
long lastMsg = 0;
char msg[50];
int value = 0;


void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  Serial.begin(115200);
  delay(100);

  setup_stepper();
  setup_wifi();
  delay(100);

  client.setServer(CFG_MQTT_SERVER, 1883);
  client.setCallback(callback);
  delay(100);
}

void setup_stepper() {
  // setup pins as output
  const int allPins[] = {
    P_EN,
    P_MS1, P_MS2, P_MS3,
    P_nRST, P_nSLP,
    P_STEP, P_DIR,
  };
  for (unsigned int i = 0; i < sizeof(allPins)/sizeof(*allPins); i++) {
    pinMode(allPins[i], OUTPUT);
  }

  digitalWrite(P_EN, HIGH);
  digitalWrite(P_nSLP, HIGH);

  digitalWrite(P_MS1, LOW);
  digitalWrite(P_MS2, LOW);
  digitalWrite(P_MS3, LOW);

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

  for (int i = 0; i < steps; i++) {
    digitalWrite(P_STEP, LOW);
    digitalWrite(P_STEP, HIGH);
    delay(1);
  }
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 50, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);

    do_steps(HIGH, 10);
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // switch LED on or off based on first character of payload
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);
  } else {
    digitalWrite(BUILTIN_LED, HIGH);
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
    if (client.connect(clientId.c_str())) {
      Serial.println("connected.");

      client.publish("outTopic", "hello world");
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
