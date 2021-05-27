/**
 * File: BrachiumRobotic.ino
 * Written 2021 by Marcus and Georg Miss.
 * Requires:
 * - Ardunio Uno Wifi Revision 2
 * - Braccio-Robot.
 * 
 * http://your-ip/
 *  Delivers html-page with current values.
 *  
 * These methods delivers always an JSON-object with current servo-values.
 * http://your-ip/gc
 *  Close gripper
 * http://your-ip/go
 *  Open gripper
 * http://your-ip/status
 *  Query current status
 * http://your-ip/safe
 *  Move arm into safe position
 * http://your-ip/sky
 *  Move arm into sky positon
 * http://your-ip/set?m1=v1&m2=v2&m3=v3&m4=v4&m5=v5&m6=v6&s=speed
 *  Move at to position(s) with given speed. All values are optional.
 */

#include <SPI.h>
#include <WiFiNINA.h>
#include <Braccio.h>
#include <Servo.h>
#include "DeviceConfig.h"

Servo base;
Servo shoulder;
Servo elbow;
Servo wrist_rot;
Servo wrist_ver;
Servo gripper;

// HTTP-Code(s)
#define HTTP_OK                 200
#define HTTP_NOT_FOUND          404
#define HTTP_NOT_IMPLEMENTED    501

// Global variables - Wifi/WLAN
char wifiSsid[] = WLAN_SSID;
char wifiPass[] = WLAN_PASSWORD;
int wifiStatus = WL_IDLE_STATUS;
WiFiServer roboServer(HTTP_SERVER_PORT);

// Global variables - Braccio
int roboM1 = 91;    // M1 Base: 90 degrees              (0...180)
int roboM2 = 92;    // M2 Shoulder: 45 degrees          (15...165)
int roboM3 = 93;    // M3 Elbow: 180 degrees            (0...180)
int roboM4 = 94;    // M4 Wrist vertical: 180 degrees   (0...180)
int roboM5 = 95;    // M5 Wrist rotation: 90 degrees    (0...180)
int roboM6 = 66;    // M6 Gripper: 10 degrees           (10...73), 10 = open, 73 = closed
int roboSpeed = 15; // step delay

const int M1_MIN = 0;
const int M1_MAX = 180;
const int M2_MIN = 15;
const int M2_MAX = 165;
const int M3_MIN = 0;
const int M3_MAX = 180;
const int M4_MIN = 0;
const int M4_MAX = 180;
const int M5_MIN = 0;
const int M5_MAX = 180;
const int M6_MIN = 10;
const int M6_MAX = 73;
const int SPEED_MIN = 10;
const int SPEED_MAX = 30;

const int GRIPPER_OPEN = M6_MIN;
const int GRIPPER_CLOSE = M6_MAX;
const int SPEED_SLOW = 10;
const int SPEED_FAST = 30;

// Global variables - MIME-types
String mimeHtml = "text/html";
String mimeJson = "text/json";

/* ------------------------------ Arduino Setup and Loop ------------------------------ */

/**
 * One-time initialization.
 */
void setup() {
  Serial.begin(9600);

  if (!(setupWifi())) {
    Serial.println("TODO:WIFI NOT AVAILABLE");
    while (true);
  }
  Braccio.begin();

  robotIntoHomePosition();
  Serial.println("BrachiumRobotic started...");
}

/**
 * Application-loop.
 */
void loop() {
  WiFiClient client = roboServer.available();
  if (client) {
    String request = readHttpRequest(&client);
    serveHttpRequest(&client, &request);
    client.stop();
  } else {
    delay(50);
  }
}

/* ------------------------------ High Level Robot-commands ------------------------------ */

const char* tdo = "<td>";
const char* tdc = "</td>";

/**
 * @param html buffer to fill
 * @param inName name of servo
 * @param value value to set
 */
void setupCellControl(String* html, char* inName, char* value) {
  html->concat(tdo);
  html->concat("<a href=\"/set?");
  html->concat(inName);
  html->concat("=");
  html->concat(value);
  html->concat("&ui=1\">");
  html->concat(value);
  html->concat("</a>");  
  html->concat(tdc);  
}

/**
 * @param html buffer to fill
 * @param value current value
 * @param inName name of servo
 * @param disName display-name
 * @param all indicator to show all options
 */
