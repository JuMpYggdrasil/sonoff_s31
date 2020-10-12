#define HOST_NAME "s31_1"

#define USE_WiFiManager true
#define USE_MDNS true
#define USE_FTP true
#define USE_OTA true//keep true, if possible.
#define USE_TELNET true
#define REDIS_GET_TEST false

#define REDIS_ADDR "siriprapawat.trueddns.com"//"192.168.1.22"
#define REDIS_PORT 14285//6379
#define REDIS_PASS "61850"

#define REDIS_EEPROM_ADDR_BEGIN 0//address of REDIS_DEVKEY
#define REDIS_EEPROM_SERVER_ADDR 100
#define REDIS_EEPROM_SERVER_PORT 130
#define REDIS_EEPROM_SERVER_PASS 132
//#define REDIS_EEPROM_SERVER_xx 140

#define REDIS_DEVKEY "ACBUSx220xengMMTR1/MMXU1$MX$"
#define REDIS_VOLTAGE "PhV$mag$f"
#define REDIS_CURRENT "A$mag$f"
#define REDIS_ACTIVEPOWER "TotW$mag$f"//P
#define REDIS_APPARENTPOWER "TotVA$mag$f"//S
#define REDIS_REACTIVEPOWER "TotVAr$mag$f"//Q
#define REDIS_POWERFACTOR "TotPF$mag$f"
#define REDIS_ENERGY "TotWh$mag$f"
//#define REDIS_FREQUENCY "Hz$mag$f"
#define REDIS_TIMESTAMP "Time$mag$f"


#if defined ESP8266

#if !USE_WiFiManager

#ifndef STASSID
#define STASSID "JUMP"
#define STAPSK  "025260652"
#endif

#endif

//////// Libraries
//// Hardware config
/// GPIOs
#define PUSHBUTTON_PIN   0
#define RELAY_PIN       12//relay(active high) include red led(active high)
#define LED_PIN         13//blue led(active high/low???)
/// UART0
//#define esp8266_TX_PIN 1//connect GPIO_CSE7766_RX PIN8(RI)
//#define esp8266_RX_PIN 3//connect GPIO_CSE7766_TX PIN6(TI)
/// i2c
//#define SDA 4//GPIO4 as D RX
//#define SCL 5//GPIO5 as D TX

//using arduino ide 1.8.13
// Includes of ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#if USE_WiFiManager
#include <WiFiManager.h>//tzapu v2.0.3-alpha
#endif

#if USE_MDNS
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#endif

#if USE_FTP
#include <ESP8266FtpServer.h>//nailbuster
#endif

#if USE_OTA
#include <ElegantOTA.h>//Ayush Sharma v2.2.4
#endif

#if USE_TELNET
// Remote debug over WiFi - not recommended for production, only for development
#include "RemoteDebug.h" //https://github.com/JoaoLopesF/RemoteDebug
#endif

#include <Redis.h>//Ryan Joseph v2.1.3
#include <NTPClient.h>//Fabrice Weinberg v3.2.0
#include <WiFiUdp.h>
#include "CSE7766.h"//ingeniuske custom-modified
#include <PinButton.h>//Martin Poelstra v1.0.0
#include <EEPROM.h>
#include <singleLEDLibrary.h>//Pim Ostendorf v1.0.0
#include <FS.h>

#else
#error "The board must be ESP8266"
#endif // ESP8266


#if !USE_WiFiManager
// SSID and password
const char* ssid PROGMEM = STASSID;
const char* password PROGMEM = STAPSK;
#endif

// webpage
const char* www_username PROGMEM = "admin";
const char* www_password PROGMEM = "admin";

#if USE_FTP
// FTP
const char* ftp_user PROGMEM = "admin";
const char* ftp_password PROGMEM = "admin";
#endif

String redis_deviceKey = REDIS_DEVKEY;
String redis_server_addr = REDIS_ADDR;
uint16_t redis_server_port = REDIS_PORT;
String redis_server_pass = REDIS_PASS;

