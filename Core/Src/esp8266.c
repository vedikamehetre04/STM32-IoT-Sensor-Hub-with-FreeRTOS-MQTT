#include "esp8266.h"
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * ESP8266 Driver — AT Command Interface over USART2
 * Baud Rate : 115200
 * TX: PA2  RX: PA3
 * Requires ESP8266 AT Firmware v2.x or later (supports MQTT AT commands)
 * ----------------------------------------------------------------------- */

static uint8_t rxBuffer[ESP_RX_BUF_SIZE];

/**
 * @brief  Send an AT command and check for an expected response string
 * @param  cmd      AT command string to transmit
 * @param  expected Substring to look for in the response
 * @param  timeout  Receive timeout in milliseconds
 * @retval true if expected string was found, false otherwise
 */
static bool ESP_SendCommand(const char *cmd, const char *expected, uint32_t timeout)
{
    memset(rxBuffer, 0, sizeof(rxBuffer));

    HAL_UART_Transmit(&ESP_UART, (uint8_t *)cmd, strlen(cmd), 1000);
    HAL_UART_Receive(&ESP_UART, rxBuffer, sizeof(rxBuffer) - 1, timeout);

    return (strstr((char *)rxBuffer, expected) != NULL);
}

/**
 * @brief  Initialize ESP8266 and connect to Wi-Fi network
 * @param  ssid      Wi-Fi network name
 * @param  password  Wi-Fi password
 * @retval true on success, false on failure
 */
bool ESP8266_Init(const char *ssid, const char *password)
{
    char cmd[160];

    /* Test communication */
    if (!ESP_SendCommand("AT\r\n", "OK", 1000))
        return false;

    /* Software reset */
    ESP_SendCommand("AT+RST\r\n", "ready", 3000);
    HAL_Delay(1500);

    /* Set station mode (client, not AP) */
    if (!ESP_SendCommand("AT+CWMODE=1\r\n", "OK", 2000))
        return false;

    /* Disable auto-connect to prevent stale connections */
    ESP_SendCommand("AT+CWAUTOCONN=0\r\n", "OK", 1000);

    /* Connect to Wi-Fi */
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    if (!ESP_SendCommand(cmd, "WIFI GOT IP", 15000))
        return false;

    return true;
}

/**
 * @brief  Connect to MQTT broker using AT+MQTT commands (ESP AT v2.x)
 * @param  broker    Broker hostname or IP address
 * @param  port      Broker port (typically 1883)
 * @param  clientId  Unique MQTT client identifier
 * @retval true on success, false on failure
 */
bool ESP8266_MQTT_Connect(const char *broker, uint16_t port, const char *clientId)
{
    char cmd[256];

    /* Configure MQTT user parameters:
     * AT+MQTTUSERCFG=<LinkID>,<scheme>,<client_id>,<username>,<password>,<cert_key_ID>,<CA_ID>,<path>
     * scheme=1 = plain MQTT (no TLS)
     */
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTUSERCFG=0,1,\"%s\",\"\",\"\",0,0,\"\"\r\n",
             clientId);
    if (!ESP_SendCommand(cmd, "OK", 3000))
        return false;

    /* Connect to broker:
     * AT+MQTTCONN=<LinkID>,<host>,<port>,<reconnect>
     */
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTCONN=0,\"%s\",%d,1\r\n",
             broker, port);
    if (!ESP_SendCommand(cmd, "OK", 8000))
        return false;

    return true;
}

/**
 * @brief  Publish a message to an MQTT topic
 * @param  topic    MQTT topic string
 * @param  payload  Message payload string
 * @retval true on success, false on failure
 *
 * @note   Payload must NOT contain unescaped double quotes.
 *         JSON values use escaped quotes: {\"key\":\"value\"}
 */
bool ESP8266_MQTT_Publish(const char *topic, const char *payload)
{
    char cmd[300];

    /* AT+MQTTPUB=<LinkID>,<topic>,<data>,<qos>,<retain> */
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n",
             topic, payload);

    return ESP_SendCommand(cmd, "OK", 3000);
}