void setupControllerRow(String* html, int value, char* inName, char* disName, boolean all) {
  html->concat("<tr>");

  html->concat(tdo); html->concat(disName); html->concat(tdc);
  if (all) {
    setupCellControl(html, inName, "-20");
    setupCellControl(html, inName, "-10");
  } else {
    html->concat("<td></td><td></td>");
  }
  setupCellControl(html, inName, "-5");
  setupCellControl(html, inName, "-1");

  html->concat(tdo); html->concat(value); html->concat(tdc);

  setupCellControl(html, inName, "+1");
  setupCellControl(html, inName, "+5");
  if (all) {
    setupCellControl(html, inName, "+10");
    setupCellControl(html, inName, "+20");
  } else {
    html->concat("<td></td><td></td>");    
  }  
  html->concat("</tr>");
}

/**
 * Create HTML-page to control robot.
 * @param client tcp-client
 */
void writeControllerPage(WiFiClient* client) {
  String html = "<!DOCTYPE HTML>\r\n";
  html.concat("<html><head><title>BrachiumRobotic</title>");
  html.concat("<meta http-equiv=\"refresh\" content=\"300\"></head><body>\r\n");
  html.concat("<h1>BrachiumRobotic</h1>");
  html.concat("Written by Marcus and Georg Miss<br/><br/>");
  html.concat("<table>");
  setupControllerRow(&html, roboM1, "m1", "Servo M1", true);
  setupControllerRow(&html, roboM2, "m2", "Servo M2", true);
  setupControllerRow(&html, roboM3, "m3", "Servo M3", true);
  setupControllerRow(&html, roboM4, "m4", "Servo M4", true);
  setupControllerRow(&html, roboM5, "m5", "Servo M5", true);
  setupControllerRow(&html, roboM6, "m6", "Servo M6", true);
  setupControllerRow(&html, roboSpeed, "s", "Speed", false);
  html.concat("</table>");
  html.concat("</body></html>\r\n\r\n");
  sendResponse(client, HTTP_OK, NULL, &mimeHtml, &html);
}

/**
 * Handle /status-command: display web-page.
 * @param client tcp-client
 */
void robotCmdInfo(WiFiClient* client) {
  Serial.println("CMD: Info");
  writeControllerPage(client);
}

/**
 * Handle /sky-command: let arm point into sky.
 * @param client tcp-client
 */
void robotCmdSky(WiFiClient* client) {
  Serial.println("CMD: Arm into sky");
  robotArmIntoSky(); 
  String json = buildStatusObject();
  sendResponse(client, HTTP_OK, NULL, &mimeJson, &json);
}

/**
 * Handle /safe-command: move arm into save position.
 * @param client tcp-client
 */
void robotCmdSafe(WiFiClient* client) {
  Serial.println("CMD: Arm into safe-position");
  robotIntoSafePosition();
  String json = buildStatusObject();
  sendResponse(client, HTTP_OK, NULL, &mimeJson, &json);
}

/**
 * Handle /status-command: just deliver current values.
 * @param client tcp-client
 */
void robotCmdStatus(WiFiClient* client) {
  Serial.println("CMD: Status");
  String json = buildStatusObject();
  sendResponse(client, HTTP_OK, NULL, &mimeJson, &json);
}

/**
 * Handle /gc-command: Close gripper.
 * @param client tcp-client
 */
void robotCmdGripperClose(WiFiClient* client) {
  Serial.println("CMD: Close gripper");
  robotArmMove(roboSpeed, roboM1, roboM2, roboM3, roboM4, roboM5, GRIPPER_CLOSE);
  String json = buildStatusObject();
  sendResponse(client, HTTP_OK, NULL, &mimeJson, &json);
}

/**
 * Handle /go-command: Open gripper.
 * @param client tcp-client
 */
void robotCmdGripperOpen(WiFiClient* client) {
  Serial.println("CMD: Open gripper");
  robotArmMove(roboSpeed, roboM1, roboM2, roboM3, roboM4, roboM5, GRIPPER_OPEN);
  String json = buildStatusObject();
  sendResponse(client, HTTP_OK, NULL, &mimeJson, &json);
}

/**
 * Handle /status-command: Move arm using values.
 * @param client tcp-client
 * @param request HTTP-request to process (without method)
 */
void robotCmdSet(WiFiClient* client, String* request) {
  Serial.println("CMD: Move arm");
  int ui = searchRequestParam(request, "ui", 0);
  roboM1 = searchRequestParam(request, "m1", roboM1);
  roboM2 = searchRequestParam(request, "m2", roboM2);
  roboM3 = searchRequestParam(request, "m3", roboM3);
  roboM4 = searchRequestParam(request, "m4", roboM4);
  roboM5 = searchRequestParam(request, "m5", roboM5);
  roboM6 = searchRequestParam(request, "m6", roboM6);
  roboSpeed = searchRequestParam(request, "s", roboSpeed);
  robotArmMove(roboSpeed, roboM1, roboM2, roboM3, roboM4, roboM5, roboM6);
  String json = buildStatusObject();
  if (ui == 0) {
    sendResponse(client, HTTP_OK, NULL, &mimeJson, &json);
  } else {
    writeControllerPage(client);
  }
}

