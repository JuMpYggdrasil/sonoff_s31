#define HOST_NAME "s31_1"

#ifndef STASSID
#define STASSID "JUMP"
#define STAPSK  "025260652"
#endif

#define REDIS_ADDR      "siriprapawat.trueddns.com"//"192.168.1.22"
#define REDIS_PORT      14285//6379
#define REDIS_PASSWORD  "61850"

#define REDIS_EEPROM_ADDR_BEGIN  0
#define REDIS_EEPROM_ADDR_VOLTAGE  100
#define REDIS_EEPROM_ADDR_CURRENT  120
#define REDIS_EEPROM_ADDR_ACTIVEPOWER  140
#define REDIS_EEPROM_ADDR_APPARENTPOWER  160
#define REDIS_EEPROM_ADDR_REACTIVEPOWER  180
#define REDIS_EEPROM_ADDR_POWERFACTOR  200
#define REDIS_EEPROM_ADDR_ENERGY  220
#define REDIS_EEPROM_ADDR_TIMESTAMP  240

#define REDIS_DEVKEY "AY4_9_CONV_621MxAY4x500xTTK_LINE2xMx/MMXU1$MX$"//TotW$mag$f
#define REDIS_VOLTAGE "Volt$mag$f"
#define REDIS_CURRENT "Curr$mag$f"
#define REDIS_ACTIVEPOWER "TotW$mag$f"//P
#define REDIS_APPARENTPOWER "Va$mag$f"//S
#define REDIS_REACTIVEPOWER "Var$mag$f"//Q
#define REDIS_POWERFACTOR "Pf$mag$f"
#define REDIS_ENERGY "E$mag$f"
#define REDIS_TIMESTAMP "Time$mag$f"

/*
  EEPROM structure to store key of value
  addr
  000-099(100) key/name
  100-119 (20) Voltage
  120-139 (20) Current
  140-159 (20) ActivePower
  160-179 (20) ApparentPower
  180-199 (20) ReactivePower
  200-219 (20) PowerFactor
  220-239 (20) Energy
  240-259 (20) TimeStamp
  260-279 (20)
  280-511 (230)
*/

// Board especific libraries
#if defined ESP8266 || defined ESP32

// Use mDNS ? (comment this do disable it)
#define USE_MDNS true

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


// Includes of ESP8266
#include <ESP8266WiFi.h>
#include "CSE7766.h"
#include <PinButton.h>
#include <ESP8266WebServer.h>
#include <Redis.h>
#include <ElegantOTA.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#ifdef USE_MDNS
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#endif

#else
#error "The board must be ESP8266 or ESP32"
#endif // ESP

// Remote debug over WiFi - not recommended for production, only for development
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug

// SSID and password
const char* ssid = STASSID;
const char* password = STAPSK;

// WWW user and password
const char* www_username = "admin";
const char* www_password = "admin";


String redis_deviceKey = REDIS_DEVKEY;
const char* redis_voltage = REDIS_VOLTAGE;
const char* redis_current = REDIS_CURRENT;
const char* redis_activepower = REDIS_ACTIVEPOWER;
const char* redis_apparentpower = REDIS_APPARENTPOWER;
const char* redis_reactivepower = REDIS_REACTIVEPOWER;
const char* redis_powerfactor = REDIS_POWERFACTOR;
const char* redis_energy = REDIS_ENERGY;
const char* redis_timestamp = REDIS_TIMESTAMP;


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

WiFiClient redisConn;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.navy.mi.th", 25200);//GMT+7 =3600*7 =25200

RemoteDebug Debug;

//prototype declare
void clickbutton_action(void);
void PowerSensorDisplay(void);
void redisInterface_handle(void);

void handleRoot();
void handleNotFound();
void handleConfig();

void EEPROM_WriteString(char addr, String data);
String EEPROM_ReadString(char addr);

