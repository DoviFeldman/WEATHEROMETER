#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Creating pointers to allow recreation of the server
WebServer* server = NULL;
Servo tempServo;
Servo iconServo;

// Pin definitions
const int tempServoPin = 13;
const int iconServoPin = 14;
const int ledPin = 2;  // Assuming built-in blue LED is on pin 2

// Wi-Fi and configuration
String staSSID = "";
String staPassword = "";
String zipCode = "";
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

  // Initialize LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);  // LED on in AP mode

  // Start in AP mode for configuration
  startConfigMode();

  // Wait for configuration
  while (!configured) {
    server->handleClient();
    delay(10);  // Prevent watchdog timeout
  }

  // Connect to Wi-Fi
  connectToWiFi();
  digitalWrite(ledPin, LOW);  // LED off when connected to home Wi-Fi

  // Fetch initial weather data
  fetchWeatherData();
  moveServos(currentTemp, currentIcon);

  // Delete and recreate the server for weather mode
  delete server;
  server = new WebServer(80);
  
  // Set up new routes for the weather server
  setupWeatherServer();
  
  Serial.println("Weather server started");
}

void loop() {
  server->handleClient();  // Handle requests from the weather web server
  
  // Optional: Periodically update weather data (e.g., every hour)
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 3600000) {  // 1 hour
    fetchWeatherData();
    moveServos(currentTemp, currentIcon);
    lastUpdate = millis();
  }
}

void setupWeatherServer() {
  // Set up new routes for weather server (server has been recreated)
  server->on("/", HTTP_GET, handleWeatherPage);
  server->on("/setZip", HTTP_POST, handleSetZip);
  server->on("/setup", HTTP_GET, handleSetup);  // Add setup route
  server->begin();
  Serial.print("Weather server started on IP: ");
  Serial.println(WiFi.localIP());
}

void startConfigMode() {
  Serial.println("Starting AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WEATHEROMETER");  // Open AP with no password
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Create a new server instance for config mode
  server = new WebServer(80);
  
  server->on("/", HTTP_GET, []() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body>"
                  "<h1>Configure Wi-Fi and Location</h1>"
                  "<form action='/setWiFi' method='post'>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Password: <input type='password' name='password'><br>"
                  "Zip Code: <input type='text' name='zipcode'><br>"
                  "<input type='submit' value='Save'>"
                  "</form>"
                  "<div style='text-align: center; margin-top: 20px;'>"
                  "<a href='/setup?step=0'><button>Setup Sequence</button></a>"
                  "</div>"
                  "</body></html>";
    server->send(200, "text/html", html);
  });
  server->on("/setWiFi", HTTP_POST, []() {
    if (server->hasArg("ssid") && server->hasArg("password") && server->hasArg("zipcode")) {
      staSSID = server->arg("ssid");
      staPassword = server->arg("password");
      zipCode = server->arg("zipcode");
      configured = true;
      Serial.println("Configuration set");
      server->send(200, "text/html", "Configuration saved. Connecting to Wi-Fi...");
    } else {
      server->send(400, "text/plain", "Bad Request");
    }
  });
  server->on("/setup", HTTP_GET, handleSetup);  // Add setup route
  server->begin();
  Serial.println("Config web server started");
}

void handleSetup() {
  String stepStr = server->arg("step");
  int step = stepStr.toInt();
  if (stepStr == "" || step < 0 || step > 13) step = 0;

  String instruction;
  if (step < 8) {
    int tempIndex = 7 - step;
    int tempAngle = anglePoints[tempIndex];
    tempServo.write(tempAngle);
    instruction = "Place " + String(tempPoints[tempIndex]) + " by the right servo motor";
  } else {
    int iconIndex = step - 8;
    int iconAngle = iconAngles[iconIndex];
    iconServo.write(iconAngle);
    instruction = "Place " + icons[iconIndex] + " by the left servo motor";
  }

  String nextLink;
  String buttonText;
  if (step < 13) {
    nextLink = "/setup?step=" + String(step + 1);
    buttonText = "Next";
  } else {
    nextLink = "/";
    buttonText = "Finish";
  }

  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body>"
                "<h1>Setup Sequence</h1>"
                "<p style='text-align: center;'>" + instruction + "</p>"
                "<div style='text-align: center;'>"
                "<a href='" + nextLink + "'><button>" + buttonText + "</button></a>"
                "</div>"
                "</body></html>";
  server->send(200, "text/html", html);
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
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to WiFi");
  }
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected, skipping weather fetch");
    return;
  }
  HTTPClient http;
  String geoUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + zipCode + "&count=1&format=json";
  http.begin(geoUrl);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.println("Geocoding failed with code: " + String(httpCode));
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
    Serial.println("Weather fetch failed with code: " + String(httpCode));
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
  Serial.print("°F, Icon: ");
  Serial.println(currentIcon);
}

void moveServos(int temp, String icon) {
  int tempAngle = mapTempToAngle(temp);
  int iconAngle = mapIconToAngle(icon);
  
  tempServo.write(tempAngle);
  delay(500);  // Wait to reduce current draw
  iconServo.write(iconAngle);
  
  Serial.print("Servos moved - Temp angle: ");
  Serial.print(tempAngle);
  Serial.print(", Icon angle: ");
  Serial.println(iconAngle);
}

void handleWeatherPage() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>Weatherometer</title></head><body>"
                "<h1>Weatherometer</h1>"
                "<p>Current Zip Code: " + zipCode + "</p>"
                "<p>Temperature: " + String(currentTemp) + "°F</p>"
                "<p>Forecast: " + currentIcon + "</p>"
                "<form action='/setZip' method='post'>"
                "New Zip Code: <input type='text' name='zipcode'><br>"
                "<input type='submit' value='Update'>"
                "</form>"
                "<div style='text-align: center; margin-top: 20px;'>"
                "<a href='/setup?step=0'><button>Setup Sequence</button></a>"
                "</div>"
                "</body></html>";
  server->send(200, "text/html", html);
}

void handleSetZip() {
  if (server->hasArg("zipcode")) {
    zipCode = server->arg("zipcode");
    fetchWeatherData();
    moveServos(currentTemp, currentIcon);
    server->send(200, "text/html", "Zip code updated. <a href='/'>Go back</a>");
  } else {
    server->send(400, "text/plain", "Bad Request");
  }
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
