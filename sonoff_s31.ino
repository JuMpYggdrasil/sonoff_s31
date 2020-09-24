#define HOST_NAME "s31_1"

#ifndef STASSID
#define STASSID "JUMP"
#define STAPSK  "025260652"
#endif

#define REDIS_ADDR      "siriprapawat.trueddns.com"//"192.168.1.22"
#define REDIS_PORT      14285//6379
#define REDIS_PASSWORD  "61850"
#define REDIS_DEVKEY    "AY4_9_CONV_621MxAY4x500xTTK_LINE2xMx/MMXU1$MX$TotW$mag$f"
#define REDIS_EEPROM_ADDRBEGIN  0
#define REDIS_EEPROM_ADDRLEN  100

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

#ifdef USE_MDNS
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#endif

#else
#error "The board must be ESP8266 or ESP32"
#endif // ESP

// Remote debug over WiFi - not recommended for production, only for development
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug

RemoteDebug Debug;

// SSID and password
const char* ssid = STASSID;
const char* password = STAPSK;

// WWW user and password
const char* www_username = "admin";
const char* www_password = "admin";

const char* redis_deviceKey = REDIS_DEVKEY;

#if !USE_MDNS
IPAddress local_IP(192, 168, 1, 17);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
#endif

// Time
uint32_t mLastTime = 0;
uint32_t mTimeSeconds = 0;

CSE7766 myCSE7766;
PinButton S31_Button(PUSHBUTTON_PIN);
ESP8266WebServer server(80);

//prototype declare
void clickbutton_action(void);
void PowerSensorDisplay(void);
void redisInterface(void);

void handleRoot();
void handleNotFound();
void handleConfig();

void setup() {
  // Initialize
  myCSE7766.setRX(1);
  myCSE7766.begin(); // will initialize serial to 4800 bps

  //pinMode(PUSHBUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

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

  digitalWrite(RELAY_PIN, HIGH);

  // Debug levels
  debugA("* This is a message of debug level ANY");//always show
  debugV("* This is a message of debug level VERBOSE");
  debugD("* This is a message of debug level DEBUG");
  debugI("* This is a message of debug level INFO");
  debugW("* This is a message of debug level WARNING");
  debugE("* This is a message of debug level ERROR");

  EEPROM.begin(512);
//  int len = EEPROM_write(address, "DSDI|123456789");
//  NodeSerial.println(EEPROM_read(address, len));

  debugI("Connected to %s", ssid);
  debugI("IP address: %s", WiFi.localIP().toString().c_str());


  redis_deviceKey = EEPROM_read(REDIS_EEPROM_ADDRBEGIN,REDIS_EEPROM_ADDRLEN).c_str();
  debugI("redis_deviceKey: %s",redis_deviceKey);
}

void loop()
{
  WiFiClient redisConn;

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
      redisInterface();
    }
  }
  clickbutton_action();

  myCSE7766.handle();// CSE7766 handle
  Debug.handle();// RemoteDebug handle
  S31_Button.update();
  server.handleClient();
  MDNS.update();
  // Give a time for ESP
  yield();
}

void handleRoot(void) {
  String rootPage = "<html><div>Sonoff S31 using ESP8266</div></br>\n";
  rootPage += "<div>To upload \"http://" + WiFi.localIP().toString() + "/update\"</div>";
  rootPage += "<form action=\"/config\" method=\"POST\">\n";
  rootPage += "<input type=\"text\" name=\"name1\" placeholder=\"1\"></br>\n";
  rootPage += "<input type=\"text\" name=\"name2\" placeholder=\"2\"></br>\n";
  rootPage += "<input type=\"text\" name=\"name3\" placeholder=\"3\"></br>\n";
  rootPage += "<input type=\"submit\">\n";
  rootPage += "</form></html>";

  //authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  // Root web page
  server.send(200, "text/html", rootPage);
}
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
void handleConfig(void) {
  //save data to eeprom
  //result.something = save(eeprom_addr,server.arg("name1"));
  for (int i = REDIS_EEPROM_ADDRBEGIN; i < (REDIS_EEPROM_ADDRBEGIN + REDIS_EEPROM_ADDRLEN-1); i++) { //clear EEPROM this section
    EEPROM.write(i, 0);
  }
  EEPROM_write(REDIS_EEPROM_ADDRBEGIN, server.arg("name1"));
  //if result.something config ok
  server.send(200, "text/plain", "config ok");
  //or not
  //server.send(200, "text/plain", "config error");
}

String EEPROM_read(int index, int length) {
  String text = "";
  char ch = 1;

  for (int i = index; (i < (index + length)) && ch; ++i) {
    if (ch = EEPROM.read(i)) {
      text.concat(ch);
    }
  }
  return text;
}

int EEPROM_write(int index, String text) {
  for (int i = index; i < text.length() + index; ++i) {
    EEPROM.write(i, text[i - index]);
  }
  EEPROM.write(index + text.length(), 0);
  EEPROM.commit();

  return text.length() + 1;
}

void clickbutton_action(void) {
  if (S31_Button.isSingleClick()) {
    if (digitalRead(RELAY_PIN)) {
      debugW("status on\n");
    } else {
      debugW("status off\n");
    }
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

void redisInterface(void) {
  WiFiClient redisConn;
  if (!redisConn.connect(REDIS_ADDR, REDIS_PORT))
  {
    debugD("Failed to connect to the Redis server!");
    return;
  }

  Redis redis(redisConn);
  auto connRet = redis.authenticate(REDIS_PASSWORD);
  if (connRet == RedisSuccess)
  {
    debugD("Connected to the Redis server!");
  }
  else
  {
    debugD("Failed to authenticate to the Redis server! Errno: %d\n", (int)connRet);
    return;
  }

  debugD("SET foo bar: ");
  bool redis_bool_result = redis.set(redis_deviceKey, "345.6");
  if (redis_bool_result) {
    debugD("ok!");
  } else {
    debugD("err!");
  }

  String redis_str_result = redis.get(redis_deviceKey);
  debugD("GET foo: %s", redis_str_result.c_str());

  redisConn.stop();
  debugD("Connection closed!");
}
