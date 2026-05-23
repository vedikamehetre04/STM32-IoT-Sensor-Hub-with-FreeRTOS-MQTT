#include "main.h"
#include "cmsis_os.h"

/* -----------------------------------------------------------------------
 * STM32F103C8T6 — Main Entry Point
 *
 * Peripheral Setup:
 *   USART1 (PA9/PA10) — 115200 baud — Debug / UART log
 *   USART2 (PA2/PA3)  — 115200 baud — ESP8266 Wi-Fi module
 *   GPIOA  (PA1)      — DHT11 sensor (bit-bang, configured in dht11.c)
 *
 * Clock: HSE 8 MHz × PLL × 9 = 72 MHz SYSCLK
 * ----------------------------------------------------------------------- */

/* Peripheral handles — shared with freertos.c and esp8266.c */
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* Declared in freertos.c */
extern void MX_FREERTOS_Init(void);

/* -----------------------------------------------------------------------
 * main()
 * ----------------------------------------------------------------------- */
int main(void)
{
    /* Reset all peripherals, initialize Flash and SysTick */
    HAL_Init();

    /* Configure system clock to 72 MHz */
    SystemClock_Config();

    /* Initialize peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* Create FreeRTOS objects and tasks */
    MX_FREERTOS_Init();

    /* Start the FreeRTOS scheduler — this call does not return */
    vTaskStartScheduler();

    /* Should never reach here */
    while (1) {}
}

/* -----------------------------------------------------------------------
 * System Clock: 72 MHz via HSE (8 MHz crystal) + PLL × 9
 * ----------------------------------------------------------------------- */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL         = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType           = RCC_CLOCKTYPE_HCLK   |
                                            RCC_CLOCKTYPE_SYSCLK |
                                            RCC_CLOCKTYPE_PCLK1  |
                                            RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource        = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider       = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider      = RCC_HCLK_DIV2;   /* APB1 max 36 MHz */
    RCC_ClkInitStruct.APB2CLKDivider      = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

/* -----------------------------------------------------------------------
 * USART1 — Debug Log (PA9=TX, PA10=RX) at 115200 baud
 * ----------------------------------------------------------------------- */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/* -----------------------------------------------------------------------
 * USART2 — ESP8266 (PA2=TX, PA3=RX) at 115200 baud
 * ----------------------------------------------------------------------- */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* -----------------------------------------------------------------------
 * GPIO Initialization — Enable clocks; PA1 configured later by DHT11_Init
 * ----------------------------------------------------------------------- */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}

/* -----------------------------------------------------------------------
 * HAL MSP: Configure GPIO alternate functions for USART1 and USART2
 * Called internally by HAL_UART_Init()
 * ----------------------------------------------------------------------- */
void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (uartHandle->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();

        /* PA9 — USART1 TX (AF Push-Pull) */
        GPIO_InitStruct.Pin   = GPIO_PIN_9;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* PA10 — USART1 RX (Input floating) */
        GPIO_InitStruct.Pin  = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
    else if (uartHandle->Instance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();

        /* PA2 — USART2 TX (AF Push-Pull) */
        GPIO_InitStruct.Pin   = GPIO_PIN_2;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* PA3 — USART2 RX (Input floating) */
        GPIO_InitStruct.Pin  = GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/* -----------------------------------------------------------------------
 * Error Handler — called by HAL on assertion failures
 * ----------------------------------------------------------------------- */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

/* -----------------------------------------------------------------------
 * FreeRTOS: Stack overflow hook
 * ----------------------------------------------------------------------- */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* Stack overflow detected — halt */
    Error_Handler();
}
