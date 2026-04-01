#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
// THE ASYNC LIBRARIES NEVER WORK FOR ME THEY NEVER EVER WORKED FOR ME!!!!! EVEN BEFORE!
// THIS ONE IS NOT SYNCRIOUS LIKE THE ASYNC ONE IS BUT IT WORKS!!! I SHOULD LEARN A BIT ABOUT THE DIFFERENCE BEFORE FULLY DEPLOYING,
// BUT ALL THESE LIBRARIES COME WITH THE ESP32 NO NEED TO INSTALL ANYTHING AND WORKED PERFECTLY FIRST TIME!!! THERE IS A NOTIFICATION TO SIGN INTO THE WIFI!!! PERFECT!!! 
//AMAZING!! BRINGS ME STRIGHT TO THE WEBSITE!!!! THIS SOLVES ALL MY PROBLEMS!!!!!

DNSServer dnsServer;
WebServer server(80);

void setup() {
  WiFi.softAP("ESP32");
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  server.onNotFound([]() {
    server.send(200, "text/html", "<h1>Welcome!</h1>");
  });
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