/**
 * Search for query-parameter.
 * @param request source to red from
 * @param pname name of parameter
 * @param curr current value, will be returned if parameter is not given
 * @return new parameter value
 */
int searchRequestParam(String* request, char* pname, int curr) {
  int qmark = request->indexOf("?");
  if (qmark < 0) {
    return curr; // no query-parameters
  }
  int paramPos = request->indexOf(pname, qmark);
  if (paramPos < qmark) {
    return curr; // no parameter
  }
  int paramAssign = request->indexOf("=", paramPos);
  if (paramAssign < 0) {
    return curr; // no assign
  }
  int paramAmp = request->indexOf("&", paramAssign);
  String paramValue = "";
  if (paramAmp < 0) {
    paramValue = request->substring(paramAssign + 1);
  } else {
    paramValue = request->substring(paramAssign + 1, paramAmp);
  }
  int numValue = paramValue.toInt();
  Serial.print("Param (");
  Serial.print(pname);
  Serial.print(")=");
  Serial.print(paramValue);
  Serial.print("; ");
  Serial.println(numValue);

  if (paramValue.startsWith("+") || paramValue.startsWith("-")) {
    curr += numValue;
  } else {
    curr = numValue;
  }
  return curr;  
}

/**
 * Build JSON-object with current servo-values,.
 */
String buildStatusObject() {
  String json = "{ ";
  json.concat("\"m1\": ");
  json.concat(roboM1);
  json.concat(", \"m2\": ");
  json.concat(roboM2);
  json.concat(", \"m3\": ");
  json.concat(roboM3);
  json.concat(", \"m4\": ");
  json.concat(roboM4);
  json.concat(", \"m5\": ");
  json.concat(roboM5);
  json.concat(", \"m6\": ");
  json.concat(roboM6);
  json.concat(", \"s\": ");
  json.concat(roboSpeed);
  json.concat(" }\r\n");
  return json;  
}

/* ------------------------------ HTTP processing ------------------------------ */

/**
 * Handle HTTP GET-request and delivers response.
 * @param client tcp-client
 * @param request HTTP-request to process (without method)
 */
void serveHttpGetRequest(WiFiClient* client, String* request) {
  if (request->startsWith("/sky")) {
    robotCmdSky(client);
  } else if (request->startsWith("/safe")) {
    robotCmdSafe(client);
  } else if (request->startsWith("/status")) {
    robotCmdStatus(client);
  } else if (request->startsWith("/set")) {
    robotCmdSet(client, request);
  } else if (request->startsWith("/gc")) {
    robotCmdGripperClose(client);
  } else if (request->startsWith("/go")) {
    robotCmdGripperOpen(client);
  } else if (request->equals("/")) {
    robotCmdInfo(client);
  } else {
    sendStatusResponse(client, HTTP_NOT_FOUND, NULL);
  }
}

/**
 * Handle HTTP-request and delivers response.
 * @param client tcp-client
 * @param request HTTP-request to process
 */
void serveHttpRequest(WiFiClient* client, String* request) {
  if (request->startsWith("GET")) {
    String requestURL = request->substring(4);
    requestURL.trim();
    serveHttpGetRequest(client, &requestURL);
  } else {
    sendStatusResponse(client, HTTP_NOT_IMPLEMENTED, NULL);
  }
}

/**
 * Send small HTTP-status response.
 * @param client tcp-client
 * @param code HTTP-code
 * @param msg Optional HTTP-message
 */
void sendStatusResponse(WiFiClient* client, int code, String* msg) {
  sendResponse(client, code, msg,  NULL, NULL);
}

/**
 * Send full HTTP-response.
 * @param client tcp-client
 * @param code HTTP-code
 * @param msg Optional HTTP-message
 * @param ctype Optional content-type
 * @param content Optional content
 */
