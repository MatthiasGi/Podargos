/*
 * Firmware for the Podargos garage door opener.
 * Description: Firmware with an integrated webserver responding to control
 *              over WiFi.
 * Author:      Matthias Gianfelice <matthias@gianfelice.de>
 * Date:        2020-05-26
 */

#include "HC_SR04.h"
#include "RdWebServer.h"
#include "RestAPIEndpoints.h"

////////////////////////////////////////////////////////////////////////////////
//////////////////////// GLOBAL CONSTANTS AND VARIABLES ////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Assure that no connection to the particle-cloud is made. As the connection
// is not necessary for the project, we should cut everything out that may add
// complexity to the project.
SYSTEM_MODE(MANUAL);

// The webserver that runs in the background and allows for generating the
// REST-API
RdWebServer* pWebServer = NULL;

// Preparing the REST-API.
RestAPIEndpoints restAPIEndpoints;

// Definition of the used pins alongside the project.
#define PIN_DISTANCE_TRIG D4
#define PIN_DISTANCE_ECHO D5
#define PIN_RELAIS A4
#define PIN_MAGNET D3

// Definition of possible states.
#define STATE_UNKNOWN 0
#define STATE_CLOSED 1
#define STATE_OPENING 2
#define STATE_OPENED 3
#define STATE_CLOSING 4

// Saves the current state of the door.
int state = STATE_UNKNOWN;

// This device sits on top of the garage and measures whether the gate is open
// or closed. Additionally it can sense, if a car is present.
HC_SR04 distance_sensor = HC_SR04(PIN_DISTANCE_TRIG, PIN_DISTANCE_ECHO);

// Saves the distance measured by the sensor.
double distance_cache = 0;

// Saves the state of the magnet checking whether the door is closed.
bool magnet_cache = false;

// Saves, wether a car must be inside of the closed garage
bool car_inside = false;

// Key used to secure the REST-API. Should be used to prepend the entrypoints.
String rest_key = "qMp3Q2pGuYw3";

// Once the ultrasonic-distance-sensor triggers an open door we should wait a
// bit, as the door isn't instant open. This behaviour is being made able by
// saving the timestamp here.
#define TIME_DIFFERENCE_DOOR_OPEN 3000UL
unsigned long time_until_door_open = 0UL;
bool awaiting_door_open = false;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Hardware ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Unified method for triggering a pulse on the relais and thus triggering the
// door.
void trigger_relais() {
  digitalWrite(PIN_RELAIS, HIGH);
  delay(50);
  digitalWrite(PIN_RELAIS, LOW);
  delay(100);
}

void setup_hardware() {
  pinMode(PIN_RELAIS, OUTPUT);
  pinMode(PIN_MAGNET, INPUT);
}

void loop_hardware() {
  distance_cache = distance_sensor.getDistanceCM();

  if (distance_cache < 20) {
    // If the distance is small, the door is open as the view of the sensor is
    // obstructed. To differentiate between real open door or a door that's right
    // before opening, this check is introduced.
    if (!awaiting_door_open && state != STATE_OPENED) {
      time_until_door_open = millis() + TIME_DIFFERENCE_DOOR_OPEN;
      awaiting_door_open = true;
      if (state != STATE_CLOSING) state = STATE_OPENING;
    }
    if (awaiting_door_open && millis() >= time_until_door_open) {
      awaiting_door_open = false;
      if (state == STATE_OPENING) {
        state = STATE_OPENED;
      } else if (state == STATE_CLOSING) {
        trigger_relais();
      }
    }

    car_inside = false;

  } else if(distance_cache < 80) {
    // If the distance is medium there must be a car inside of the garage

    if (state == STATE_OPENED) state = STATE_CLOSING;
    car_inside = true;

  } else if(distance_cache > 100) {
    // If the distance is large, there can't be a car inside of the garage

    if (state == STATE_OPENED) state = STATE_CLOSING;
    car_inside = false;

  }

  magnet_cache = digitalRead(PIN_MAGNET);
  if (magnet_cache) {
    if (state != STATE_OPENING) state = STATE_CLOSED;
  } else {
    if (state == STATE_CLOSED) state = STATE_OPENING;
  }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// REST-API ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void restAPI_getData(RestAPIEndpointMsg& apiMsg, String& retStr) {
  retStr = "{\"distance\": " + String(distance_cache) + "," +
           " \"magnet\": " + String(magnet_cache) + "," +
           " \"state\": " + String(state) + "," +
           " \"car\": " + String(car_inside) + "}";
}

void restAPI_openDoor(RestAPIEndpointMsg& apiMsg, String& retStr) {
  if (state == STATE_CLOSED) {
    trigger_relais();
    state = STATE_OPENING;
    retStr = "{\"response\": \"OK\"}";
  } else if (state == STATE_CLOSING) {
    trigger_relais();
    trigger_relais();
    state = STATE_OPENING;
    retStr = "{\"response\": \"OK\"}";
  } else if (state == STATE_OPENED) {
    retStr = "{\"response\": \"OK\"}";
  } else if (state == STATE_OPENING) {
    retStr = "{\"response\": \"OK\"}";
  } else if (state == STATE_UNKNOWN) {
    retStr = "{\"response\": \"ERROR\", \"message\": \"State is unknown.\"}";
  }
}

void restAPI_closeDoor(RestAPIEndpointMsg& apiMsg, String& retStr) {
  if (state == STATE_CLOSED) {
    retStr = "{\"response\": \"OK\"}";
  } else if (state == STATE_CLOSING) {
    retStr = "{\"response\": \"OK\"}";
  } else if (state == STATE_OPENED) {
    trigger_relais();
    state = STATE_CLOSING;
    retStr = "{\"response\": \"OK\"}";
  } else if (state == STATE_OPENING) {
    state = STATE_CLOSING;
    retStr = "{\"response\": \"OK\"}";
  } else {
    retStr = "{\"response\": \"ERROR\", \"message\": \"State is unknown.\"}";
  }
}

void setup_restAPI() {
  pWebServer = new RdWebServer();
  restAPIEndpoints.addEndpoint(rest_key + "/get",
      RestAPIEndpointDef::ENDPOINT_CALLBACK, restAPI_getData, "", "");
  restAPIEndpoints.addEndpoint(rest_key + "/open",
      RestAPIEndpointDef::ENDPOINT_CALLBACK, restAPI_openDoor, "", "");
  restAPIEndpoints.addEndpoint(rest_key + "/close",
      RestAPIEndpointDef::ENDPOINT_CALLBACK, restAPI_closeDoor, "", "");
  pWebServer->addRestAPIEndpoints(&restAPIEndpoints);
  pWebServer->start(80);
}

void loop_restAPI() {
  if (pWebServer) pWebServer->service();
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Central firmware ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Prepares the device for usage.
void setup() {
  // We assume that the user already configured the WiFi-connection. So we only
  // need to set the hostname to ensure that the device is reachable
  // independant from its IP-address.
  WiFi.setHostname("podargos");
  WiFi.connect();
  while (!WiFi.ready()) delay(100);

  setup_hardware();
  loop_hardware();
  setup_restAPI();
}

// Update everything.
void loop() {
  loop_hardware();
  loop_restAPI();
}