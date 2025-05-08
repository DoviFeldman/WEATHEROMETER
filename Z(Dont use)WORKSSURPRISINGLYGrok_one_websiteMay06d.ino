#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// THIS WORKS AMAZING!!! BUT ITS NOT PRACTICAL FOR NOW, IT MAKES ITS OWN WIFI WHILE ALSO BEING CONNECTED TO THE HOME WIFI SIMOTANIOUSLY AND YOU CAN UPDATE IT AND IT WORKS SUPER WELL AND PERFECTLY!!!!!

WebServer server(80);
Servo tempServo;
Servo iconServo;

// Pin definitions
const int tempServoPin = 13;
const int iconServoPin = 14;
const int ledPin = 2;  // Built-in LED, assumed on pin 2

// Wi-Fi and configuration
String staSSID = "";
String staPassword = "";
String zipcode = "";

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

  // Set LED pin
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);  // LED on (AP mode, assuming active high)

  // Attach servos
  tempServo.attach(tempServoPin);
  iconServo.attach(iconServoPin);

  // Start Wi-Fi in AP_STA mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("WEATHEROMETER");

  // Set up web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setWiFi", HTTP_POST, handleSetWiFi);
  server.on("/setZip", HTTP_POST, handleSetZip);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  // Update LED based on Wi-Fi connection status
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledPin, HIGH);  // LED on when in AP mode
  } else {
    digitalWrite(ledPin, LOW);   // LED off when connected to home Wi-Fi
  }
  delay(10);  // Prevent watchdog timeout
}

void handleRoot() {
  server.send(200, "text/html", generateHTML());
}

void handleSetWiFi() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("zipcode")) {
    staSSID = server.arg("ssid");
    staPassword = server.arg("password");
    zipcode = server.arg("zipcode");

    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to Wi-Fi");
      fetchWeatherData();
      moveServos(currentTemp, currentIcon);
    }
    // Send updated page regardless of connection success
    server.send(200, "text/html", generateHTML());
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleSetZip() {
  if (server.hasArg("zipcode") && WiFi.status() == WL_CONNECTED) {
    zipcode = server.arg("zipcode");
    fetchWeatherData();
    moveServos(currentTemp, currentIcon);
    server.send(200, "text/html", generateHTML());
  } else {
    server.send(200, "text/html", generateHTML());  // Do nothing if not connected
  }
}

String generateHTML() {
  String html = "<html><body>";
  if (WiFi.status() != WL_CONNECTED) {
    // Configuration section active when not connected, with blue border
    html += "<div style='border: 2px solid blue; padding: 10px;'>";
    html += "<h1>Configure Wi-Fi and Zip Code</h1>";
    html += "<form action='/setWiFi' method='post'>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='password'><br>";
    html += "Zip Code: <input type='text' name='zipcode'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form>";
    html += "</div>";
    // Weather section present but inactive
    html += "<div style='padding: 10px;'>";
    html += "<h1>Current Weather</h1>";
    html += "Zip Code: Not set<br>";
    html += "Temperature: N/A<br>";
    html += "Forecast: N/A<br>";
    html += "<form action='/setZip' method='post'>";
    html += "Change Zip Code: <input type='text' name='zipcode'><br>";
    html += "<input type='submit' value='Update'>";
    html += "</form>";
    html += "</div>";
  } else {
    // Configuration section present but inactive
    html += "<div style='border: 2px solid blue; padding: 10px;'>";
    html += "<h1>Configure Wi-Fi and Zip Code</h1>";
    html += "<form action='/setWiFi' method='post'>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='password'><br>";
    html += "Zip Code: <input type='text' name='zipcode'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form>";
    html += "</div>";
    // Weather section active when connected
    html += "<div style='padding: 10px;'>";
    html += "<h1>Current Weather</h1>";
    html += "Zip Code: " + zipcode + "<br>";
    html += "Temperature: " + String(currentTemp) + "°F<br>";
    html += "Forecast: " + currentIcon + "<br>";
    html += "<form action='/setZip' method='post'>";
    html += "Change Zip Code: <input type='text' name='zipcode'><br>";
    html += "<input type='submit' value='Update'>";
    html += "</form>";
    html += "</div>";
  }
  html += "</body></html>";
  return html;
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected, skipping weather fetch");
    return;
  }
  HTTPClient http;
  String geoUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + zipcode + "&count=1&format=json";
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
    Serial.println("Invalid zip code or no results");
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
  tempServo.write(tempAngle);
  delay(500);  // Delay to manage current draw
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
