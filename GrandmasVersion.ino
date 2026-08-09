#include <WiFi.h>
// #include <WebServer.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>  // Added for NTP time functions

//JULY 8TH 2025


Servo tempServo;
Servo iconServo;

// Pin definitions
const int tempServoPin = 13;
const int iconServoPin = 14;
const int ledPin = 2;  // Assuming built-in blue LED is on pin 2

// Hardcoded Wi-Fi credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const String zipCode = "YOUR_ZIP_CODE"; 



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

// ADDED SEPTEMBER 26!!!! 
//NOTES!!!!! 
// 180 is to the left and it is snow and 15 degrees 0 is to the right and its sun and 95 degrees.
// WERE USING 180 DEGREES CAUSE ITS MY GRANDMAS VERSION, THE OTHER ONES USE LESS.
// I WANT THE SUN ON THE RIGHT AND THE SNOW ON THE LEFT AND THE HIGH TEMPRATURE ON THE RIGHT AND THE COLD TEMPRATURE ON THE LEFT!!!!
// BUT THE SERVOS ONLY TURN LIKE 1 DIRECTION WHICH I DONT EXACTLY GET BUT I TESTED A FEW TIMES SO YEAH 0 HAS TO BE ON THE RIGHT AND YOU CANT JUST TURN IT UPSIDE DOWN
// OK NOW: TEMP HAS 8 NUMBERS AND ICONS HAVE 6 ICONS
// 180/7 GIVES US 8 NUMBERS IF WE INCLUDE 0(AND 180/8 GIVES US 9 SO WE COULDNT USE THAT) SO THATS WHAT I DID TO GET THEM TO BE EQUALLY SPACED, I TESTED IT AND IT WORKS GREAT AND LOOKS NICE!
// AND 180 OVER 5 GIVES US 6 NUMBERS IF WE INCLUDE 0 SO THATS WHAT I DID AND IT LOOKS NICE
// THE 180/7 NUMBERS HAVE TO BE ROUNDED SO HERE ARE THE REAL NUMBERS AND I PUT THE ROUNDED ONES IN THE CODE 180/7 = 0, 25.7 , 51.4 , 77.1 , 102.8 , 128.5 , 154.2 , 179.9  THATS 8 NUMBERS.
// 180/5 = 0 , 36 , 72 , 108 , 144 , 180 
// OK PERFECT. 
// THE TEMPRATURE SERVO AND ICONS SHOULD BE ON BOTTOM CAUSE THERES MORE ROOM THERE CAUSE WE TOOK OUT THE BARS DESIGNS AND THE ICONS ON TOP CAUSE THERES LESS OF THEM (ONLY 6) AND THERES LESS ROOM ON TOP

// Temperature to angle mapping
const int tempPoints[] = {15, 32, 45, 55, 65, 75, 85, 95};
//const int anglePoints[] = {0, 15, 45, 57, 69, 81, 93, 105}; // OLD BASE VERSION
const int anglePoints[] = {180, 154, 128, 103, 77, 51, 26, 0}; //  Working Tested Grandmas Version
const int numPoints = 8;

// Icon to angle mapping
const String icons[] = {"Sunny", "Partially Cloudy", "Very Cloudy", "Raining", "Thunderstorm", "Snow"};
// const int iconAngles[] = {90, 72, 54, 36, 18, 0}; // old base version
const int iconAngles[] = {0, 36, 72, 108, 144, 180}; // Working Tested Grandmas Version!!!!! ok yes yeah this is good. 

// ok great updated everything and added all the code. tested mostly and it all works and looks well(didnt test this program though yet.

// AUGUST 2026: brought over the daytime-only weather update from the consumer version.
// Instead of using the one daily weathercode (which could be a rainstorm at 3 AM while the day is sunny),
// it now looks at every hour between START_HOUR and END_HOUR and picks the weather that dominates the day.

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
// Default: 2 = 2 AM (this is what grandmas version always did). Change to whatever you want.
// To change: just edit the number. Example: 5 = 5 AM
const int UPDATE_HOUR = 2;

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
  digitalWrite(ledPin, HIGH);  


  // Connect to Wi-Fi
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

    digitalWrite(ledPin, LOW);  // LED off when connected to Wi-Fi, i moved it inside the loop unlike the other ones, idk about the other ones, maybe i should look into the consumer version.

  }

  

  Serial.println("Weatherometer ready");
}

void loop() {
  
  // Check time and fetch weather at UPDATE_HOUR local time (default 2 AM)
  time_t now = time(nullptr);
  time_t local_now = now + utc_offset_seconds;
  struct tm *local_tm = gmtime(&local_now);  // Interpret as local time due to offset

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



void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    
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
  Serial.println("Daytime weather scores (hours " + String(START_HOUR) + "-" + String(END_HOUR) + "):");
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
