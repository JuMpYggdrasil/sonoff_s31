#define HOST_NAME "s31_1"

#define USE_WiFiManager true
#define USE_MDNS true
#define USE_FTP false
#define USE_OTA true//keep true, if possible.

#define REDIS_ADDR "siriprapawat.trueddns.com"//"192.168.1.22"
#define REDIS_PORT 14285//6379
#define REDIS_PASS "61850"

#define REDIS_EEPROM_ADDR_BEGIN 0//address of REDIS_DEVKEY
#define REDIS_EEPROM_SERVER_ADDR 100
#define REDIS_EEPROM_SERVER_PORT 130
#define REDIS_EEPROM_SERVER_PASS 132
//#define REDIS_EEPROM_SERVER_xx 140

#define REDIS_DEVKEY "AY4_9_CONV_621MxAY4x500xTTK_LINE2xMx/MMXU1$MX$"//TotW$mag$f
#define REDIS_VOLTAGE "Volt$mag$f"
#define REDIS_CURRENT "Curr$mag$f"
#define REDIS_ACTIVEPOWER "TotW$mag$f"//P
#define REDIS_APPARENTPOWER "Va$mag$f"//S
#define REDIS_REACTIVEPOWER "Var$mag$f"//Q
#define REDIS_POWERFACTOR "Pf$mag$f"
#define REDIS_ENERGY "E$mag$f"
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

#if USE_OTA
#include <ElegantOTA.h>//Ayush Sharma v2.2.4
#endif

#if USE_FTP
#include <ESP8266FtpServer.h>//nailbuster
#endif

#include <Redis.h>//Ryan Joseph v2.1.3
#include <NTPClient.h>//Fabrice Weinberg v3.2.0
#include <WiFiUdp.h>
#include "CSE7766.h"//ingeniuske custom-modified
#include <PinButton.h>//Martin Poelstra v1.0.0
#include <EEPROM.h>
#include <singleLEDLibrary.h>//Pim Ostendorf v1.0.0
#include <FS.h>

#if USE_MDNS
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#endif

#else
#error "The board must be ESP8266"
#endif // ESP

// Remote debug over WiFi - not recommended for production, only for development
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug

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


#if !USE_MDNS
IPAddress local_IP(192, 168, 1, 17);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
#endif

// Time
uint32_t mLastTime = 0;
uint32_t mTimeSeconds = 0;

bool redisInterface_flag = false;
int redisInterface_state = 0;

CSE7766 myCSE7766;
PinButton S31_Button(PUSHBUTTON_PIN);
ESP8266WebServer server(80);

#if USE_FTP
FtpServer ftpSrv;
#endif

WiFiClient redisConn;

#if USE_WiFiManager
WiFiManager wm;
#endif

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.navy.mi.th", 25200);//GMT+7 =3600*7 =25200

sllib blue_led(LED_PIN);
RemoteDebug Debug;

int init_pattern[] = {900, 100};
int normal_pattern[] = {1500, 100, 300, 100};
int error_pattern[] = {1100, 100, 300, 100, 300, 100};
//int special_pattern[] = {300,100,300,100,300,900};
//int wifi_pattern[] = {500,1500};

//prototype declare
void clickbutton_action(void);
void PowerSensorDisplay(void);
void redisInterface_handle(void);

void handleRoot();
void handleNotFound();
void handleConfig();

void EEPROM_WriteString(char addr, String data);
String EEPROM_ReadString(char addr);
void EEPROM_WriteUInt(char address, unsigned int number);
unsigned int EEPROM_ReadUInt(char address);

void setup() {
  // Initialize
  myCSE7766.setRX(1);
  myCSE7766.begin(); // will initialize serial to 4800 bps

  //  pinMode(PUSHBUTTON_PIN, INPUT);
  //  pinMode(LED_PIN, OUTPUT);
  //  digitalWrite(LED_PIN, LOW);
  blue_led.setOffSingle();//turn on led
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  redis_deviceKey.reserve(80);
  redis_server_addr.reserve(30);

#if !USE_MDNS
  WiFi.config(local_IP, primaryDNS, gateway, subnet);
#endif

  // WiFi connection
  WiFi.mode(WIFI_STA);

#if USE_WiFiManager
  wm.setDebugOutput(false);
  bool res = wm.autoConnect(); // password protected ap
#else
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    yield();
  }
