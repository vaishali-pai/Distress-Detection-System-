#define BLYNK_TEMPLATE_ID "TMPL3tN_SPne5"
#define BLYNK_TEMPLATE_NAME "Distress Detection"
#define BLYNK_AUTH_TOKEN "pJoDLn8QFvPWzf-Kf1dzh50bnOTrlqF8"

#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <Base64.h>
#define WIFI_SSID "Airtel_Fariha"
#define WIFI_PASSWORD "8296867161"
#define TWILIO_SID "ACbfe42e4aded3d76706edde29b880e8d6"
#define TWILIO_AUTH_TOKEN "185d4d6c6478992c93635f7a487922df"
#define TWILIO_PHONE_NUMBER "‪+16282503635‬"
#define DOCTOR_PHONE_NUMBER "‪+917349355637‬"


#define GSR_PIN 34
#define BUTTON_PIN 4


#define GSR_THRESHOLD 2500
#define BPM_THRESHOLD 120


MAX30105 particleSensor;
const byte RATE_SIZE = 10;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;
bool alertTriggered = false;
bool cancelState = false;
bool inCooldown = false;
unsigned long alertStartTime = 0;
unsigned long cooldownStartTime = 0;

unsigned long lastSendTime = 0;

int gsrValue = 0;
bool buttonPressed = false;
bool fingerDetected = false;

BLYNK_WRITE(V4) {
  cancelState = param.asInt();
  if (cancelState) {
    Serial.println(" Alert Cancelled by User!");
  }
}

void sendTwilioMessage(String message) {
  HTTPClient http;
  String url = "https://api.twilio.com/2010-04-01/Accounts/" + String(TWILIO_SID) + "/Messages.json";
  String auth = String(TWILIO_SID) + ":" + String(TWILIO_AUTH_TOKEN);
  String encodedAuth = base64::encode(auth);

  http.begin(url);
  http.addHeader("Authorization", "Basic " + encodedAuth);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "To=" + String(DOCTOR_PHONE_NUMBER) + "&From=" + String(TWILIO_PHONE_NUMBER) + "&Body=" + message;
  int httpResponseCode = http.POST(body);

  if (httpResponseCode > 0) {
    Serial.println("Twilio Message Sent Successfully.");
  } else {
    Serial.print(" Twilio Error. HTTP Code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n WiFi  Has been Connected You can Proceed ");

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(" MAX30102  has not not found");
    while (1);
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
}

void loop() {
  Blynk.run();

  if (inCooldown && millis() - cooldownStartTime < 30000) return;
  if (inCooldown) {
    inCooldown = false;
    Serial.println(" Cooldown Finished.");
  }

  
  buttonPressed = digitalRead(BUTTON_PIN) == LOW;


  gsrValue = analogRead(GSR_PIN);


  long irValue = particleSensor.getIR();
  fingerDetected = irValue > 50000;

  if (fingerDetected && checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute > 20 && beatsPerMinute < 200) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      beatAvg = 0;
      for (byte i = 0; i < RATE_SIZE; i++) beatAvg += rates[i];
      beatAvg /= RATE_SIZE;
    }
  }

  
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();

    Blynk.virtualWrite(V0, fingerDetected ? beatAvg : 0);
    Blynk.virtualWrite(V1, fingerDetected ? beatsPerMinute : 0);
    Blynk.virtualWrite(V2, gsrValue);
    Blynk.virtualWrite(V3, buttonPressed ? 1 : 0);

    Serial.println("----------- STATUS -----------");
    Serial.print("GSR Value     : "); Serial.println(gsrValue);
    Serial.print("BPM           : "); Serial.println(beatsPerMinute);
    Serial.print("Avg BPM       : "); Serial.println(beatAvg);
    
  }

  
  bool gsrAlert = gsrValue > GSR_THRESHOLD;
  bool bpmAlert = beatAvg > BPM_THRESHOLD;

  if ((buttonPressed || gsrAlert || bpmAlert) && !alertTriggered) {
    Serial.println("🚨 Pre-Alert Triggered.");
    Blynk.logEvent("pre_alert");
    alertStartTime = millis();
    alertTriggered = true;
    cancelState = false;
  }

  
  if (alertTriggered && millis() - alertStartTime <= 10000) {
    if (cancelState) {
      alertTriggered = false;
      Serial.println("Alert Cancelled.");
    }
  }

  
  if (alertTriggered && millis() - alertStartTime > 10000) {
    if (!cancelState) {
      Serial.println("🚨 Sending Final Alert (SMS)");
      sendTwilioMessage("🚨 Distress detected! Immediate help needed!");
    }
    alertTriggered = false;
    inCooldown = true;
    cooldownStartTime = millis();
  }
}
