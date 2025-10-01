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
