#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

WebServer server(80);
Preferences preferences;
Servo tempServo;
Servo iconServo;

// Pin definitions
const int tempServoPin = 13;
const int iconServoPin = 14;

// Wi-Fi and configuration
String staSSID = "";
String staPassword = "";
String city = "";
bool configured = false;

// Weather data
int currentTemp = 0;
String currentIcon = "";

// Temperature to angle mapping
const int tempPoints[] = {15, 32, 45, 55, 65, 75, 85, 95};
const int anglePoints[] = {0, 15, 45, 57, 69, 81, 93, 105};
const int numPoints = 8;

// Icon to angle mapping
const String icons[] = {"Sunny", "Partially Cloudy", "Very Cloudy", "Raining", "Thunderstorm", "Snow"};
const int iconAngles[] = {90, 72, 54, 36, 18, 0};

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give Serial time to initialize
  Serial.println("Setup started");

  // Attach servos
  tempServo.attach(tempServoPin);
  iconServo.attach(iconServoPin);

  // Initialize preferences
  preferences.begin("weather", false);
  staSSID = preferences.getString("ssid", "");
  staPassword = preferences.getString("password", "");
  city = preferences.getString("city", "");

  // Start in AP mode for configuration
  startConfigMode();

  // Wait for configuration
  while (!configured) {
    server.handleClient();
    delay(10);  // Prevent watchdog timeout
  }

  // Connect to Wi-Fi
  connectToWiFi();

  // Fetch weather data
  fetchWeatherData();

  // Move servos with a delay to manage current
  moveServos(currentTemp, currentIcon);
}

void loop() {
  // Empty for this test
}

void startConfigMode() {
  Serial.println("Starting AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config");
  server.on("/", HTTP_GET, []() {
    String html = "<html><body><h1>Configure Wi-Fi and Location</h1>"
                  "<form action='/setWiFi' method='post'>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Password: <input type='password' name='password'><br>"
                  "City: <input type='text' name='city'><br>"
                  "<input type='submit' value='Save'>"
                  "</form></body></html>";
    server.send(200, "text/html", html);
  });
  server.on("/setWiFi", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("city")) {
      staSSID = server.arg("ssid");
      staPassword = server.arg("password");
      city = server.arg("city");
      preferences.putString("ssid", staSSID);
      preferences.putString("password", staPassword);
      preferences.putString("city", city);
      configured = true;
      Serial.println("Configuration saved");
      server.send(200, "text/plain", "Configuration saved");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });
  server.begin();
  Serial.println("Web server started");
}

void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(staSSID.c_str(), staPassword.c_str());
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Failed to connect");
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected, skipping weather fetch");
    return;
  }
  HTTPClient http;
  String geoUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + city + "&count=1&format=json";
  http.begin(geoUrl);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.println("Geocoding failed");
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);
  if (!doc["results"][0]) {
    Serial.println("Invalid city or no results");
    return;
  }
  float lat = doc["results"][0]["latitude"];
  float lon = doc["results"][0]["longitude"];

  String weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 6) + "&longitude=" + String(lon, 6) + "&daily=temperature_2m_max,weathercode&temperature_unit=fahrenheit&forecast_days=1";
  http.begin(weatherUrl);
  httpCode = http.GET();
  if (httpCode != 200) {
    Serial.println("Weather fetch failed");
    http.end();
    return;
  }
  payload = http.getString();
  http.end();

  deserializeJson(doc, payload);
  if (!doc["daily"]) {
    Serial.println("No daily weather data");
    return;
  }
  float maxTemp = doc["daily"]["temperature_2m_max"][0];
  int weatherCode = doc["daily"]["weathercode"][0];
  currentTemp = round(maxTemp);
  currentIcon = mapWeatherCodeToIcon(weatherCode);
  Serial.print("Temp: ");
  Serial.print(currentTemp);
  Serial.print(", Icon: ");
  Serial.println(currentIcon);
}

void moveServos(int temp, String icon) {
  int tempAngle = mapTempToAngle(temp);
  int iconAngle = mapIconToAngle(icon);
  
  // Move temperature servo first
  tempServo.write(tempAngle);
  delay(500);  // Wait 500ms to reduce simultaneous current draw
  
  // Then move icon servo
  iconServo.write(iconAngle);
  
  Serial.print("Servos moved - Temp angle: ");
  Serial.print(tempAngle);
  Serial.print(", Icon angle: ");
  Serial.println(iconAngle);
}

String mapWeatherCodeToIcon(int code) {
  if (code == 0) return "Sunny";
  else if (code == 1 || code == 2) return "Partially Cloudy";
  else if (code == 3 || code == 45 || code == 48) return "Very Cloudy";
  else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "Raining";
  else if (code >= 71 && code <= 77) return "Snow";
  else if (code >= 95 && code <= 99) return "Thunderstorm";
  else return "Sunny";
}

int mapTempToAngle(float temp) {
  if (temp <= tempPoints[0]) return anglePoints[0];
  if (temp >= tempPoints[numPoints - 1]) return anglePoints[numPoints - 1];
  for (int i = 0; i < numPoints - 1; i++) {
    if (temp >= tempPoints[i] && temp < tempPoints[i + 1]) {
      float ratio = (temp - tempPoints[i]) / (tempPoints[i + 1] - tempPoints[i]);
      return anglePoints[i] + ratio * (anglePoints[i + 1] - anglePoints[i]);
    }
  }
  return 0;
}

int mapIconToAngle(String icon) {
  for (int i = 0; i < 6; i++) {
    if (icon == icons[i]) return iconAngles[i];
  }
  return 90;  // Default to Sunny
}