#endif

  // Register host name in WiFi and mDNS
  String hostNameWifi = HOST_NAME;
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
  server.onNotFound(handleNotFound);
  server.on("/config", HTTP_POST, handleConfig);

#if USE_OTA
  ////==== OTA section ====
  ElegantOTA.begin(&server);    // Start ElegantOTA
#endif

  server.begin();

  if (SPIFFS.begin()) {
#if USE_FTP
    ftpSrv.begin(ftp_user, ftp_password);// Then start FTP server when WiFi connection in On
#endif
  }

  File configFile = SPIFFS.open("/config.txt", "r");
  if (configFile)
  {
    while (configFile.available())
    {
      String line = configFile.readStringUntil('\n');
      //      String resultstr;
      //      if (line.startsWith("xxxx")) {
      //        resultstr = line.substring(line.indexOf(",") + 1);
      //        resultstr.trim();
      //        resultstr.toCharArray(XXXX, resultstr.length() + 1);//global char* XXXX = "initial";
      //      } else if (line.startsWith("yyyy")) {
      //        resultstr = line.substring(line.indexOf(",") + 1);
      //        resultstr.trim();
      //        resultstr.toCharArray(YYYY, resultstr.length() + 1);//global char* YYYY = "initial";
      //      }
    }
  }
  configFile.close();

  //// Initialize RemoteDebug
  Debug.begin(HOST_NAME); // Initialize the WiFi server
  Debug.setResetCmdEnabled(true); // Enable the reset command
  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors

  timeClient.begin();

  digitalWrite(RELAY_PIN, HIGH);

  /// Debug levels
  debugA("* This is a message of debug level ANY");//always show
  debugV("* This is a message of debug level VERBOSE");
  debugD("* This is a message of debug level DEBUG");
  debugI("* This is a message of debug level INFO");
  debugW("* This is a message of debug level WARNING");
  debugE("* This is a message of debug level ERROR");

  EEPROM.begin(512);

  debugI("IP address: %s", WiFi.localIP().toString().c_str());

  redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
  redis_server_addr = EEPROM_ReadString(REDIS_EEPROM_SERVER_ADDR);
  redis_server_port = EEPROM_ReadUInt(REDIS_EEPROM_SERVER_PORT);
  redis_server_pass = EEPROM_ReadString(REDIS_EEPROM_SERVER_PASS);

  debugI("redis_deviceKey: %s", redis_deviceKey.c_str());
  debugI("redis_server_addr: %s", redis_server_addr.c_str());
  debugI("redis_server_port: %d", redis_server_port);
  debugI("redis_server_pass: %s", redis_server_pass.c_str());

  blue_led.setPatternSingle(init_pattern, 2);
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
    if (mTimeSeconds % 30 == 0) {
      redisInterface_flag = true;
    }
  }
  clickbutton_action();

  myCSE7766.handle();// CSE7766 handle
  Debug.handle();// RemoteDebug handle
  S31_Button.update();
  blue_led.update();
  server.handleClient();
#if USE_FTP
  ftpSrv.handleFTP();
#endif
#if USE_MDNS
  MDNS.update();
#endif
  timeClient.update();
  redisInterface_handle();

  // Give a time for ESP
  yield();
}

