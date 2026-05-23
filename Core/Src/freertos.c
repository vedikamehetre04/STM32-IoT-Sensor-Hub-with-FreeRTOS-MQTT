#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "dht11.h"
#include "esp8266.h"
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * FreeRTOS Task Definitions
 *
 * Task 1: Task_DHT11     — Priority 3 (Highest) — Reads sensor every 2s
 * Task 2: Task_UARTLog   — Priority 2 (Medium)  — Logs data via USART1
 * Task 3: Task_MQTT      — Priority 1 (Lowest)  — Publishes via ESP8266
 * ----------------------------------------------------------------------- */

/* ---- Wi-Fi & MQTT Configuration — UPDATE THESE ---- */
#define WIFI_SSID        "YourWiFiSSID"
#define WIFI_PASSWORD    "YourWiFiPassword"
/* ---------------------------------------------------- */

/* External UART handle (Debug/Log UART — USART1: PA9 TX, PA10 RX) */
extern UART_HandleTypeDef huart1;

/* -----------------------------------------------------------------------
 * Shared Data Types
 * ----------------------------------------------------------------------- */
typedef struct {
    float temperature;
    float humidity;
} SensorData_t;

/* -----------------------------------------------------------------------
 * FreeRTOS Objects
 * ----------------------------------------------------------------------- */
static QueueHandle_t     xSensorQueue;   /* Holds latest SensorData_t     */
static SemaphoreHandle_t xUARTMutex;    /* Protects shared USART1 access  */

/* Task handles */
static TaskHandle_t xDHT11TaskHandle;
static TaskHandle_t xUARTLogTaskHandle;
static TaskHandle_t xMQTTTaskHandle;

/* -----------------------------------------------------------------------
 * Helper: Thread-safe UART print
 * ----------------------------------------------------------------------- */
static void UART_Log(const char *msg)
{
    if (xSemaphoreTake(xUARTMutex, pdMS_TO_TICKS(500)) == pdTRUE)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 1000);
        xSemaphoreGive(xUARTMutex);
    }
}

/* -----------------------------------------------------------------------
 * TASK 1: DHT11 Sensor Reading
 * Priority  : 3 (Highest — sensor data is the source of truth)
 * Period    : 2000 ms
 * Behaviour : Reads DHT11 every 2 s. On valid read, overwrites the queue
 *             so downstream tasks always see the freshest value.
 * ----------------------------------------------------------------------- */
