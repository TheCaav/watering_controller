#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define STASSID "raspi-webgui"
#define STAPSK "BYip3sj5pbnYtswmM3r4"

const char* ssid = STASSID;
const char* password = STAPSK;
const char* mqtt_server = "10.3.141.1";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

#define motorPin D2
// How many Watering cycles
int nCycles = 3;
// Duration of 1 Water cycle
int cycleDuration = 60;
// Pause between cycles
int cyclePause = 20;

void sendStatus() {
  Serial.println("Test status output");
  Serial.println(nCycles);
  Serial.println(cycleDuration);
  Serial.println(cyclePause);

  char telegrafMess[100];
  sprintf(telegrafMess, "wateringController,location=grow,type=Status cycles=%d,cycleDuration=%d,cyclePause=%d,message=\"alive\"", nCycles, cycleDuration, cyclePause);
  client.publish("/telegraf/wateringController", telegrafMess);
}

void doWatering() {
  for (int cycle = 0; cycle < nCycles; cycle++) {
    Serial.println("Watering on");
    digitalWrite(motorPin, HIGH);
    delay(1000 * cycleDuration);
    digitalWrite(motorPin, LOW);
    Serial.println("Watering off");
    delay(1000 * cyclePause);
  }

  char telegrafMess[100];
  sprintf(telegrafMess, "wateringController,location=grow,type=Watering cycles=%d,cycleDuration=%d,cyclePause=%d,message=\"didWatering\"", nCycles, cycleDuration, cyclePause);
  client.publish("/telegraf/wateringController", telegrafMess);

  return;
}

void printMessage(byte* message, unsigned int length) {
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
  }
  Serial.println("");
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  printMessage(message, length);
  
  if (String(topic) == "/family/groom/wateringController/doWatering" || String(topic) == "/family/groom/wateringController/setValues") {
    String splitMessage[3];
    int partNo = 0;long lastMsg = 0;
    char msg[50];

    for (unsigned int i = 0; i < length; i++) {
      if (partNo > 2) {
        Serial.println("Too many message parts");
        return;
      }
      if ((char)message[i] == '/') {
        partNo++;
      } else {
        splitMessage[partNo] += (char)message[i];
      }
    }

    if(partNo != 2) {
      Serial.println("Not enough message parts");
      return;
    }

    if (splitMessage[0].length() > 0) {
      nCycles = splitMessage[0].toInt();
    }
    if (splitMessage[1].length() > 0) {
      cycleDuration = splitMessage[1].toInt();
    }
    if (splitMessage[2].length() > 0) {
      cyclePause = splitMessage[2].toInt();
    }

    if (String(topic) == "/family/groom/wateringController/doWatering") {
      doWatering();
    }
  } else if (String(topic) == "/family/groom/wateringController/statusQuery") {
    sendStatus();
  } else {
    Serial.println("From /family/groom/wateringController/#.");
  }
  return;
}

void mqttconnect() {
  /* Loop until reconnected */
    Serial.print("MQTT connecting ...");
    /* client ID */
    String clientId = "ESP32Client";
    /* connect now */
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("/family/groom/wateringController/#", 0);
      client.publish("/telegraf/wateringController", "wateringController,location=grow,type=alive message=\"alive\"");
    } else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      /* Wait 5 seconds before retrying */
      delay(5000);
    }
}

void setup() {
    delay(3000);
    pinMode(motorPin, OUTPUT);
    Serial.begin(9600);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // You can remove the password parameter if you want the AP to be open. 
    //WiFi.mode(WIFI_STA);
    // IPAddress local_IP(10, 3, 141, 53);
    //IPAddress gateway(10, 3, 141, 1);
    //IPAddress subnet(255, 255, 255, 0);
    //WiFi.config(local_IP, gateway, subnet);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    WiFi.setAutoReconnect(true);

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    mqttconnect();
}


void loop() {
  /* if client was disconnected then try to reconnect again */
  if (!client.connected()) {
    mqttconnect();
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi disconnected. Reconnecting...");
    WiFi.reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 1000 * 60 * 5) {
    lastMsg = now;
    sendStatus();
  }
}