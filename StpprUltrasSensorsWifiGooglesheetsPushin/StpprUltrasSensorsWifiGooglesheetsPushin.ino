/*************************************************
  SMART IoT PET FEEDER SYSTEM (TELEGRAM + SHEETS)
*************************************************/

#include <Wire.h>
#include "rgb_lcd.h"
#include <Stepper.h>
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "arduino_secrets.h"

// ================= GOOGLE SHEETS =================
const char* GOOGLE_SCRIPT_URL =
"https://script.google.com/macros/s/AKfycbyFU1at5o2xpYCwut_74_bduKa6QreYbNO43RzLcGlWI1ekzcSS2CZFptPdo5wP-Fe0QQ/exec";

// ================= TELEGRAM =================
const char* TELEGRAM_TOKEN = SECRET_TELEGRAM_TOKEN;
const char* CHAT_ID = SECRET_CHAT_ID;

// ================= HARDWARE =================
rgb_lcd lcd;

#define ULTRASONIC_PIN 7
const int buzzerPin = 6;
Stepper myStepper(200, 8, 9, 10, 11);

// ================= WIFI =================
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// ================= STATE =================
bool dispensing = false;
unsigned long lastTriggerTime = 0;
const unsigned long cooldown = 4000;

long lastDistance = -1;

// ======================================================
// SENSOR
// ======================================================
long readDistance() {

  pinMode(ULTRASONIC_PIN, OUTPUT);
  digitalWrite(ULTRASONIC_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_PIN, LOW);

  pinMode(ULTRASONIC_PIN, INPUT);

  long duration = pulseIn(ULTRASONIC_PIN, HIGH, 15000);
  long distance = duration * 0.034 / 2;

  if (distance <= 0 || distance > 200) return -1;

  return distance;
}

// ======================================================
// GOOGLE SHEETS
// ======================================================
void logSheet(String event, int distance) {

  WiFiSSLClient client;

  String url = String(GOOGLE_SCRIPT_URL) +
               "?event=" + event +
               "&distance=" + String(distance);

  Serial.println("Sheets: " + url);

  if (client.connect("script.google.com", 443)) {
    client.println("GET " + url + " HTTP/1.1");
    client.println("Host: script.google.com");
    client.println("Connection: close");
    client.println();
  }

  client.stop();
}

// ======================================================
// TELEGRAM
// ======================================================
void sendTelegram(String message) {

  WiFiSSLClient client;

  String url = "/bot" + String(TELEGRAM_TOKEN) +
               "/sendMessage?chat_id=" + String(CHAT_ID) +
               "&text=" + message;

  Serial.println("Telegram: " + url);

  if (client.connect("api.telegram.org", 443)) {
    client.println("GET " + url + " HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Connection: close");
    client.println();
  }

  client.stop();
}

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.setRGB(0, 255, 0);
  lcd.print("Starting...");

  pinMode(buzzerPin, OUTPUT);
  myStepper.setSpeed(90);

  WiFi.begin(ssid, pass);

  Serial.print("Connecting WiFi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi CONNECTED");
    sendTelegram("🟢 Pet feeder system online");
  } else {
    Serial.println("WiFi FAILED");
  }

  timeClient.begin();

  delay(2000);

  lcd.clear();
  lcd.print("READY");
}

// ======================================================
// LOOP
// ======================================================
void loop() {

  timeClient.update();

  int hour = (timeClient.getHours() + 1) % 24;
  int minute = timeClient.getMinutes();

  lastDistance = readDistance();

  Serial.print("Distance: ");
  Serial.println(lastDistance);

  // ======================================================
  // TRIGGER
  // ======================================================
  if (!dispensing &&
      lastDistance > 0 &&
      lastDistance <= 12 &&
      (millis() - lastTriggerTime > cooldown)) {

    dispensing = true;
    lastTriggerTime = millis();

    lcd.clear();
    lcd.print("Dispensing...");

    sendTelegram("🐶 Pet detected! Distance: " + String(lastDistance) + "cm");
    logSheet("TRIGGER", lastDistance);
  }

  // ======================================================
  // DISPENSE
  // ======================================================
  if (dispensing) {

    for (int i = 0; i < 800; i++) {
      myStepper.step(1);
      delay(2);
    }

    dispensing = false;

    sendTelegram("🍖 Food dispensed successfully!");
    logSheet("DONE", lastDistance);

    tone(buzzerPin, 1000);
    delay(250);
    noTone(buzzerPin);

    lcd.clear();
    lcd.print("DONE");
  }

  // ======================================================
  // DISPLAY
  // ======================================================
  if (!dispensing) {

    lcd.setCursor(0, 0);
    lcd.print("Time ");
    if (hour < 10) lcd.print("0");
    lcd.print(hour);
    lcd.print(":");
    if (minute < 10) lcd.print("0");
    lcd.print(minute);

    lcd.setCursor(0, 1);
    lcd.print("Dist ");
    lcd.print(lastDistance);
    lcd.print("cm   ");
  }
}