const char* redis_voltage PROGMEM = REDIS_VOLTAGE;
const char* redis_current PROGMEM = REDIS_CURRENT;
const char* redis_activepower PROGMEM = REDIS_ACTIVEPOWER;
const char* redis_apparentpower PROGMEM = REDIS_APPARENTPOWER;
const char* redis_reactivepower PROGMEM = REDIS_REACTIVEPOWER;
const char* redis_powerfactor PROGMEM = REDIS_POWERFACTOR;
const char* redis_energy PROGMEM = REDIS_ENERGY;
const char* redis_timestamp PROGMEM = REDIS_TIMESTAMP;

String hostNameWifi;

#if USE_WiFiManager
WiFiManager wm;
#endif

#if !USE_MDNS
IPAddress local_IP(192, 168, 1, 17);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
#endif

#if USE_FTP
FtpServer ftpSrv;
#endif

#if USE_TELNET
RemoteDebug Debug;
#endif

// Time
uint32_t mLastTime = 0;
uint32_t mTimeSeconds = 0;

bool redisInterface_flag = false;
int redisInterface_state = 0;

CSE7766 cse7766;
PinButton S31_Button(PUSHBUTTON_PIN);
ESP8266WebServer server(80);
WiFiClient redisConn;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.navy.mi.th", 25200);//GMT+7 =3600*7 =25200
sllib blue_led(LED_PIN);

int init_pattern[] = {1900, 100};
int normal_pattern[] = {1500, 100, 300, 100};
int error_pattern[] = {1100, 100, 300, 100, 300, 100};
int noAuthen_pattern[] = {700, 100, 300, 100, 300, 100, 300, 100};
int unauthen_pattern[] = {300, 100, 300, 100, 300, 900};
int waitReset_pattern[] = {50, 50};

//prototype declare
void startupConfig(void);
void startupLog(void);
void clickbutton_action(void);
void PowerSensorDisplay(void);
void redisInterface_handle(void);

void handleRoot(void);
void handleNotFound(void);
void handleConfig(void);
void handleInfo(void);

void EEPROM_WriteString(char addr, String data);
String EEPROM_ReadString(char addr);
void EEPROM_WriteUInt(char address, unsigned int number);
unsigned int EEPROM_ReadUInt(char address);

const char WEB_HEAD[] PROGMEM = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
const char WEB_STYLE[] PROGMEM = "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">";
const char WEB_BODY_START[] PROGMEM = "</head><body>";
const char WEB_SIDENAV[] PROGMEM = "<div class=\"sidenav\"><a href=\"/\">Home</a><a href=\"/info\">Info</a><a href=\"/update\">OTA</a></div>";
const char WEB_CONTENT_START[] PROGMEM = "<div class=\"content\"><h2><span style=\"color: maroon\">I</span>ED</h2>";
const char WEB_BODY_HTML_END[] PROGMEM = "</div></body></html>";//with end content
const char WEB_SCRIPT_START[] PROGMEM = "</div></body><script>";//with end content
const char WEB_SCRIPT_HTML_END[] PROGMEM = "</script></html>";

void startupConfig(void) {
  //  //timeClient.update();
  //  timeClient.forceUpdate();
  //
  //  File configFile = SPIFFS.open("/config.txt", "r");
  //  if (!configFile)
  //  {
  //    return;
  //  }
  //  while (configFile.available())
  //  {
  //    String line = configFile.readStringUntil('\n');
  //    //      String resultstr;
  //    //      if (line.startsWith("xxxx")) {
  //    //        resultstr = line.substring(line.indexOf(",") + 1);
  //    //        resultstr.trim();
  //    //        resultstr.toCharArray(XXXX, resultstr.length() + 1);//global char* XXXX = "initial";
  //    //      } else if (line.startsWith("yyyy")) {
  //    //        resultstr = line.substring(line.indexOf(",") + 1);
  //    //        resultstr.trim();
  //    //        resultstr.toCharArray(YYYY, resultstr.length() + 1);//global char* YYYY = "initial";
  //    //      }
  //  }
  //  configFile.close();
}
void startupLog(void) {
  //timeClient.update();
  timeClient.forceUpdate();
  timeClient.forceUpdate();

  //a+ -> Open for reading and appending (writing at end of file).
  //The file is created if it does not exist.
  File logFile = SPIFFS.open("/log.txt", "a+");
  if (!logFile) {
    return;
  }
  if (logFile.size() < 2000) {
    int bytesWritten = logFile.print(timeClient.getEpochTime());
    bytesWritten = logFile.print(",");
    bytesWritten = logFile.println( ESP.getResetReason());
    //bytesWritten = logFile.println(timeClient.getFormattedTime());

  }
  logFile.close();
}

