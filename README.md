# sonoff_s31

## hardware
### detail
ESP8266 (controller)  
CSE7766 (power sensor)
### config
// GPIOs  
#define PUSHBUTTON_PIN   0  
#define RELAY_PIN       12//relay(active high) include led(active low)  
#define LED_PIN         13  
/// UART0  
//#define esp8266_TX_PIN 1//connect GPIO_CSE7766_RX PIN8(RI)  
//#define esp8266_RX_PIN 3//connect GPIO_CSE7766_TX PIN6(TI)  
/// i2c  
//#define SDA 4//GPIO4 as D RX  
//#define SCL 5//GPIO5 as D TX  


## software
### config
using arduino ide 1.8 -- ESP8266  
*To program press button before power on*
