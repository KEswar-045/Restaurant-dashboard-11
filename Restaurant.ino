#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// --- WiFi credentials ---
const char* ssid = "eswar5448";
const char* password = "eswar5448";

// --- Flask server (LOCAL) ---
// Make sure Flask is running on your PC with: python server.py
// Check your PC IP using "ipconfig" (use the IPv4 address of your Wi-Fi adapter)
//const char* serverURL = "http://192.168.137.1:5000/log";  // <-- HTTP, not HTTPS
const char* serverURL = "http://10.140.255.10:5000/log";


// --- Pins ---
#define LED_PIN D2
#define BUZZER_PIN D1
#define TOUCH_CALL D5
#define TOUCH_RESPOND D6
#define TOUCH_DELIVER D7

// --- NTP client ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 19800, 60000); // IST +5:30

// --- Buzzer ---
bool buzzerOn = false;
unsigned long buzzerStart = 0;
const unsigned long BUZZER_DURATION = 200; // ms

// --- Table state ---
bool tableActive = false; // LED stays ON until closed

// --- Touch button tracking ---
struct TouchButton {
  uint8_t pin;
  bool prevState;
  String event;
  String icon;
};
TouchButton touchButtons[3] = {
  {TOUCH_CALL, HIGH, "Customer_Called", "📞"},
  {TOUCH_RESPOND, HIGH, "Waiter_Responded", "👨‍🍳"},
  {TOUCH_DELIVER, HIGH, "Food_Delivered", "🍽️"}
};

// --- Utility: get current time ---
String getCurrentTime() {
  timeClient.update();
  unsigned long epoch = timeClient.getEpochTime();
  time_t raw = epoch;
  struct tm* ti = localtime(&raw);
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
  return String(buf);
}

// --- Trigger buzzer ---
void ringBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerOn = true;
  buzzerStart = millis();
}

void updateBuzzer() {
  if (buzzerOn && millis() - buzzerStart >= BUZZER_DURATION) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
  }
}

// --- Send event to Flask (HTTP local) ---
void sendEvent(String event, String icon) {
  String timeStr = getCurrentTime();
  Serial.println(event + " " + icon + " at " + timeStr);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected!");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"tableId\":1,\"event\":\"" + event + "\",\"time\":\"" + timeStr + "\",\"icon\":\"" + icon + "\"}";
  Serial.println("Posting to: " + String(serverURL));
  Serial.println("Payload: " + payload);

  int code = http.POST(payload);

  if (code > 0) {
    String resp = http.getString();
    Serial.println("HTTP Response code: " + String(code));
    Serial.println("Server response: " + resp);
    Serial.println("✅ Sent: " + payload);
  } else {
    Serial.println("❌ Failed to send. Error code: " + String(code));
  }

  http.end();

  // Always ring buzzer on event
  ringBuzzer();

  // Keep LED ON until manually closed
  if (event == "Customer_Called") {
    tableActive = true;
    digitalWrite(LED_PIN, HIGH);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Touch sensor inputs
  for (int i = 0; i < 3; i++) {
    pinMode(touchButtons[i].pin, INPUT_PULLUP);
  }

  // Connect WiFi
  Serial.print("Connecting WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected! IP: " + WiFi.localIP().toString());

  timeClient.begin();
}

void loop() {
  updateBuzzer();

  // --- Check touch sensors ---
  for (int i = 0; i < 3; i++) {
    int reading = digitalRead(touchButtons[i].pin);

    if (reading == LOW && touchButtons[i].prevState == HIGH) { // touch detected
      delay(50); // debounce
      Serial.println("Touch sensor " + String(i) + " activated");
      sendEvent(touchButtons[i].event, touchButtons[i].icon);
    }

    touchButtons[i].prevState = reading;
  }

  // --- Serial Monitor for "close 1" ---
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "close 1") {
      tableActive = false;
      digitalWrite(LED_PIN, LOW);
      sendEvent("Table_Closed 💰 Bill", "💵");
      Serial.println("Table 1 Closed 💰 Bill");
    }
  }

  delay(50);
}
