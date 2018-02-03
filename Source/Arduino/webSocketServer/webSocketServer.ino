/*
 * WebSocketServer.ino
 *
 *  Created on: 22.05.2015
 *
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>

ESP8266WiFiMulti WiFiMulti;

// Server radi na portu 81
WebSocketsServer webSocket = WebSocketsServer(81);

#define USE_SERIAL Serial

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        
              // send message to client
              webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

            String pl = (char*)payload;
            if(pl == "testApp:up")
            {
              // diži rampu
              //USE_SERIAL.printf("[%u] get Text: up", num);
            }
            else if(pl == "testApp:down")
            {
              // spuštaj rampu
              //USE_SERIAL.printf("[%u] get Text: down", num);
            }
            else if( pl == "testApp:stop")
            {
              // zaustavi rampu
              //USE_SERIAL.printf("[%u] get Text: stop", num);
            }
            else if( pl == "testApp:sensors")
            {
              webSocket.sendTXT(num, "5 5 5 5");
            }

            
            if( pl == "rampApp:state")
            {
              // 4 senzora + zauzeta mjesta parking + stanje rampe u stupnjevima(0-60)
              webSocket.sendTXT(num, "5, 5, 5, 5, 2, 30");
            }
            // send message to client
            // webSocket.sendTXT(num, "message here");
            // send data to all connected clients
            // webSocket.broadcastTXT("message here");

            
            break;
    }

}

void setup() {
    // USE_SERIAL.begin(921600);

    // ispis na serial monitoru mora biti naštiman na 115200
    USE_SERIAL.begin(115200);

    //Serial.setDebugOutput(true);
    USE_SERIAL.setDebugOutput(true);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    //Ovdje upisati ime i pass wireless mreže
    //ISKON-HotSpot
    WiFiMulti.addAP("ISKON-HotSpot", "");
    

    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
    }

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

int lastMessageSent = 0;
const int sensorStateInterval = 5000;

void loop() {
    webSocket.loop();

    if( lastMessageSent + sensorStateInterval >= millis())
    {
       webSocket.broadcastTXT("sensor state here");
       USE_SERIAL.printf("Sensor data sent.");
       lastMessageSent = millis();
    }
}