void setup() {
  // Initialize
  myCSE7766.setRX(1);
  myCSE7766.begin(); // will initialize serial to 4800 bps

  //pinMode(PUSHBUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  redis_deviceKey.reserve(80);

#if !USE_MDNS
  WiFi.config(local_IP, primaryDNS, gateway, subnet);
#endif

  // WiFi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Register host name in WiFi and mDNS
  String hostNameWifi = HOST_NAME;
  hostNameWifi.concat(".local");

#ifdef ESP8266 // Only for it
  WiFi.hostname(hostNameWifi);
#endif

#ifdef USE_MDNS  // Use the MDNS ?
  if (MDNS.begin(HOST_NAME)) {
    //Serial.print("* MDNS responder started. Hostname -> ");
    //Serial.println(HOST_NAME);
  }
  MDNS.addService("telnet", "tcp", 23);
#endif

  ////==== OTA section ====
  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/config", HTTP_POST, handleConfig);

  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  MDNS.addService("telnet", "tcp", 23);
  MDNS.addService("http", "tcp", 80);

  //// Initialize RemoteDebug
  Debug.begin(HOST_NAME); // Initialize the WiFi server
  Debug.setResetCmdEnabled(true); // Enable the reset command
  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors

  timeClient.begin();

  digitalWrite(RELAY_PIN, HIGH);

  // Debug levels
  debugA("* This is a message of debug level ANY");//always show
  debugV("* This is a message of debug level VERBOSE");
  debugD("* This is a message of debug level DEBUG");
  debugI("* This is a message of debug level INFO");
  debugW("* This is a message of debug level WARNING");
  debugE("* This is a message of debug level ERROR");

  EEPROM.begin(512);

  debugI("Connected to %s", ssid);
  debugI("IP address: %s", WiFi.localIP().toString().c_str());

  if (EEPROM.read(REDIS_EEPROM_ADDR_BEGIN) == 0) {
    EEPROM_WriteString(REDIS_EEPROM_ADDR_BEGIN, redis_deviceKey);
  } else {
    redis_deviceKey = EEPROM_ReadString(REDIS_EEPROM_ADDR_BEGIN);
  }

  debugI("redis_deviceKey: %s", redis_deviceKey.c_str());
}

void loop()
{
  // Each second
  if ((millis() - mLastTime) >= 1000) {
    // Time
    mLastTime = millis();
    mTimeSeconds++;

    // Blink the led
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));

    // Debug the time (verbose level)
    //debugV("* Time: %u seconds (VERBOSE)", mTimeSeconds);

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
  server.handleClient();
  MDNS.update();
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

  rootPage = "<html><div class=\"container\">";
  rootPage += "<div>Sonoff S31 using ESP8266</div>";
  rootPage += "<div>To upload \"http://" + WiFi.localIP().toString() + "/update\"</div></br>";
  rootPage += "<form action=\"/config\" method=\"POST\">";
  rootPage += "<label for=\"name1\">Device key:</label>";
  rootPage += "<input type=\"text\" style=\"width:60%\" name=\"name1\" placeholder=\"" + redis_deviceKey + "\"></br>";
  rootPage += "<input type=\"submit\">";
  rootPage += "</form></div></html>";
  // Root web page
  server.send(200, "text/html", rootPage);
}
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
void handleConfig(void) {
  //authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  if (server.hasArg("name1")) {
    EEPROM_WriteString(REDIS_EEPROM_ADDR_BEGIN, server.arg("name1"));
    server.send(200, "text/plain", "config ok");
    debugD("config ok");
  } else {
    server.send(200, "text/plain", "config error");
    debugE("config error");
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
  EEPROM.commit();
}
String EEPROM_ReadString(char addr)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len = 0;
  unsigned char k;
  k = EEPROM.read(addr);
  while (k != '\0' && len < 200) //Read until null character
  {
    k = EEPROM.read(addr + len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}
void clickbutton_action(void) {
  if (S31_Button.isSingleClick()) {
    if (digitalRead(RELAY_PIN)) {
      debugW("status on\n");
    } else {
      debugW("status off\n");
    }

    debugI("redis_deviceKey: %s", redis_deviceKey.c_str());
  }
  if (S31_Button.isDoubleClick()) {
    digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
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
      if (!redisConn.connect(REDIS_ADDR, REDIS_PORT))
      {
        debugE("Failed to connect to the Redis server!");
        redisInterface_state = 0;
        redisInterface_flag = false;
        return;
      }
      redisInterface_state++;
    } else if (redisInterface_state == 1) {
      Redis redis(redisConn);
      auto connRet = redis.authenticate(REDIS_PASSWORD);//RedisReturnValue connRet
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
      auto connRet = redis.authenticate(REDIS_PASSWORD);//RedisReturnValue connRet
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
