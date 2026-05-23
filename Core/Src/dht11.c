#include "dht11.h"

/* -----------------------------------------------------------------------
 * DHT11 Driver — Bit-bang GPIO implementation for STM32F103
 * Data Pin: PA1 (configurable in dht11.h)
 * Protocol: Single-wire, 40-bit transfer (16-bit humidity + 16-bit temp + 8-bit checksum)
 * ----------------------------------------------------------------------- */

/**
 * @brief  Reconfigure DHT11 pin as Push-Pull Output
 */
static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DHT11_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/**
 * @brief  Reconfigure DHT11 pin as Floating Input
 */
static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/**
 * @brief  Microsecond delay using DWT Cycle Counter
 * @param  us: Number of microseconds to delay
 */
static void DHT11_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks);
}

/**
 * @brief  Initialize DHT11 — enables DWT counter for us-level timing
 */
void DHT11_Init(void)
{
    /* Enable DWT (Data Watchpoint and Trace) for cycle counting */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* Ensure GPIO clock is enabled */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Set pin high as idle state */
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
}

/**
 * @brief  Read temperature and humidity from DHT11 sensor
 * @retval DHT11_Data_t struct — check .Status for DHT11_OK or DHT11_ERROR
 */
DHT11_Data_t DHT11_Read(void)
{
    DHT11_Data_t result = {0.0f, 0.0f, DHT11_ERROR};
    uint8_t      raw[5] = {0};
    uint32_t     timeout;

    /* --- Step 1: Host sends START signal --- */
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(18);                                   /* Pull low >= 18 ms */
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_DelayUs(40);                               /* Pull high 20-40 us */

    /* --- Step 2: Switch to input and wait for DHT11 response --- */
    DHT11_SetInput();

    /* Wait for DHT11 to pull LOW (response signal) */
    timeout = 10000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
        if (--timeout == 0) return result;

    /* Wait for DHT11 to pull HIGH (80 us low) */
    timeout = 10000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
        if (--timeout == 0) return result;

    /* Wait for DHT11 to pull LOW (80 us high — ready to transmit) */
    timeout = 10000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
        if (--timeout == 0) return result;

    /* --- Step 3: Read 40 bits of data --- */
    for (int i = 0; i < 40; i++)
    {
        /* Each bit starts with a 50 us LOW */
        timeout = 10000;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
            if (--timeout == 0) return result;

        /* Sample after 40 us:
         *   If still HIGH → bit is '1' (26-28 us = 0, 70 us = 1)
         *   If already LOW → bit is '0'
         */
        DHT11_DelayUs(40);
        raw[i / 8] <<= 1;
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
            raw[i / 8] |= 1;

        /* Wait for HIGH to end */
        timeout = 10000;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
            if (--timeout == 0) return result;
    }

    /* --- Step 4: Verify checksum --- */
    if (raw[4] != ((raw[0] + raw[1] + raw[2] + raw[3]) & 0xFF))
        return result;  /* Checksum mismatch */

    /* --- Step 5: Parse data --- */
    result.Humidity    = (float)raw[0] + (float)raw[1] * 0.1f;
    result.Temperature = (float)raw[2] + (float)raw[3] * 0.1f;
    result.Status      = DHT11_OK;

    return result;
}