void setup() {
  // Initialize
  ESP.wdtDisable();

  cse7766.setRX(1);
  cse7766.begin();// will initialize serial to 4800 bps

  //  pinMode(PUSHBUTTON_PIN, INPUT);
  //  pinMode(LED_PIN, OUTPUT);
  //  digitalWrite(LED_PIN, LOW);
  blue_led.setOffSingle();//turn on blue led
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);//turn off red led

  redis_deviceKey.reserve(80);
  redis_server_addr.reserve(30);

  EEPROM.begin(512);
  timeClient.begin();

  blue_led.setPatternSingle(waitReset_pattern, 2);

  unsigned long exitTime = millis() + 5000;
  while (millis() < exitTime) {
    if (S31_Button.isSingleClick()) {
      //restore to default
      EEPROM_WriteString(REDIS_EEPROM_ADDR_BEGIN, REDIS_DEVKEY);
      EEPROM_WriteString(REDIS_EEPROM_SERVER_ADDR, REDIS_ADDR);
      EEPROM_WriteUInt(REDIS_EEPROM_SERVER_PORT, REDIS_PORT);
      EEPROM_WriteString(REDIS_EEPROM_SERVER_PASS, REDIS_PASS);

      redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
      redis_server_addr = EEPROM_ReadString(REDIS_EEPROM_SERVER_ADDR);
      redis_server_port = EEPROM_ReadUInt(REDIS_EEPROM_SERVER_PORT);
      redis_server_pass = EEPROM_ReadString(REDIS_EEPROM_SERVER_PASS);

#if USE_WiFiManager
      wm.resetSettings();
      //ESP.restart();
#endif
    }
    S31_Button.update();
    blue_led.update();
    timeClient.update();
  }
  blue_led.setOffSingle();//turn on blue led

#if !USE_MDNS
  WiFi.config(local_IP, primaryDNS, gateway, subnet);
#endif

  // WiFi connection
  WiFi.mode(WIFI_STA);

#if USE_WiFiManager
  //sets timeout for which to attempt connecting, useful if you get a lot of failed connects
  //wm.setConnectTimeout(20);     // how long to try to connect for before continuing
  // ConnectTimeout calback???? default 10 sec

  wm.setDebugOutput(false);
  bool res = wm.autoConnect();    // password protected ap
#else
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    yield();
  }
#endif

  // Register host name in WiFi and mDNS
  hostNameWifi = HOST_NAME;
  hostNameWifi.concat(".local");

#ifdef ESP8266 // Only for it
  WiFi.hostname(hostNameWifi);
#endif

#if USE_MDNS
  if (MDNS.begin(HOST_NAME)) {
    //Serial.print("* MDNS responder started. Hostname -> ");
    //Serial.println(HOST_NAME);
  }
  MDNS.addService("telnet", "tcp", 23);
  MDNS.addService("http", "tcp", 80);
#endif

  ////==== webpage assign section ====
  server.on("/", HTTP_GET, handleRoot);
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.serveStatic("/log.txt", SPIFFS, "/log.txt");
  server.onNotFound(handleNotFound);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/on", HTTP_POST, []() {
    digitalWrite(RELAY_PIN, HIGH);
    server.send(204);
  });
  server.on("/off", HTTP_POST, []() {
    digitalWrite(RELAY_PIN, LOW);
    server.send(204);
  });
  server.on("/xVal", HTTP_GET, []() {//using AJAX
    String xValue = "";
    if (digitalRead(RELAY_PIN)) {
      xValue.concat("on");
    } else {
      xValue.concat("off");
    }
    xValue.concat("," + String(cse7766.getVoltage()));
    xValue.concat("," + String(cse7766.getCurrent()));
    xValue.concat("," + String(cse7766.getActivePower()));
    xValue.concat("," + String(cse7766.getApparentPower()));
    xValue.concat("," + String(cse7766.getReactivePower()));
    xValue.concat("," + String(cse7766.getPowerFactor()));
    xValue.concat("," + String(cse7766.getEnergy()));

    server.send(200, "text/plain", xValue);
  });

