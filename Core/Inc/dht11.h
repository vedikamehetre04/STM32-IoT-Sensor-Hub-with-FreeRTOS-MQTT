#ifndef DHT11_H
#define DHT11_H

#include "main.h"

/* DHT11 GPIO Configuration */
#define DHT11_PORT    GPIOA
#define DHT11_PIN     GPIO_PIN_1

/* DHT11 Status Codes */
#define DHT11_OK      0
#define DHT11_ERROR   1

/**
 * @brief Structure to hold DHT11 sensor readings
 */
typedef struct {
    float    Temperature;  /* Temperature in degrees Celsius */
    float    Humidity;     /* Relative humidity in percent   */
    uint8_t  Status;       /* DHT11_OK or DHT11_ERROR        */
} DHT11_Data_t;

/* Function Prototypes */
void         DHT11_Init(void);
DHT11_Data_t DHT11_Read(void);

#endif /* DHT11_H */