void Task_DHT11(void *pvParameters)
{
    DHT11_Data_t  rawData;
    SensorData_t  queueData;
    char          logBuf[80];

    DHT11_Init();
    UART_Log("[DHT11] Task started\r\n");

    for (;;)
    {
        rawData = DHT11_Read();

        if (rawData.Status == DHT11_OK)
        {
            queueData.temperature = rawData.Temperature;
            queueData.humidity    = rawData.Humidity;

            /* Overwrite — always hold only the latest reading */
            xQueueOverwrite(xSensorQueue, &queueData);

            snprintf(logBuf, sizeof(logBuf),
                     "[DHT11] Read OK — Temp: %.1f C, Hum: %.1f%%\r\n",
                     queueData.temperature, queueData.humidity);
            UART_Log(logBuf);
        }
        else
        {
            UART_Log("[DHT11] Read ERROR — retrying next cycle\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* -----------------------------------------------------------------------
 * TASK 2: UART Logger
 * Priority  : 2 (Medium)
 * Period    : 2000 ms
 * Behaviour : Peeks at the shared queue (non-destructive) and prints
 *             formatted sensor data to USART1 terminal.
 * ----------------------------------------------------------------------- */
void Task_UARTLog(void *pvParameters)
{
    SensorData_t data;
    char         logBuf[100];

    UART_Log("[UART] Logger task started\r\n");
    UART_Log("------------------------------------------\r\n");
    UART_Log("  STM32 IoT Sensor Hub — FreeRTOS & MQTT  \r\n");
    UART_Log("------------------------------------------\r\n");

    for (;;)
    {
        if (xQueuePeek(xSensorQueue, &data, pdMS_TO_TICKS(3000)) == pdTRUE)
        {
            snprintf(logBuf, sizeof(logBuf),
                     "[LOG]  Temperature : %.1f C\r\n"
                     "[LOG]  Humidity    : %.1f %%\r\n"
                     "-----------------------------\r\n",
                     data.temperature, data.humidity);
            UART_Log(logBuf);
        }
        else
        {
            UART_Log("[LOG]  Waiting for sensor data...\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* -----------------------------------------------------------------------
 * TASK 3: MQTT Publisher
 * Priority  : 1 (Lowest — network I/O is slow, yield to sensor tasks)
 * Period    : 5000 ms (publish interval)
 * Behaviour : Initializes ESP8266, connects to Wi-Fi and MQTT broker,
 *             then publishes sensor JSON every 5 seconds. Reconnects
 *             automatically if the broker connection drops.
 * ----------------------------------------------------------------------- */
void Task_MQTT(void *pvParameters)
{
    SensorData_t data;
    char         payload[120];
    bool         mqttConnected = false;

    UART_Log("[MQTT] Task started — initializing ESP8266...\r\n");

    /* --- Connect to Wi-Fi (retry until success) --- */
    while (!ESP8266_Init(WIFI_SSID, WIFI_PASSWORD))
    {
        UART_Log("[MQTT] Wi-Fi connection failed — retrying in 5s...\r\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    UART_Log("[MQTT] Wi-Fi connected!\r\n");

    /* --- Connect to MQTT Broker (retry until success) --- */
    while (!ESP8266_MQTT_Connect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID))
    {
        UART_Log("[MQTT] Broker connection failed — retrying in 5s...\r\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    UART_Log("[MQTT] Connected to broker: " MQTT_BROKER "\r\n");
    UART_Log("[MQTT] Publishing on topic: " MQTT_TOPIC "\r\n");
    mqttConnected = true;

    for (;;)
    {
        if (mqttConnected &&
            xQueuePeek(xSensorQueue, &data, pdMS_TO_TICKS(3000)) == pdTRUE)
        {
            /* Build JSON payload with escaped quotes for AT command */
            snprintf(payload, sizeof(payload),
                     "{\\\"temperature\\\":%.1f,\\\"humidity\\\":%.1f}",
                     data.temperature, data.humidity);

            if (ESP8266_MQTT_Publish(MQTT_TOPIC, payload))
            {
                UART_Log("[MQTT] Published: " MQTT_TOPIC "\r\n");
            }
            else
            {
                UART_Log("[MQTT] Publish failed — reconnecting to broker...\r\n");
                mqttConnected = ESP8266_MQTT_Connect(
                                    MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* -----------------------------------------------------------------------
 * FreeRTOS Initialization — called from main() before vTaskStartScheduler
 * ----------------------------------------------------------------------- */
void MX_FREERTOS_Init(void)
{
    /* Create shared queue — capacity 1, always holds the latest reading */
    xSensorQueue = xQueueCreate(1, sizeof(SensorData_t));
    configASSERT(xSensorQueue != NULL);

    /* Create UART mutex */
    xUARTMutex = xSemaphoreCreateMutex();
    configASSERT(xUARTMutex != NULL);

    /* Create tasks
     * Stack sizes are in words (4 bytes each on Cortex-M3)
     * MQTT task gets more stack for sprintf + AT command buffers
     */
    xTaskCreate(Task_DHT11,   "DHT11_Task",    256, NULL, 3, &xDHT11TaskHandle);
    xTaskCreate(Task_UARTLog, "UART_Log_Task", 256, NULL, 2, &xUARTLogTaskHandle);
    xTaskCreate(Task_MQTT,    "MQTT_Task",     512, NULL, 1, &xMQTTTaskHandle);
}
