#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>  // Added for NTP time functions

// THIS VERSION WAS CREATED FEB 19 2026. TODAY THE DAY THIS FILE WAS CREATED.

// join weatherometer wifi, then go to website 192.168.4.1 
// thats the website



//September 30th 2025
// tested out the correct angles for the icons and temprature numbers

// Numbers on the right icons on the left

// set the sun to 0 degrees, and the snowflake to 120 degrees
// and set the 95 to 120 degrees and the 15 to 0 degrees
// IMPORTANT!!!!
// ot goes 120 degrees matching with the angles on the hexagon, which looks super cool.

//   so the degrees should be:

// 0 SUN

// 24 PARTALLY CLOUDY

// 48 CLOUDY

// 72 RAINY

// 96 THUNDERSTOMY 

// 120 SNOWY

// OK THATS GOOD ENOUGH

// 0 - 15

// 17 - 32 

// 34 -45 

// 51 - 55 

// 68 - 65 

// 85 - 75

// 102 - 85 

// 119  - 95 (120 works the same too, ill just make it 120, it doesnt matter(it mattered in the onshape sketch but not here))

// ok wow perfect!



  
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// THIS IS THE CONSUMER VERSION OF THIS WEATHEROMETER ROBOT. THERE IS NO NEW SECOND SERVER AND NO NEW SECOND WIFI TO TELL YOU THE IP ADDRESS ON THE HOME WIFI.
// THE PURPOSE OF THAT WAS FOR TESTING TO SEE THE SERVOS MOVE WHEN I CHANGED ZIPCODES, FOR CONSUMERS THERES NO NEED, IT JUST CONNECTS TO THE USERS WIFI AND THATS IT, NO OTHER SERVER OR SECOND WIFI
// ALL VERSIONS OF THESE WEATHEROMETERS (INCLUDING THE ORIGINAL), EXCEPT THE GRANDMAS VERSION, FORGET THE USERS WIFI AND RESTART FROM SCRATCH WHEN YOU UNPLUG THEM.
//JULY 8TH 2025

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

// Time variables for scheduling
static int utc_offset_seconds = 0;     // Offset from UTC in seconds
static int last_fetch_yday = -1;       // Day of the year of last fetch
static int last_fetch_year = -1;       // Year of last fetch

  #ifdef ENABLE_SECOND_UPDATE
  static bool second_update_done = false;
  #endif


// Temperature to angle mapping
const int tempPoints[] = {15, 32, 45, 55, 65, 75, 85, 95};
const int anglePoints[] = {0, 17, 34, 51, 68, 85, 102, 120};
const int numPoints = 8;

// Icon to angle mapping
const String icons[] = {"Sunny", "Partially Cloudy", "Very Cloudy", "Raining", "Thunderstorm", "Snow"};
const int iconAngles[] = {0, 24, 48, 72, 96, 120};

// NEW CODE FROM CLAUDE OPUS 4.6

// =========================== CONFIGURABLE SETTINGS ===========================
  //
  // DAYTIME WINDOW: Only weather between these hours is considered.
  // Uses 24-hour format. 6=6AM, 14=2PM, 22=10PM
  // To change: edit the numbers. Example: START_HOUR=8 and END_HOUR=18 = 8AM to 6PM
  const int START_HOUR = 6;    // Start of daytime window (inclusive)
  const int END_HOUR = 22;     // End of daytime window (inclusive, 22 = 10 PM)

  // SEVERITY WEIGHT: How much "bad weather" hours count vs "nice weather" hours.
  // This is your "dial" to control how sensitive it is to bad weather:
  //
  // 1.0 = pure majority wins, every hour counts the same
  //       14 sunny hours + 3 rainy hours → SUNNY (14 vs 3)
  //
  // 1.5 = bad weather counts 1.5x (RECOMMENDED starting point)
  //       14 sunny hours + 3 rainy hours (3×1.5=4.5) → SUNNY (14 vs 4.5)
  //       8 sunny hours + 6 rainy hours (6×1.5=9) → RAINY (8 vs 9)
  //
  // 2.0 = bad weather counts double
  //       12 sunny + 4 rainy (4×2=8) → SUNNY (12 vs 8)
  //       8 sunny + 5 rainy (5×2=10) → RAINY (8 vs 10)
  //
  // 3.0 = bad weather counts triple, pretty cautious
  //       12 sunny + 4 rainy (4×3=12) → TIE → RAINY (ties go to worse weather)
  //
  // 5.0 = even brief bad weather dominates
  //       14 sunny + 3 rainy (3×5=15) → RAINY (14 vs 15)
  //
  // "Bad weather" = Raining, Thunderstorm, Snow (get multiplied)
  // "Good/neutral" = Sunny, Partially Cloudy, Very Cloudy (always count as 1 per hour)
  //
  // START AT 1.5, then raise it if you want more "heads up" about rain,
  // or lower toward 1.0 if you want it to only show rain when it's raining most of the day.
  const float SEVERITY_WEIGHT = 1.5;

  // DAILY UPDATE HOUR: When to fetch weather each day (local time, 24h format)
  // Default: 2 = 2 AM. Change to whatever you want.
  // To change: just edit the number. Example: 5 = 5 AM
  const int UPDATE_HOUR = 20;

  // ========================= SECOND DAILY UPDATE (OPTIONAL) =========================
  // Want a second, more accurate update later in the day? Uncomment these two lines:
  // #define ENABLE_SECOND_UPDATE
  // const int SECOND_UPDATE_HOUR = 6;
  //
  // How it works: First fetch happens at UPDATE_HOUR (e.g. 2 AM).
  // Second fetch happens at SECOND_UPDATE_HOUR (e.g. 6 AM) for a more accurate forecast.
  // SECOND_UPDATE_HOUR must be AFTER UPDATE_HOUR.
  // To change the second update time, just change the number.
  // Examples: 6 = 6AM, 8 = 8AM, 12 = noon
  // =================================================================================





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

  // Connect to Wi-Fi (this shuts off the AP by setting WIFI_STA)
  connectToWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    // Set up NTP to get UTC time
    configTime(0, 0, "pool.ntp.org");
    Serial.println("Waiting for NTP time sync");
    time_t now = time(nullptr);
    while (now < 1000000000) {  // Wait until time is set
      delay(1000);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println("\nTime synchronized");

    // Fetch initial weather data and set UTC offset
    fetchWeatherData();
    moveServos(currentTemp, currentIcon);

     digitalWrite(ledPin, LOW);  // LED off when connected to Wi-Fi ( moved the LED turn off into the if statement so it only shuts off only if it successfully connects to wifi July 9th) 

  }

 

  // Delete the server since we don't need it anymore
  delete server;
  server = nullptr;


  Serial.println("Weatherometer ready");
}

