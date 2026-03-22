#include <HardwareSerial.h>
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <PubSubClient.h>

// -------- BLUETOOTH --------
BluetoothSerial SerialBT;
String readString = "";

// -------- WIFI --------
const char* ssid = "iot";
const char* password = "12345678";

// -------- MQTT --------
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

// -------- PIN DEFINITIONS --------
#define SEATBELT_PIN   5
#define VIBRATION_PIN  18
#define ALCOHOL_PIN    35
#define IR_EYE_PIN     21
#define RELAY_PIN      23
#define BUZZER_PIN     22

// -------- SERIAL OBJECTS --------
HardwareSerial gsm(2);
HardwareSerial gps(1);

// -------- PHONE NUMBERS --------
String guardian = "+917989049838";
String hospital = "+919063355354";

// -------- FLAGS --------
bool alcoholSent = false;
bool drowsySent = false;
bool accidentSent = false;
bool seatbeltLocked = false;

// -------- TIMERS --------
unsigned long seatbeltStart = 0;
bool seatbeltWarning = false;

unsigned long drowsyStart = 0;
bool drowsyWarning = false;


// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);
  SerialBT.begin("ESP32_CAR");

  gsm.begin(9600, SERIAL_8N1, 16, 17);
  gps.begin(9600, SERIAL_8N1, 12, 34);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  pinMode(SEATBELT_PIN, INPUT);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(IR_EYE_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  logEvent("SYSTEM STARTED");
}


// =====================================================
// LOOP
// =====================================================
void loop() {

  if (!client.connected()) reconnect();
  client.loop();

  readBluetooth();
  readGPS();

  int seatbelt = digitalRead(SEATBELT_PIN);
  int vibration = digitalRead(VIBRATION_PIN);
  int eyeBlink = digitalRead(IR_EYE_PIN);
  int alcohol = readAlcohol();

  sendMQTT(seatbelt, alcohol, eyeBlink, vibration);

  // ================= ALCOHOL =================
  if (alcohol > 3500) {

    engineOFF();
    buzzerHigh();

    if (!alcoholSent) {
      logEvent("Alcohol Detected");
      sendSMS(guardian, "Alcohol detected. Engine OFF.");
      alcoholSent = true;
    }
  }

  // ================= DROWSINESS =================
  else if (eyeBlink == LOW) {

    if (!drowsyWarning) {
      drowsyStart = millis();
      drowsyWarning = true;
      logEvent("Drowsiness Warning");
    }

    buzzerBeep();

    if (millis() - drowsyStart >= 5000 && !drowsySent) {

      engineOFF();
      buzzerHigh();

      logEvent("Driver Drowsy - Engine OFF");
      sendSMS(guardian, "Driver Drowsy! Engine stopped.");

      drowsySent = true;
    }
  }

  // ================= SEATBELT =================
  else if (seatbelt == HIGH) {

    if (!seatbeltWarning) {
      seatbeltStart = millis();
      seatbeltWarning = true;
      logEvent("Seatbelt Warning");
    }

    buzzerBeep();

    if (millis() - seatbeltStart >= 5000 && !seatbeltLocked) {

      engineOFF();
      buzzerHigh();

      logEvent("Seatbelt Not Worn - Engine Locked");
      sendSMS(guardian, "Seatbelt not worn.");

      seatbeltLocked = true;
    }
  }

  // ================= ACCIDENT =================
  else if (vibration == HIGH) {

    engineOFF();
    buzzerHigh();

    if (!accidentSent) {

      logEvent("Accident Detected");

      sendSMS(guardian, "Accident detected!");
      sendSMS(hospital, "Accident detected!");

      accidentSent = true;
    }
  }

  // ================= SAFE =================
  else {

    engineON();
    digitalWrite(BUZZER_PIN, LOW);

    alcoholSent = false;
    drowsySent = false;
    accidentSent = false;
    seatbeltLocked = false;
    seatbeltWarning = false;
    drowsyWarning = false;
  }

  delay(200);
}


// =====================================================
// WIFI
// =====================================================
void setup_wifi() {

  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
}


// =====================================================
// MQTT
// =====================================================
void reconnect() {

  while (!client.connected()) {

    Serial.print("Connecting MQTT...");

    if (client.connect("ESP32_CAR")) {
      Serial.println("Connected");
    } else {
      delay(2000);
    }
  }
}

void sendMQTT(int seatbelt, int alcohol, int eye, int vibration) {

  char data[120];

  sprintf(data,
    "{\"seatbelt\":%d,\"alcohol\":%d,\"eye\":%d,\"vibration\":%d}",
    seatbelt, alcohol, eye, vibration
  );

  client.publish("car/data", data);

  Serial.print("MQTT DATA: ");
  Serial.println(data);
}


// =====================================================
// EVENT LOGGER (Serial + MQTT)
// =====================================================
void logEvent(String event) {

  Serial.println(event);

  String payload = "{\"event\":\"" + event + "\"}";
  client.publish("car/events", payload.c_str());
}


// =====================================================
// BLUETOOTH
// =====================================================
void readBluetooth() {

  while (SerialBT.available()) {

    char c = SerialBT.read();
    if (c == '#') break;
    readString += c;
  }

  if (readString.length() > 0) {

    logEvent("BT: " + readString);

    if (readString == "f success") {
      digitalWrite(RELAY_PIN, HIGH);
      delay(5000);
      digitalWrite(RELAY_PIN, LOW);
    }

    if (readString == "buz") {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(3000);
      digitalWrite(BUZZER_PIN, LOW);
    }

    readString = "";
  }
}


// =====================================================
// OTHER FUNCTIONS
// =====================================================
int readAlcohol() {
  int total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(ALCOHOL_PIN);
    delay(10);
  }
  return total / 10;
}

void engineOFF() {
  digitalWrite(RELAY_PIN, LOW);
  logEvent("Engine OFF");
}

void engineON() {
  digitalWrite(RELAY_PIN, HIGH);
  logEvent("Engine ON");
}

void buzzerBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
}

void buzzerHigh() {
  digitalWrite(BUZZER_PIN, HIGH);
}

void readGPS() {
  while (gps.available()) {
    String line = gps.readStringUntil('\n');
    if (line.startsWith("$GPGGA")) {
      Serial.println(line);
    }
  }
}


// =====================================================
// SMS FUNCTION
// =====================================================
void sendSMS(String number, String msg) {

  Serial.println("Sending SMS to: " + number);

  gsm.println("AT+CMGF=1");
  delay(1000);

  gsm.print("AT+CMGS=\"");
  gsm.print(number);
  gsm.println("\"");

  delay(1000);

  gsm.print(msg);
  delay(500);

  gsm.write(26);
  delay(5000);

  while (gsm.available()) {
    Serial.write(gsm.read());
  }

  logEvent("SMS Sent to " + number);
}