void sendResponse(WiFiClient* client, int code, String* msg, String* ctype, String* content) {
  // Response line
  client->print("HTTP/1.1 ");
  client->print(code);
  client->print(" ");
  if (msg != NULL) {
    client->println(msg->c_str());
  } else {
    client->println( code == HTTP_OK ? "OK" : "");
  }
  client->println("Cache-Control: no-cache, must-revalidate");
  client->println("Connection: close");
  // Optional content
  if (ctype != NULL && content != NULL) {
    client->print("Content-type: ");
    client->println(ctype->c_str());
  }
  if (ctype != NULL && content != NULL) {
    client->print("Content-Length: ");
    client->print(content->length());
    client->println();
    client->println();
    client->println(content->c_str());
  }
  client->flush();
}

/**
 * Read next HTTP-request.
 * @param client tcp-client
 * @return received HTTP-request 
 */
String readHttpRequest(WiFiClient* client) {
  String httpLine = "";
  while (client->connected()) {
    if (client->available()) {
      char ch = client->read();
      if (ch == '\n' || ch == '\r') {
        break;
      } else {
        httpLine += ch;
      }
    }
  }
  // Read/ignore trailing stuff from http-request (headers, etc...)
  while (client->available()) {
    client->read();
  }
  // Remove http-version
  int httpTypePos = httpLine.indexOf("HTTP/");
  if (httpTypePos > 0) {
    httpLine = httpLine.substring(0, httpTypePos - 1);
  }
  httpLine.trim();
  return httpLine;
}

/* ------------------------------ Low Level Robot-commands ------------------------------ */

/**
 * Arm of robot should point into sky.
 */
void robotArmIntoSky() {
  robotArmMove(SPEED_SLOW, 0, 90, 90, 90, roboM5, roboM6); 
}

/**
 * Move arm into save-position.
 */
void robotIntoSafePosition() {
  robotArmMove(SPEED_SLOW, 90, 45, 180, 180, 90, GRIPPER_OPEN);
}

/**
 * Move arm into home-position.
 */
void robotIntoHomePosition() {
  robotArmMove(SPEED_SLOW, 90, 90, 90, 90, 90, 10);
}

/**
 * Move arm.
 * @param sd speed
 * @param mx1 value for servo 1
 * @param mx2 value for servo 2
 * @param mx3 value for servo 3
 * @param mx4 value for servo 4
 * @param mx5 value for servo 5
 * @param mx6 value for servo 6
 */
void robotArmMove(int sd, int mx1, int mx2, int mx3, int mx4, int mx5, int mx6) {
  roboSpeed =  adjustValue(sd, SPEED_MIN, SPEED_MAX);
  roboM1 = adjustValue(mx1, M1_MIN, M1_MAX);
  roboM2 = adjustValue(mx2, M2_MIN, M2_MAX);
  roboM3 = adjustValue(mx3, M3_MIN, M3_MAX);
  roboM4 = adjustValue(mx4, M4_MIN, M4_MAX);
  roboM5 = adjustValue(mx5, M5_MIN, M5_MAX);
  roboM6 = adjustValue(mx6, M6_MIN, M6_MAX);

  Serial.print("Servos: M1=");
  Serial.print(roboM1);
  Serial.print(",M2=");
  Serial.print(roboM2);
  Serial.print(",M3=");
  Serial.print(roboM3);
  Serial.print(",M4=");
  Serial.print(roboM4);
  Serial.print(",M5=");
  Serial.print(roboM5);
  Serial.print(",M6=");
  Serial.println(roboM6);
  Serial.flush();

  Braccio.ServoMovement(roboSpeed, roboM1, roboM2, roboM3, roboM4, roboM5, roboM6);
}

/**
 * Ensure value is in limit. 
 * @param v new value
 * @param miv minimum value
 * @param mav maximum value
 * @return adjusted value
 */
int adjustValue(int v, int miv, int mav) {
  if (v < miv) {
    return miv;
  } else if (v > mav) {
    return mav;
  } else {
    return v;
  }
}

/* ------------------------------ WIFI ------------------------------ */

/**
 * Setup WIFI.
 * @return true in case of success, otherwise false
 */
bool setupWifi() {
  String fv = WiFi.firmwareVersion();
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("No Wifi-module available.");
    return false;
  }  
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
  WiFi.setHostname(HOSTNAME);
  while (wifiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(wifiSsid);
    wifiStatus = WiFi.begin(wifiSsid, wifiPass);
    if (wifiStatus != WL_CONNECTED) {
      delay(10000);
    }
  }

  roboServer.begin();

  printCurrentNet();
  printWifiData();
  return true;
}

/**
 * Print IP and MAC-address.
 */
void printWifiData() {
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

/**
 * Show info about current net.
 */
void printCurrentNet() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

/**
 * Print MAC-address.
 * @param mac related adress
 */
void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

/* EOF */
