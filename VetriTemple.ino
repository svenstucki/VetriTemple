/*

  - Select LOLIN(WEMOS) D1 R2 & mini + defaults
  - Serial out is 74880 baud on boot, then 115200
  - Install PubSub library (Sketch -> Include Library -> Manage Libraries -> search PubSub, author is Nick O'Leary)


  Includes code from: https://github.com/sparkfun/SparkFun_AutoDriver_Arduino_Library/tree/V_1.3.2 -> see beerware license

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include "SparkFunAutoDriver.h"

#include "config.h"


WiFiClient wifiClient;

PubSubClient client(wifiClient);
long lastMsg = 0;
char msg[50];
int value = 0;

AutoDriver stepper(0, 15);


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

void setup_stepper() {
  // set CS, MISO, MOSI, SCK
  pinMode(15, OUTPUT); // CS
  pinMode(12, INPUT);  // MISO
  pinMode(13, OUTPUT); // MOSI
  pinMode(14, OUTPUT); // SCK

  SPI.begin();
  SPI.setDataMode(SPI_MODE3);

  stepper.resetDev();
  stepper.configStepMode(STEP_FS);   // 0 microsteps per step
  stepper.setMaxSpeed(10000);        // 10000 steps/s max
  stepper.setFullSpeed(10000);       // microstep below 10000 steps/s
  stepper.setAcc(10000);             // accelerate at 10000 steps/s/s
  stepper.setDec(10000);
  stepper.setSlewRate(SR_530V_us);   // Upping the edge speed increases torque.
  stepper.setOCThreshold(OC_750mA);  // OC threshold 750mA
  stepper.setPWMFreq(PWM_DIV_2, PWM_MUL_2); // 31.25kHz PWM freq
  stepper.setOCShutdown(OC_SD_DISABLE); // don't shutdown on OC
  stepper.setVoltageComp(VS_COMP_DISABLE); // don't compensate for motor V
  stepper.setSwitchMode(SW_USER);    // Switch is not hard stop
  stepper.setOscMode(EXT_16MHZ_OSCOUT_INVERT); // for stepper, we want 16MHz
                                    //  external osc, 16MHz out. boardB
                                    //  will be the same in all respects
                                    //  but this, as it will generate the
                                    //  clock.
  stepper.setAccKVAL(128);           // We'll tinker with these later, if needed.
  stepper.setDecKVAL(128);
  stepper.setRunKVAL(128);
  stepper.setHoldKVAL(32);           // This controls the holding current; keep it low.
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

    stepper.move(FWD, 1000);
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

