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
const int ledPin = 2;  // Built-in LED, active low

// Wi-Fi and configuration
String staSSID = "";
String staPassword = "";
String zipcode = "";
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
  delay(1000);  // Allow Serial to initialize
  Serial.println("Setup started");

  // Attach servos
  tempServo.attach(tempServoPin);
  iconServo.attach(iconServoPin);

  // Initialize preferences
  preferences.begin("weather", false);
  staSSID = preferences.getString("ssid", "");
  staPassword = preferences.getString("password", "");
  zipcode = preferences.getString("zipcode", "");

  // Start in AP mode if not configured
  if (staSSID == "" || staPassword == "" || zipcode == "") {
    startConfigMode();
    while (!configured) {
      server.handleClient();
      delay(10);  // Prevent watchdog timeout
    }
    server.close();  // Stop AP server after configuration
  }

  // Connect to home Wi-Fi
  connectToWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    startHomeServer();  // Start weather station server
    fetchWeatherData(); // Fetch initial weather data
    moveServos(currentTemp, currentIcon); // Update servos
  } else {
    Serial.println("Failed to connect to Wi-Fi. Restarting in AP mode...");
    ESP.restart();  // Restart to reconfigure if connection fails
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();  // Serve weather station page
  }
  delay(10);
}

void startConfigMode() {
  Serial.println("Starting AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config");
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  // LED on in AP mode

  server.on("/", HTTP_GET, []() {
    String html = "<html><body><h1>Configure Wi-Fi and Location</h1>"
                  "<form action='/setWiFi' method='post'>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Password: <input type='password' name='password'><br>"
                  "Zip Code: <input type='text' name='zipcode'><br>"
                  "<input type='submit' value='Save'>"
                  "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/setWiFi", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("zipcode")) {
      staSSID = server.arg("ssid");
      staPassword = server.arg("password");
      zipcode = server.arg("zipcode");
      preferences.putString("ssid", staSSID);
      preferences.putString("password", staPassword);
      preferences.putString("zipcode", zipcode);
      configured = true;
      Serial.println("Configuration saved");
      server.send(200, "text/plain", "Configuration saved. Connecting to Wi-Fi...");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.begin();
  Serial.println("AP server started");
}

void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi: " + staSSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(staSSID.c_str(), staPassword.c_str());
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());  // Print IP to access weather page
    digitalWrite(ledPin, HIGH);  // LED off when connected
  } else {
    Serial.println("\nConnection failed");
  }
}

void startHomeServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<html><body><h1>Weather Station</h1>"
                  "<p>Current Zip Code: " + zipcode + "</p>"
                  "<p>Current Temperature: " + String(currentTemp) + "°F</p>"
                  "<p>Forecast: " + currentIcon + "</p>"
                  "<form action='/setzip' method='get'>"
                  "New Zip Code: <input type='text' name='zip'><br>"
                  "<input type='submit' value='Update'>"
                  "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/setzip", HTTP_GET, []() {
    if (server.hasArg("zip")) {
      zipcode = server.arg("zip");
      preferences.putString("zipcode", zipcode);
      fetchWeatherData();
      moveServos(currentTemp, currentIcon);
      server.send(200, "text/plain", "Zip code updated to " + zipcode);
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.begin();
  Serial.println("Home Wi-Fi server started");
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;
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
    Serial.println("Invalid zip code");
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
  delay(500);  // Reduce current draw
  iconServo.write(iconAngle);
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
