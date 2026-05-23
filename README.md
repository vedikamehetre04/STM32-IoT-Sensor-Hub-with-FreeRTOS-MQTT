# STM32 IoT Sensor Hub with FreeRTOS & MQTT

> Firmware developed on STM32F103 (ARM Cortex-M3) using FreeRTOS — 3 concurrent tasks for DHT11 sensor reading, UART logging, and real-time MQTT data transmission to a Node-RED dashboard via ESP8266.

---

## Overview

This project implements a real-time IoT sensor hub using the STM32F103C8T6 (Blue Pill) microcontroller. It uses FreeRTOS to run three independent concurrent tasks simultaneously — reading temperature and humidity from a DHT11 sensor, logging data over UART, and publishing sensor values to an MQTT broker via an ESP8266 Wi-Fi module. The live data is visualized on a Node-RED dashboard.

---

## Hardware Requirements

| Qty | Component |
|-----|-----------|
| x1 | STM32F103C8T6 (Blue Pill) |
| x1 | DHT11 Temperature & Humidity Sensor |
| x1 | ESP8266 Wi-Fi Module (ESP-01 or NodeMCU) |
| x1 | USB-to-TTL Serial Adapter (for flashing & debug) |
| x1 | 3.3V Voltage Regulator (AMS1117) |
| x1 | Breadboard |
| ~20 | Male-Male Jumper Wires |
| x1 | Micro USB Cable |
| x1 | 5V Power Supply / Power Bank |

---

## Pin Configuration

| STM32 Pin | Function | Connected To |
|-----------|----------|--------------|
| PA1 | GPIO (Bit-bang) | DHT11 Data Pin |
| PA9 | USART1 TX | USB-TTL RX (Debug Log) |
| PA10 | USART1 RX | USB-TTL TX (Debug Log) |
| PA2 | USART2 TX | ESP8266 RX |
| PA3 | USART2 RX | ESP8266 TX |
| 3.3V | Power | DHT11 VCC, ESP8266 VCC |
| GND | Ground | All GND pins |

---

## Software Architecture

### FreeRTOS Tasks

The firmware runs **3 concurrent FreeRTOS tasks** with different priorities:

```
┌─────────────────────────────────────────────────────┐
│                  FreeRTOS Scheduler                 │
│                                                     │
│  Task 1 (Priority 3)   Task 2 (Priority 2)          │
│  ┌─────────────────┐   ┌─────────────────┐          │
│  │  DHT11 Sensor   │   │  UART Logger    │          │
│  │  Reads every 2s │   │  Logs every 2s  │          │
│  └────────┬────────┘   └────────┬────────┘          │
│           │ xQueueOverwrite      │ xQueuePeek        │
│           └──────────┬──────────┘                   │
│                ┌─────▼─────┐                        │
│                │  FreeRTOS │                        │
│                │   Queue   │                        │
│                └─────┬─────┘                        │
│           ┌──────────┘                              │
│  Task 3 (Priority 1)                                │
│  ┌─────────────────┐                                │
│  │  MQTT Publisher │                                │
│  │  Sends every 5s │                                │
│  └─────────────────┘                                │
└─────────────────────────────────────────────────────┘
```

### Task Descriptions

**Task 1 — DHT11 Sensor Reading (Highest Priority)**
Reads temperature and humidity from the DHT11 sensor using a bit-banging GPIO protocol every 2 seconds. Valid data is pushed into a FreeRTOS queue shared with other tasks.

**Task 2 — UART Logging (Medium Priority)**
Reads from the shared queue and prints formatted sensor data to the serial terminal via USART1 at 115200 baud. Protected by a mutex to prevent UART conflicts.

**Task 3 — MQTT Transmission (Lowest Priority)**
Connects to a Wi-Fi network via the ESP8266 using AT commands. Once connected to the MQTT broker (HiveMQ public broker), it reads from the queue every 5 seconds and publishes sensor data as a JSON payload to the configured topic.

---

## MQTT Data Format

**Topic:** `stm32/sensors`

**Payload (JSON):**
```json
{
  "temperature": 28.5,
  "humidity": 65.0
}
```

---

## Node-RED Dashboard

1. Install Node-RED: `npm install -g node-red`
2. Install the MQTT palette and Dashboard palette
3. Add an MQTT-in node with broker: `broker.hivemq.com:1883`
4. Subscribe to topic: `stm32/sensors`
5. Parse the JSON payload and connect to gauge/chart nodes

---

## FreeRTOS Concepts Used

| Concept | Used For |
|---------|----------|
| `xTaskCreate` | Create all 3 tasks |
| `xQueueCreate` | Shared sensor data between tasks |
| `xQueueOverwrite` | DHT11 always updates latest reading |
| `xQueuePeek` | UART and MQTT read without consuming |
| `xSemaphoreCreateMutex` | Protect shared UART resource |
| `vTaskDelay` | Non-blocking task delays |

---

## How to Build & Flash

1. Open **STM32CubeIDE** and create a new project for STM32F103C8T6
2. Enable FreeRTOS (CMSIS-RTOS v1) from the middleware section in CubeMX
3. Enable USART1 and USART2 in Asynchronous mode at 115200 baud
4. Copy the source files from `Core/Src/` and headers from `Core/Inc/`
5. Update `WIFI_SSID` and `WIFI_PASSWORD` in `freertos.c`
6. Build and flash using ST-Link or a USB-TTL adapter via STM32CubeProgrammer

---

## Important Notes

- The ESP8266 must have AT firmware v2.x or later to support MQTT AT commands
- The DHT11 uses a 1-wire bit-banging protocol — do not use it from an ISR
- All tasks share a single-element queue using `xQueueOverwrite` to always hold the latest sensor reading
- A UART mutex ensures Task 2 and Task 3 do not simultaneously write to USART1
- Adjust `DHT11_PORT` and `DHT11_PIN` in `dht11.h` if using a different GPIO pin

---

## Project Structure

```
STM32-IoT-Sensor-Hub/
├── Core/
│   ├── Inc/
│   │   ├── dht11.h
│   │   └── esp8266.h
│   └── Src/
│       ├── main.c
│       ├── freertos.c
│       ├── dht11.c
│       └── esp8266.c
└── README.md
```

---

## Author

Vedika Mehetre — Electronics & Communication Engineering
