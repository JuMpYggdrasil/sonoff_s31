#define HOST_NAME "remotedebug_CSE7766_S31"

// Board especific libraries
#if defined ESP8266 || defined ESP32

// Use mDNS ? (comment this do disable it)
#define USE_MDNS true

// Arduino OTA (uncomment this to enable)
//#define USE_ARDUINO_OTA true

#else

// RemoteDebug library is now only to Espressif boards,
// as ESP32 and ESP82266,
// If need for another WiFi boards,
// please add an issue about this
// and we will see if it is possible made the port for your board.
// access: https://github.com/JoaoLopesF/RemoteDebug/issues

#error "The board must be ESP8266 or ESP32"

#endif // ESP

//////// Libraries
//// Hardware config
/// GPIOs
#define PUSHBUTTON_PIN   0
#define RELAY_PIN       12//relay(active high) include led(active low)
#define LED_PIN         13
/// UART0
//#define esp8266_TX_PIN 1//connect GPIO_CSE7766_RX PIN8(RI)
//#define esp8266_RX_PIN 3//connect GPIO_CSE7766_TX PIN6(TI)
/// i2c
//#define SDA 4//GPIO4 as D RX
//#define SCL 5//GPIO5 as D TX

#if defined ESP8266

// Includes of ESP8266
#include <ESP8266WiFi.h>
#include "CSE7766.h"
#include <PinButton.h>

#ifdef USE_MDNS
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#endif

#elif defined ESP32

// Includes of ESP32
#include <WiFi.h>

#ifdef USE_MDNS
#include <DNSServer.h>
#include "ESPmDNS.h"
#endif

#endif // ESP

// Remote debug over WiFi - not recommended for production, only for development
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug

RemoteDebug Debug;

// SSID and password
const char* ssid = "JUMP";
const char* password = "025260652";

IPAddress local_IP(192, 168, 1, 17);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);

// Time
uint32_t mLastTime = 0;
uint32_t mTimeSeconds = 0;

CSE7766 myCSE7766;
PinButton S31_Button(PUSHBUTTON_PIN);

//prototype declare
void PowerSensorDisplay();

void setup() {
  // Initialize
  myCSE7766.setRX(1);
  myCSE7766.begin(); // will initialize serial to 4800 bps

  //pinMode(PUSHBUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.config(local_IP, primaryDNS, gateway, subnet);

  // WiFi connection
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

  //// Initialize RemoteDebug
  Debug.begin(HOST_NAME); // Initialize the WiFi server
  Debug.setResetCmdEnabled(true); // Enable the reset command
  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors


  digitalWrite(RELAY_PIN, HIGH);

  // Debug levels
  debugV("* This is a message of debug level VERBOSE");
  debugD("* This is a message of debug level DEBUG");
  debugI("* This is a message of debug level INFO");
  debugW("* This is a message of debug level WARNING");
  debugE("* This is a message of debug level ERROR");
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
    debugV("* Time: %u seconds (VERBOSE)", mTimeSeconds);

    if (mTimeSeconds % 5 == 0) { // Each 5 seconds
      PowerSensorDisplay();
    }
  }

  if (S31_Button.isDoubleClick()) {
    digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
  }
  if (S31_Button.isSingleClick()) {
    if(digitalRead(RELAY_PIN)){
      debugW("status on\n");
    }else{
      debugW("status off\n");
    }
  }
  myCSE7766.handle();// CSE7766 handle
  Debug.handle();// RemoteDebug handle
  S31_Button.update();

  // Give a time for ESP
  yield();
}

// Function example to show a new auto function name of debug* macros
void PowerSensorDisplay() {
  debugI("Voltage %.4f V\n", myCSE7766.getVoltage());
  debugI("Current %.4f A\n", myCSE7766.getCurrent());
  debugI("ActivePower %.4f W\n", myCSE7766.getActivePower());
  debugI("ApparentPower %.4f VA\n", myCSE7766.getApparentPower());
  debugI("ReactivePower %.4f VAR\n", myCSE7766.getReactivePower());
  debugI("PowerFactor %.4f \n", myCSE7766.getPowerFactor());
  debugI("Energy %.4f Ws\n", myCSE7766.getEnergy());
}