#if USE_OTA
  ElegantOTA.begin(&server);    // Start ElegantOTA
#endif

  server.begin();

  bool spiffsResult = SPIFFS.begin();
  if (spiffsResult) {
#if USE_FTP
    /////FTP Setup, ensure SPIFFS is started before ftp;  /////////
    ftpSrv.begin(ftp_user, ftp_password);// Then start FTP server when WiFi connection in On
#endif

    startupConfig();
    startupLog();
  }

#if USE_TELNET
  //// Initialize RemoteDebug
  Debug.begin(HOST_NAME); // Initialize the WiFi server
  Debug.setResetCmdEnabled(true); // Enable the reset command
  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors

  /// Debug levels
  debugA("* This is a message of debug level ANY");//always show
  debugV("* This is a message of debug level VERBOSE");
  debugD("* This is a message of debug level DEBUG");
  debugI("* This is a message of debug level INFO");
  debugW("* This is a message of debug level WARNING");
  debugE("* This is a message of debug level ERROR");
#endif

  redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
  redis_server_addr = EEPROM_ReadString(REDIS_EEPROM_SERVER_ADDR);
  redis_server_port = EEPROM_ReadUInt(REDIS_EEPROM_SERVER_PORT);
  redis_server_pass = EEPROM_ReadString(REDIS_EEPROM_SERVER_PASS);

#if USE_TELNET
  debugI("IP address: %s", WiFi.localIP().toString().c_str());
  debugI("redis_deviceKey: %s", redis_deviceKey.c_str());
  debugI("redis_server_addr: %s", redis_server_addr.c_str());
  debugI("redis_server_port: %d", redis_server_port);
  debugI("redis_server_pass: %s", redis_server_pass.c_str());
#endif

  blue_led.setOnSingle();//turn off blue led
  digitalWrite(RELAY_PIN, HIGH);//turn on red led

#if USE_WiFiManager
  if (res) {//normal operation
    blue_led.setPatternSingle(init_pattern, 2);
  } else {
    blue_led.setPatternSingle(unauthen_pattern, 6);
  }
#endif
  ESP.wdtEnable(WDTO_8S);
}

void loop()
{
  // Each second
  if ((millis() - mLastTime) >= 1000) {
    // Time
    mLastTime = millis();
    mTimeSeconds++;

    if (mTimeSeconds % 5 == 0) { // Each 5 seconds
      PowerSensorDisplay();
    }

    redisInterface_flag = true;
  }
  cse7766.handle();// CSE7766 handle
  clickbutton_action();
  S31_Button.update();
  blue_led.update();
  server.handleClient();
  timeClient.update();
  redisInterface_handle();

#if USE_MDNS
  MDNS.update();
#endif

#if USE_FTP
  ftpSrv.handleFTP();
#endif

#if USE_TELNET
  Debug.handle();// RemoteDebug handle
#endif

  ESP.wdtFeed();

  // Give a time for ESP
  yield();
}

