/******************************** Config page *********************************
This file is part of the Ewings Esp8266 Stack.

This is free software. you can redistribute it and/or modify it but without any
warranty.

Author          : Suraj I.
created Date    : 1st June 2019
******************************************************************************/
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "Common.h"
#include "GlobalConfig.h"
#include "WifiConfig.h"
#include "ServerConfig.h"
#include "OtaConfig.h"
#include "EventConfig.h"
#include "NetworkConfig.h"

#ifdef ENABLE_MQTT_SERVICE
#include "MqttConfig.h"
#endif

#ifdef ENABLE_ESP_NOW
#include "EspnowConfig.h"
#endif

#ifdef ENABLE_GPIO_SERVICE
#include "GpioConfig.h"
#endif

#ifdef ENABLE_EMAIL_SERVICE
#include "EmailConfig.h"
#endif

// #define GLOBAL_CONFIG_TABLE_ADDRESS CONFIG_START
// #define LOGIN_CREDENTIAL_TABLE_ADDRESS GLOBAL_CONFIG_TABLE_ADDRESS +  global_config_size
// #define WIFI_CONFIG_TABLE_ADDRESS LOGIN_CREDENTIAL_TABLE_ADDRESS + login_credential_size
// #define OTA_CONFIG_TABLE_ADDRESS WIFI_CONFIG_TABLE_ADDRESS + wifi_config_size
// #ifdef ENABLE_GPIO_SERVICE
//     #define GPIO_CONFIG_TABLE_ADDRESS OTA_CONFIG_TABLE_ADDRESS + ota_config_size
// #endif
// #ifdef ENABLE_MQTT_SERVICE
//     #define MQTT_GENERAL_CONFIG_TABLE_ADDRESS GPIO_CONFIG_TABLE_ADDRESS + gpio_config_size
//     #define MQTT_LWT_CONFIG_TABLE_ADDRESS MQTT_GENERAL_CONFIG_TABLE_ADDRESS + mqtt_general_config_size
//     #define MQTT_PUBSUB_CONFIG_TABLE_ADDRESS MQTT_LWT_CONFIG_TABLE_ADDRESS + mqtt_lwt_config_size
// #endif

/**
 * table addresses in ewings database.
 */
enum eeprom_db_table_address {
  GLOBAL_CONFIG_TABLE_ADDRESS = CONFIG_START,
  LOGIN_CREDENTIAL_TABLE_ADDRESS = 50,
  WIFI_CONFIG_TABLE_ADDRESS = 150,
  OTA_CONFIG_TABLE_ADDRESS = 300,
  #ifdef ENABLE_GPIO_SERVICE
  GPIO_CONFIG_TABLE_ADDRESS = 500,
  #endif
  #ifdef ENABLE_MQTT_SERVICE
  MQTT_GENERAL_CONFIG_TABLE_ADDRESS = 700,
  MQTT_LWT_CONFIG_TABLE_ADDRESS = 1400,
  MQTT_PUBSUB_CONFIG_TABLE_ADDRESS = 1600,
  #endif
  #ifdef ENABLE_EMAIL_SERVICE
  EMAIL_CONFIG_TABLE_ADDRESS = 1900,
  #endif
};

#endif