void loop() {

    time_t now = time(nullptr);
    time_t local_now = now + utc_offset_seconds;
    struct tm *local_tm = gmtime(&local_now);

    // First daily update at UPDATE_HOUR (default 2 AM)
    if (local_tm->tm_hour >= UPDATE_HOUR &&
        (local_tm->tm_yday != last_fetch_yday || local_tm->tm_year != last_fetch_year)) {
      fetchWeatherData();
      moveServos(currentTemp, currentIcon);
      last_fetch_yday = local_tm->tm_yday;
      last_fetch_year = local_tm->tm_year;
      #ifdef ENABLE_SECOND_UPDATE
      second_update_done = false;  // Reset for today
      #endif
    }

    // Second daily update (only active if you uncommented ENABLE_SECOND_UPDATE above)
    #ifdef ENABLE_SECOND_UPDATE
    if (local_tm->tm_hour >= SECOND_UPDATE_HOUR && !second_update_done &&
        local_tm->tm_yday == last_fetch_yday && local_tm->tm_year == last_fetch_year) {
      Serial.println("Running second daily update...");
      fetchWeatherData();
      moveServos(currentTemp, currentIcon);
      second_update_done = true;
    }
    #endif
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
                  "Password: <input type='text' name='password'><br>"
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

String calculateDominantWeather(JsonArray hourlyWeatherCodes) {
    // Score each weather category based on how many hours it appears
    // Bad weather gets multiplied by SEVERITY_WEIGHT
    float scores[6] = {0, 0, 0, 0, 0, 0};  // Same order as icons[] array

    for (int h = START_HOUR; h <= END_HOUR; h++) {
      if (h >= (int)hourlyWeatherCodes.size()) break;
      int code = hourlyWeatherCodes[h];
      String icon = mapWeatherCodeToIcon(code);

      for (int i = 0; i < 6; i++) {
        if (icon == icons[i]) {
          // Indices 3,4,5 = Raining, Thunderstorm, Snow → apply severity weight
          if (i >= 3) {
            scores[i] += SEVERITY_WEIGHT;
          } else {
            scores[i] += 1.0;
          }
          break;
        }
      }
    }

    // Pick highest scoring category. Ties go to the more severe weather (higher index).
    int bestIndex = 0;
    float bestScore = scores[0];
    for (int i = 1; i < 6; i++) {
      if (scores[i] >= bestScore) {
        bestScore = scores[i];
        bestIndex = i;
      }
    }

    // Print breakdown to Serial for debugging
    Serial.println("Daytime weather scores (hours " + String(START_HOUR) + "-" +
  String(END_HOUR) + "):");
    for (int i = 0; i < 6; i++) {
      if (scores[i] > 0) {
        Serial.println("  " + icons[i] + ": " + String(scores[i]));
      }
    }
    Serial.println("  Result: " + icons[bestIndex]);

    return icons[bestIndex];
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

String weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 6) + "&longitude=" + String(lon, 6) + "&daily=temperature_2m_max&hourly=weathercode&temperature_unit=fahrenheit&forecast_days=1";       
  http.begin(weatherUrl);
  httpCode = http.GET();
  if (httpCode != 200) {
    Serial.println("Weather fetch failed with code: " + String(httpCode));
    http.end();
    return;
  }
  payload = http.getString();
  http.end();

  DynamicJsonDocument weatherDoc(4096);  // Larger buffer needed for hourly data
  deserializeJson(weatherDoc, payload);
  if (!weatherDoc["daily"] || !weatherDoc["hourly"]) {
    Serial.println("No weather data");
    return;
  }
  float maxTemp = weatherDoc["daily"]["temperature_2m_max"][0];
  currentTemp = round(maxTemp);

  // Calculate dominant weather from daytime hours only
  JsonArray hourlyWeatherCodes = weatherDoc["hourly"]["weathercode"].as<JsonArray>();
  currentIcon = calculateDominantWeather(hourlyWeatherCodes);

  // Extract UTC offset for local time calculation
  utc_offset_seconds = weatherDoc["utc_offset_seconds"].as<int>();

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