void handleRoot(void) {
  String rootPage = "";
  //authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
  redis_server_addr = EEPROM_ReadString(REDIS_EEPROM_SERVER_ADDR);
  redis_server_port = EEPROM_ReadUInt(REDIS_EEPROM_SERVER_PORT);
  redis_server_pass = EEPROM_ReadString(REDIS_EEPROM_SERVER_PASS);

  rootPage.concat(FPSTR(WEB_HEAD));
  rootPage.concat(FPSTR(WEB_STYLE));
  rootPage.concat(FPSTR(WEB_BODY_START));
  rootPage.concat(FPSTR(WEB_SIDENAV));
  rootPage.concat(FPSTR(WEB_CONTENT_START));

  rootPage.concat("<div>" + hostNameWifi + " : " + WiFi.localIP().toString() + "</div><br>");
  rootPage.concat(F("<form action=\"/config\" method=\"POST\">"));
  rootPage.concat(F("<label for=\"name1\">Device key:  </label><br>"));
  rootPage.concat("<input type=\"text\" name=\"name1\" placeholder=\"" + redis_deviceKey + "\"><br>");
  rootPage.concat(F("<label for=\"name2\">Redis addr:  </label><br>"));
  rootPage.concat("<input type=\"text\" name=\"name2\" placeholder=\"" + redis_server_addr + "\"><br>");
  rootPage.concat(F("<label for=\"name3\">Redis port:  </label><br>"));
  rootPage.concat("<input type=\"text\" name=\"name3\" placeholder=\"" + String(redis_server_port) + "\"><br>");
  rootPage.concat(F("<label for=\"name4\">Redis pass:  </label><br>"));
  rootPage.concat("<input type=\"password\" name=\"name4\" placeholder=\"" + redis_server_pass + "\"><br>");
  rootPage.concat(F("<input type=\"submit\" value=\"Save\">"));
  rootPage.concat(F("</form></br>"));
  rootPage.concat(F("<form action=\"/on\" method=\"POST\">"));
  rootPage.concat(F("<input type=\"submit\" value=\"relay on\">"));
  rootPage.concat(F("</form>"));
  rootPage.concat(F("<form action=\"/off\" method=\"POST\">"));
  rootPage.concat(F("<input type=\"submit\" value=\"relay off\">"));
  rootPage.concat(F("</form>"));
  rootPage.concat(F("<div id='x0Val'></div>"));
  rootPage.concat(F("<div id='x1Val'></div>"));
  rootPage.concat(FPSTR(WEB_SCRIPT_START));
  rootPage.concat("setInterval(function xmlDataRequest(){");
  rootPage.concat("var xhttp = new XMLHttpRequest();");
  rootPage.concat("xhttp.onreadystatechange = function() {");
  rootPage.concat("if (this.readyState == 4 && this.status == 200) {");
  rootPage.concat("var resultText = this.responseText.split(',');");
  rootPage.concat("document.getElementById('x0Val').innerHTML = resultText[0];");
  rootPage.concat("document.getElementById('x1Val').innerHTML = resultText[1];}");
  rootPage.concat("};");//function()
  rootPage.concat("xhttp.open('GET', '/xVal', true);");
  rootPage.concat("xhttp.send();");
  rootPage.concat("}, 2000 ) ; ");
  rootPage.concat(FPSTR(WEB_SCRIPT_HTML_END));
  //rootPage.concat(FPSTR(WEB_BODY_HTML_END));

  // Root web page
  server.send(200, "text / html", rootPage);
}
void handleNotFound() {
  server.send(404, "text / plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
void handleInfo(void) {
  String infoPage = "";
  //authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  infoPage.concat(FPSTR(WEB_HEAD));
  infoPage.concat(FPSTR(WEB_STYLE));
  infoPage.concat(FPSTR(WEB_BODY_START));
  infoPage.concat(FPSTR(WEB_SIDENAV));
  infoPage.concat(FPSTR(WEB_CONTENT_START));

  infoPage.concat(F("<div style=\"font-weight:bold\">Network</div>"));
  infoPage.concat("<div>IP: " + WiFi.localIP().toString() + "</div>");
  infoPage.concat("<div>SSID: " + WiFi.SSID() + "</div>");

  infoPage.concat(F("<br><div style=\"font-weight:bold\">Hardware</div>"));
  infoPage.concat(F("<div>Relay Status: <span id='x0Val'></span></div>"));
  infoPage.concat(F("<div>Voltage: <span id='x1Val'></span></div>"));
  infoPage.concat(F("<div>Current: <span id='x2Val'></span></div>"));
  infoPage.concat(F("<div>ActivePower: <span id='x3Val'></span></div>"));
  infoPage.concat(F("<div>ApparentPower: <span id='x4Val'></span></div>"));
  infoPage.concat(F("<div>ReactivePower: <span id='x5Val'></span></div>"));
  infoPage.concat(F("<div>PowerFactor: <span id='x6Val'></span></div>"));
  infoPage.concat(F("<div>Energy: <span id='x7Val'></span></div>"));

  FSInfo fs_info;
  SPIFFS.info(fs_info);

  infoPage.concat(F("<br><div style=\"font-weight:bold\">Filesystem information</div>"));
  infoPage.concat("<div>totalBytes " + String(fs_info.totalBytes) + "</div>");
  infoPage.concat("<div>usedBytes " + String(fs_info.usedBytes) + "</div>");

  Dir dir = SPIFFS.openDir ("");
  while (dir.next ()) {
    infoPage.concat("<div>" + String(dir.fileName ()) + "," + dir.fileSize () + "</div>");
  }

  infoPage.concat(FPSTR(WEB_SCRIPT_START));
  infoPage.concat("setInterval(function xmlDataRequest(){");
  infoPage.concat("var xhttp = new XMLHttpRequest();");
  infoPage.concat("xhttp.onreadystatechange = function() {");
  infoPage.concat("if (this.readyState == 4 && this.status == 200) {");
  infoPage.concat("var resultText = this.responseText.split(',');");
  infoPage.concat("document.getElementById('x0Val').innerHTML = resultText[0];");
  infoPage.concat("document.getElementById('x1Val').innerHTML = resultText[1];");
  infoPage.concat("document.getElementById('x2Val').innerHTML = resultText[2];");
  infoPage.concat("document.getElementById('x3Val').innerHTML = resultText[3];");
  infoPage.concat("document.getElementById('x4Val').innerHTML = resultText[4];");
  infoPage.concat("document.getElementById('x5Val').innerHTML = resultText[5];");
  infoPage.concat("document.getElementById('x6Val').innerHTML = resultText[6];");
  infoPage.concat("document.getElementById('x7Val').innerHTML = resultText[7];}");
  infoPage.concat("};");//function()
  infoPage.concat("xhttp.open('GET', '/xVal', true);");
  infoPage.concat("xhttp.send();");
  infoPage.concat("}, 2000 ) ; ");
  infoPage.concat(FPSTR(WEB_SCRIPT_HTML_END));
  //infoPage.concat(FPSTR(WEB_BODY_HTML_END));
  // Info web page
  server.send(200, "text/html", infoPage);
}

void handleConfig(void) {
  String configPage = "";

  configPage.concat(FPSTR(WEB_HEAD));
  configPage.concat(FPSTR(WEB_STYLE));
  configPage.concat(FPSTR(WEB_BODY_START));
  configPage.concat(FPSTR(WEB_SIDENAV));
  configPage.concat(FPSTR(WEB_CONTENT_START));

  configPage.concat(F("<div>config ok</div>"));
  configPage.concat(FPSTR(WEB_BODY_HTML_END));
  if ((server.hasArg("name1")) && (server.hasArg("name2")) && (server.hasArg("name3")) && (server.hasArg("name4"))) {
    EEPROM_WriteString(REDIS_EEPROM_ADDR_BEGIN, server.arg("name1"));
    EEPROM_WriteString(REDIS_EEPROM_SERVER_ADDR, server.arg("name2"));
    EEPROM_WriteUInt(REDIS_EEPROM_SERVER_PORT, server.arg("name3").toInt());
    EEPROM_WriteString(REDIS_EEPROM_SERVER_PASS, server.arg("name4"));

    redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
    redis_server_addr = EEPROM_ReadString(REDIS_EEPROM_SERVER_ADDR);
    redis_server_port = EEPROM_ReadUInt(REDIS_EEPROM_SERVER_PORT);
    redis_server_pass = EEPROM_ReadString(REDIS_EEPROM_SERVER_PASS);
    server.send(200, "text/html", configPage);
#if USE_TELNET
    debugD("config ok");
#endif
  } else {
    server.send(200, "text/plain", "config error");
#if USE_TELNET
    debugE("config error");
#endif
  }
}

void clickbutton_action(void) {
  if (S31_Button.isClick()) {
#if USE_TELNET
    debugD("Click");
#endif
  }
  if (S31_Button.isSingleClick()) {
#if USE_TELNET
    debugD("SingleClick");
    if (digitalRead(RELAY_PIN)) {
      debugI("status on\n");
    } else {
      debugI("status off\n");
    }
    debugI("redis_deviceKey: %s", redis_deviceKey.c_str());
    debugI("redis_server_addr: %s", redis_server_addr.c_str());
    debugI("redis_server_port: %d", redis_server_port);
    debugI("redis_server_pass: %s", redis_server_pass.c_str());
    //debugI("cse7766: %s", cse7766.description().c_str());
    debugW("ssid %s", WiFi.SSID().c_str());

    //WL_IDLE_STATUS: it is a temporary status assigned when WiFi.begin() is called and remains active until the number of attempts expires (resulting in WL_CONNECT_FAILED) or a connection is established (resulting in WL_CONNECTED);

    if (WiFi.status() == WL_CONNECTED) {
      debugW("connected");
    } else if (WiFi.status() == WL_NO_SHIELD) {
      debugW("no WiFi shield is present");
    } else if (WiFi.status() == WL_IDLE_STATUS) {
      debugW("idle");
    } else if (WiFi.status() == WL_NO_SSID_AVAIL) {
      debugW("no SSID are available");
    } else if (WiFi.status() == WL_SCAN_COMPLETED) {
      debugW("scan networks is completed");
    } else if (WiFi.status() == WL_CONNECT_FAILED) {
      debugW("connection fails for all the attempts");
    } else if (WiFi.status() == WL_CONNECTION_LOST) {
      debugW("connection is lost");
    } else if (WiFi.status() == WL_DISCONNECTED) {
      debugW("disconnected from a network");
    } else {
      debugW("status error");
    }
#endif
    blue_led.setPatternSingle(init_pattern, 2);

    File configFile = SPIFFS.open("/config.txt", "r");
    if (configFile)
    {
      while (configFile.available())
      {
        //read line by line from the file
        String line = configFile.readStringUntil('\n');
#if USE_TELNET
        debugI("%s", line.c_str());
#endif
        //        String resultstr;
        //        if (line.startsWith("ssid")) {
        //          resultstr = line.substring(line.indexOf(",") + 1);
        //          resultstr.trim();
        //          resultstr.toCharArray(ssid, resultstr.length() + 1);
        //          debugI("-> %s", resultstr.c_str());
        //          debugI("--> %s", ssid);
        //        } else if (line.startsWith("pass")) {
        //          resultstr = line.substring(line.indexOf(",") + 1);
        //          resultstr.trim();
        //          resultstr.toCharArray(password, resultstr.length() + 1);
        //          debugI("-> %s", resultstr.c_str());
        //          debugI("--> %s", password);
        //        } else {
        //          debugI("%s", line.c_str());
        //        }
      }
    }
    configFile.close();
  }
  if (S31_Button.isDoubleClick()) {
    digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
#if USE_TELNET
    debugD("DoubleClick");
#endif
  }
  if (S31_Button.isLongClick()) {
    //restore to default
    EEPROM_WriteString(REDIS_EEPROM_ADDR_BEGIN, REDIS_DEVKEY);
    EEPROM_WriteString(REDIS_EEPROM_SERVER_ADDR, REDIS_ADDR);
    EEPROM_WriteUInt(REDIS_EEPROM_SERVER_PORT, REDIS_PORT);
    EEPROM_WriteString(REDIS_EEPROM_SERVER_PASS, REDIS_PASS);

    redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
    redis_server_addr = EEPROM_ReadString(REDIS_EEPROM_SERVER_ADDR);
    redis_server_port = EEPROM_ReadUInt(REDIS_EEPROM_SERVER_PORT);
    redis_server_pass = EEPROM_ReadString(REDIS_EEPROM_SERVER_PASS);

#if USE_TELNET
    debugD("LongClick");
#endif

#if USE_WiFiManager
    wm.resetSettings();
    ESP.restart();
#endif
  }
}
void PowerSensorDisplay(void) {
#if USE_TELNET
  debugV("Voltage %.4f V\n", cse7766.getVoltage());
  debugV("Current %.4f A\n", cse7766.getCurrent());
  debugV("ActivePower %.4f W\n", cse7766.getActivePower());
  debugV("ApparentPower %.4f VA\n", cse7766.getApparentPower());
  debugV("ReactivePower %.4f VAR\n", cse7766.getReactivePower());
  debugV("PowerFactor %.4f %%\n", cse7766.getPowerFactor());
  debugV("Energy %.4f Ws\n", cse7766.getEnergy());
#endif
}

void redisInterface_handle(void) {
  String redis_key;
  String cse7766_value;
  String redis_str_result;
  bool redis_bool_result;

  if (redisInterface_flag == true) {
    if (redisInterface_state == 0) {//WIFI CLIENT
      if (!redisConn.connect(redis_server_addr.c_str(), redis_server_port))
      {
#if USE_TELNET
        debugE("Failed to connect to the Redis server!");
#endif
        redisInterface_state = 0;
        redisInterface_flag = false;
        blue_led.setPatternSingle(error_pattern, 6);
        return;
      }
      redisInterface_state++;
    } else if (redisInterface_state == 1) {
      Redis redis(redisConn);
      if (redis_server_pass != "") {
        auto connRet = redis.authenticate(redis_server_pass.c_str());

        if (connRet == RedisSuccess)
        {
#if USE_TELNET
          debugD("Connected to the Redis server!");
#endif
          blue_led.setPatternSingle(normal_pattern, 4);
        } else {
#if USE_TELNET
          debugE("Failed to authenticate to the Redis server! Errno: %d\n", (int)connRet);
#endif
          redisInterface_state = 0;
          redisInterface_flag = false;
          redisConn.stop();
          blue_led.setPatternSingle(unauthen_pattern, 6);
          return;
        }
      } else {
        blue_led.setPatternSingle(noAuthen_pattern, 8);
      }

      // Voltage
      redis_key = redis_deviceKey + String(redis_voltage);
      cse7766_value = String(cse7766.getVoltage());
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
        if (redis_server_pass == "") {//can connect but auth fail
          blue_led.setPatternSingle(unauthen_pattern, 6);
        }
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      // Current
      redis_key = redis_deviceKey + String(redis_current);
      cse7766_value = String(cse7766.getCurrent());
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      // ActivePower
      redis_key = redis_deviceKey + String(redis_activepower);
      cse7766_value = String(cse7766.getActivePower());
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      // ApparentPower
      redis_key = redis_deviceKey + String(redis_apparentpower);
      cse7766_value = String(cse7766.getApparentPower());
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      redisInterface_state++;
    } else if (redisInterface_state == 2) {
      Redis redis(redisConn);
      if (redis_server_pass != "") {
        auto connRet = redis.authenticate(redis_server_pass.c_str());

        if (connRet == RedisSuccess)
        {
#if USE_TELNET
          debugD("Connected to the Redis server!");
#endif
        } else {
#if USE_TELNET
          debugE("Failed to authenticate to the Redis server! Errno: %d\n", (int)connRet);
#endif
          redisInterface_state = 0;
          redisInterface_flag = false;
          redisConn.stop();
          return;
        }
      }

      // ReactivePower
      redis_key = redis_deviceKey + String(redis_reactivepower);
      cse7766_value = String(cse7766.getReactivePower());
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      // PowerFactor
      redis_key = redis_deviceKey + String(redis_powerfactor);
      cse7766_value = String(cse7766.getPowerFactor());
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      // Energy
      redis_key = redis_deviceKey + String(redis_energy);
      cse7766_value = String(cse7766.getEnergy());
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      // TimeStamp
      redis_key = redis_deviceKey + String(redis_timestamp);
      String timeStamp = timeClient.getFormattedTime();
#if USE_TELNET
      debugD("SET %s %s: ", redis_key.c_str(), timeStamp.c_str());
#endif
      redis_bool_result = redis.set(redis_key.c_str(), timeStamp.c_str());
#if USE_TELNET
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }
#if REDIS_GET_TEST
      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());
#endif
#endif

      redisInterface_state++;
    } else if (redisInterface_state == 3) {
      redisConn.stop();
#if USE_TELNET
      debugD("Connection closed!");
#endif

      redisInterface_state = 0;
      redisInterface_flag = false;
    } else {
      redisInterface_state = 0;
      redisInterface_flag = false;
    }
  }
}

void EEPROM_WriteString(char addr, String data)
{
  int _size = data.length();
  int i;
  for (i = 0; i < _size; i++)
  {
    EEPROM.write(addr + i, data[i]);
  }
  EEPROM.write(addr + _size, '\0'); //Add termination null character for String Data
  delay(1);
  EEPROM.commit();
  delay(1);
}
String EEPROM_ReadString(char addr)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len = 0;
  unsigned char k;
  k = EEPROM.read(addr);
  while (k != '\0' && len < 99) //Read until null character
  {
    k = EEPROM.read(addr + len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}
void EEPROM_WriteUInt(char address, unsigned int number)
{
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
  delay(1);
  EEPROM.commit();
  delay(1);
}
unsigned int EEPROM_ReadUInt(char address)
{
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}
