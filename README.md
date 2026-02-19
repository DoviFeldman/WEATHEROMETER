# WEATHEROMETER
This is an Analog Weatherometer that shows the weather forecast and daily temperature using servo motors.

instructions: Join Weatherometer wifi then 

go to this website address: 

192.168.4.1 

thats the ESP32's IP address website server Weatherometers wifis website

The "WORKSWeatherometerMay01a.ino" if the offical original file to use, it opens its server again on the users wifi and makes a new second wifi with the name of the IP address of its website on the users home wifi, so that you can check the weather, change the zipcode, and run the setup function again.

The "Consumer version" is the simpler consumer version of that code, same setup function, but after the user connects to his home wifi it shuts off the server website and doesnt turn on another wifi, setup must be done while the users on its wifi or unplug it and restart.
its the same thing but simpler no dealing with all the other confustion
these 2 versions also restart and forget the wifi whenver you unplug them.

The "Grandmas version" has the wifi hardcoded and gets rid of the server completely and also the setup function, because it will use the allready setup manufactured and fully placed icons and temps version of the 3d printed Weatherometer.
This one is the simplest and has the Wifi and password hardcoded in the Arduino IDE and requires no setup or wifi setup and the 3d print is also completely put together and setup right for use out of the box.
This version is actually and literally for my Grandma.

oh yeah and i still need to match up the servo pointers to the 3d printed icons and temp placement, i totally never did that, also for the Grandmas version.
i will update when i did that. 


## Open-Meteo API Weather Codes 

Open-Meteo uses WMO Weather interpretation codes with 28 different weather conditions 
Here are the main codes:

Clear/Cloudy:
- 0: Clear sky
- 1: Mainly clear 
- 2: Partly cloudy
- 3: Overcast

Fog:
- 45: Fog
- 48: Depositing rime fog

Drizzle:
- 51: Light drizzle
- 53: Moderate drizzle
- 55: Dense drizzle
- 56: Light freezing drizzle
- 57: Dense freezing drizzle

Rain:
- 61: Slight rain
- 63: Moderate rain
- 65: Heavy rain
- 66: Light freezing rain
- 67: Heavy freezing rain

Snow:
- 71: Slight snow fall
- 73: Moderate snow fall
- 75: Heavy snow fall
- 77: Snow grains

Showers:
- 80: Slight rain showers
- 81: Moderate rain showers
- 82: Violent rain showers
- 85: Slight snow showers
- 86: Heavy snow showers

Thunderstorms:
- 95: Thunderstorm
- 96: Thunderstorm with slight hail
- 99: Thunderstorm with heavy hail

The codes are ordered by severity from 0 (clear sky) to 99 (thunderstorm with hail)