void handleRoot(void) {
  String rootPage;
  //authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
  redis_server_addr = EEPROM_ReadString(REDIS_EEPROM_SERVER_ADDR);
  redis_server_port = EEPROM_ReadUInt(REDIS_EEPROM_SERVER_PORT);
  redis_server_pass = EEPROM_ReadString(REDIS_EEPROM_SERVER_PASS);

  rootPage = F("<!DOCTYPE html>");
  rootPage.concat(F("<html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"));
  rootPage.concat(F("</head><body>"));
  rootPage.concat(F("<style>"));
  rootPage.concat(F("body{margin:0;font-family:Arial,Helvetica,sans-serif}.sidenav{height:100%;width:180px;position:fixed;z-index:1;top:0;left:0;background-color:maroon;overflow-x:hidden}.sidenav a{color:#fff;padding:16px;text-decoration:none;display:block}.sidenav a:hover{background-color:#fcc;color:maroon}.content{margin-left:180px;padding:20px}"));
  rootPage.concat(F("</style>"));
  rootPage.concat(F("<div class=\"sidenav\"><a href=\"/\">Home</a><a href=\"/update\">OTA</a></div>"));
  rootPage.concat(F("<div class=\"content\">"));
  rootPage.concat(F("<h2><span style=\"color: maroon\">I</span>ED</h2>"));
  rootPage.concat("<div>To upload \"http://" + WiFi.localIP().toString() + "/update\"</div></br>");
  rootPage.concat(F("<form action=\"/config\" method=\"POST\">"));
  rootPage.concat(F("<label for=\"name1\">Device key:  </label>"));
  rootPage.concat("<input type=\"text\" style=\"width:70%\" name=\"name1\" placeholder=\"" + redis_deviceKey + "\"></br>");
  rootPage.concat(F("<label for=\"name2\">Redis addr:  </label>"));
  rootPage.concat("<input type=\"text\" style=\"width:70%\" name=\"name2\" placeholder=\"" + redis_server_addr + "\"></br>");
  rootPage.concat(F("<label for=\"name3\">Redis port:  </label>"));
  rootPage.concat("<input type=\"text\" style=\"width:70%\" name=\"name3\" placeholder=\"" + String(redis_server_port) + "\"></br>");
  rootPage.concat(F("<label for=\"name4\">Redis pass:  </label>"));
  rootPage.concat("<input type=\"password\" style=\"width:70%\" name=\"name4\" placeholder=\"" + redis_server_pass + "\"></br>");
  rootPage.concat(F("<input type=\"submit\">"));
  rootPage.concat(F("</form></div>"));
  rootPage.concat(F("</body></html>"));
  // Root web page
  server.send(200, "text/html", rootPage);
}
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
void handleConfig(void) {
  String configPage;
  configPage = F("<!DOCTYPE html>");
  configPage.concat(F("<html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"));
  configPage.concat(F("</head><body><style>"));
  configPage.concat(F("body{margin:0;font-family:Arial,Helvetica,sans-serif}.sidenav{height:100%;width:180px;position:fixed;z-index:1;top:0;left:0;background-color:maroon;overflow-x:hidden}.sidenav a{color:#fff;padding:16px;text-decoration:none;display:block}.sidenav a:hover{background-color:#fcc;color:maroon}.content{margin-left:180px;padding:20px}"));
  configPage.concat(F("</style>"));
  configPage.concat(F("<div class=\"sidenav\"><a href=\"/\">Home</a><a href=\"/update\">OTA</a></div>"));
  configPage.concat(F("<div class=\"content\">"));
  configPage.concat(F("<h2><span style=\"color: maroon\">I</span>ED</h2>"));
  configPage.concat(F("<div>config ok</div></div>"));
  configPage.concat(F("</body></html>"));
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
    debugD("config ok");
  } else {
    server.send(200, "text/plain", "config error");
    debugE("config error");
  }
}

