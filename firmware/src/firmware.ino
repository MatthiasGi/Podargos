/*
 * Project firmware
 * Description:
 * Author:
 * Date:
 */

#include "HC_SR04.h"
#include "RdWebServer.h"
#include "RestAPIEndpoints.h"

SYSTEM_MODE(MANUAL);

RdWebServer* pWebServer = NULL;
RestAPIEndpoints restAPIEndpoints;

int pin_trig = D4;
int pin_echo = D5;
int pin_relais = D6;

HC_SR04 rangefinder = HC_SR04(pin_trig, pin_echo);

void restAPI_getDistance(RestAPIEndpointMsg& apiMsg, String& retStr) {
  double distance = rangefinder.getDistanceCM();
  retStr = "{\"state\":\"" + String(distance) + "\"}";
}

void restAPI_triggerDoor(RestAPIEndpointMsg& apiMsg, String& retStr) {
  digitalWrite(pin_relais, HIGH);
  delay(100);
  digitalWrite(pin_relais, LOW);
  retStr = "{\"respond\":\"ok\"}";
}

void setup() {
  WiFi.setHostname("podargos");
  WiFi.connect();
  while (!WiFi.ready()) delay(100);

  pWebServer = new RdWebServer();

  if (pWebServer) {
    restAPIEndpoints.addEndpoint("distance", RestAPIEndpointDef::ENDPOINT_CALLBACK, restAPI_getDistance, "", "");
    restAPIEndpoints.addEndpoint("door", RestAPIEndpointDef::ENDPOINT_CALLBACK, restAPI_triggerDoor, "", "");

    pWebServer->addRestAPIEndpoints(&restAPIEndpoints);

    pWebServer->start(80);
  }

  pinMode(pin_relais, OUTPUT);
}

void loop() {
  if (pWebServer) pWebServer->service();
}