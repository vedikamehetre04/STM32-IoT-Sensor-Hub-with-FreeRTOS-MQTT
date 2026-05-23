#ifndef ESP8266_H
#define ESP8266_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* UART handle used for ESP8266 communication (USART2: PA2=TX, PA3=RX) */
extern UART_HandleTypeDef huart2;
#define ESP_UART          huart2

/* Receive buffer size */
#define ESP_RX_BUF_SIZE   512

/* MQTT Configuration */
#define MQTT_BROKER       "broker.hivemq.com"
#define MQTT_PORT         1883
#define MQTT_CLIENT_ID    "STM32SensorHub_01"
#define MQTT_TOPIC        "stm32/sensors"

/* Function Prototypes */
bool ESP8266_Init(const char *ssid, const char *password);
bool ESP8266_MQTT_Connect(const char *broker, uint16_t port, const char *clientId);
bool ESP8266_MQTT_Publish(const char *topic, const char *payload);

#endif /* ESP8266_H */