void clickbutton_action(void) {
  if (S31_Button.isSingleClick()) {
    if (digitalRead(RELAY_PIN)) {
      debugW("status on\n");
    } else {
      debugW("status off\n");
    }
    debugI("redis_deviceKey: %s", redis_deviceKey.c_str());
    debugI("redis_server_addr: %s", redis_server_addr.c_str());
    debugI("redis_server_port: %d", redis_server_port);
    debugI("redis_server_pass: %s", redis_server_pass.c_str());
    blue_led.setPatternSingle(init_pattern, 2);

    File configFile = SPIFFS.open("/config.txt", "r");
    if (configFile)
    {
      while (configFile.available())
      {
        //read line by line from the file
        String line = configFile.readStringUntil('\n');
        debugI("%s", line.c_str());
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
#if USE_WiFiManager
    wm.resetSettings();
    ESP.restart();
#endif
  }
}
void PowerSensorDisplay(void) {
  debugV("Voltage %.4f V\n", myCSE7766.getVoltage());
  debugV("Current %.4f A\n", myCSE7766.getCurrent());
  debugV("ActivePower %.4f W\n", myCSE7766.getActivePower());
  debugV("ApparentPower %.4f VA\n", myCSE7766.getApparentPower());
  debugV("ReactivePower %.4f VAR\n", myCSE7766.getReactivePower());
  debugV("PowerFactor %.4f %%\n", myCSE7766.getPowerFactor());
  debugV("Energy %.4f Ws\n", myCSE7766.getEnergy());
}

void redisInterface_handle(void) {
  String redis_key;
  String cse7766_value;
  String redis_str_result;
  bool redis_bool_result;

  if (redisInterface_flag == true) {
    if (redisInterface_state == 0) {
      if (!redisConn.connect(redis_server_addr.c_str(), redis_server_port))
      {
        debugE("Failed to connect to the Redis server!");
        redisInterface_state = 0;
        redisInterface_flag = false;
        blue_led.setPatternSingle(error_pattern, 6);
        return;
      }
      redisInterface_state++;
    } else if (redisInterface_state == 1) {
      Redis redis(redisConn);
      auto connRet = redis.authenticate(redis_server_pass.c_str());
      if (connRet == RedisSuccess)
      {
        debugD("Connected to the Redis server!");
        blue_led.setPatternSingle(normal_pattern, 4);
      } else {
        debugE("Failed to authenticate to the Redis server! Errno: %d\n", (int)connRet);
        redisInterface_state = 0;
        redisInterface_flag = false;
        redisConn.stop();
        blue_led.setPatternSingle(error_pattern, 6);
        return;
      }

      // Voltage
      redis_key = redis_deviceKey + String(redis_voltage);
      cse7766_value = String(myCSE7766.getVoltage());
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      // Current
      redis_key = redis_deviceKey + String(redis_current);
      cse7766_value = String(myCSE7766.getCurrent());
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      // ActivePower
      redis_key = redis_deviceKey + String(redis_activepower);
      cse7766_value = String(myCSE7766.getActivePower());
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      // ApparentPower
      redis_key = redis_deviceKey + String(redis_apparentpower);
      cse7766_value = String(myCSE7766.getApparentPower());
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      redisInterface_state++;
    } else if (redisInterface_state == 2) {
      Redis redis(redisConn);
      auto connRet = redis.authenticate(redis_server_pass.c_str());
      if (connRet == RedisSuccess)
      {
        debugD("Connected to the Redis server!");
      } else {
        debugE("Failed to authenticate to the Redis server! Errno: %d\n", (int)connRet);
        redisInterface_state = 0;
        redisInterface_flag = false;
        redisConn.stop();
        return;
      }

      // ReactivePower
      redis_key = redis_deviceKey + String(redis_reactivepower);
      cse7766_value = String(myCSE7766.getReactivePower());
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      // PowerFactor
      redis_key = redis_deviceKey + String(redis_powerfactor);
      cse7766_value = String(myCSE7766.getPowerFactor());
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      // Energy
      redis_key = redis_deviceKey + String(redis_energy);
      cse7766_value = String(myCSE7766.getEnergy());
      debugD("SET %s %s: ", redis_key.c_str(), cse7766_value.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), cse7766_value.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      // TimeStamp
      redis_key = redis_deviceKey + String(redis_timestamp);
      String timeStamp = timeClient.getFormattedTime();
      debugD("SET %s %s: ", redis_key.c_str(), timeStamp.c_str());
      redis_bool_result = redis.set(redis_key.c_str(), timeStamp.c_str());
      if (redis_bool_result) {
        debugD("ok!");
      } else {
        debugE("err");
      }

      redis_str_result = redis.get(redis_key.c_str());
      debugD("GET %s: %s", redis_key.c_str(), redis_str_result.c_str());

      redisInterface_state++;
    } else if (redisInterface_state == 3) {
      redisConn.stop();
      debugD("Connection closed!");

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
