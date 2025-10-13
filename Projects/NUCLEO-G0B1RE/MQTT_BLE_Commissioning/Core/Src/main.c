/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "spi_iface.h"
#include "mqtt.h"
#include "ble.h"

#if defined(__ICCARM__)
#include <LowLevelIOInterface.h>
#endif /* __ICCARM__ */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EVT_WIFI_CONNECT                              (1<<0)
#define EVT_WIFI_DISCONNECT                       (1<<1)
#define EVT_WIFI_CONNECT_ERROR                    (1<<2)
#define EVT_MQTT_RECEIVED         (1<<3)
#define EVT_TIM_UPDATE                            (1<<4)
#define EVT_BLE_WRITE                           (1<<5)
#define EVT_WIFI_SCAN_END                           (1<<6)
#define EVT_MQTT_CONNECT                           (1<<7)

#define SET_EVENT_BIT(bit)     (event_bits |= (bit))
#define CLEAR_EVENT_BIT(bit)   (event_bits &= ~(bit))
#define CHECK_EVENT_BIT(bit)   ((event_bits & (bit)) != 0)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t wifi_connected = 0;
volatile uint32_t event_bits = 0;
char *mqtt_data_received_ptr = NULL;
char *gatt_data_received_ptr = NULL;

uint8_t color_red = 0, color_green = 0, color_blue = 0;
uint8_t color_red_s = 0, color_green_s = 0, color_blue_s = 0;
float color_scale = 10;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */
static void report_callback(char *report, int32_t report_len);
void parse_mqtt_received_event(char *data);
int parse_rgb(const char *data, uint8_t *red, uint8_t *green, uint8_t *blue);
int parse_level(const char *data, uint8_t *level);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if defined(__ICCARM__)
size_t __write(int file, unsigned char const *ptr, size_t len)
{
	size_t idx;
	unsigned char const *pdata = ptr;
	/* USER CODE BEGIN __write_1 */

	/* USER CODE END __write_1 */

	HAL_UART_Transmit(&huart2, pdata, len, 10000);
	return len;
	/* USER CODE BEGIN __write_End */

	/* USER CODE END __write_End */
}
#else

#if defined ( __GNUC__) && !defined(__clang__)
/* With GCC, small printf (option LD Linker->Libraries->Small printf
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
	/* USER CODE BEGIN PUTCHAR_PROTOTYPE_1 */

	/* USER CODE END PUTCHAR_PROTOTYPE_1 */
	while (!LL_USART_IsActiveFlag_TXE(USART2)){}

	LL_USART_ClearFlag_TC(USART2);
	LL_USART_TransmitData8(USART2, ch);

	return ch;
	/* USER CODE BEGIN PUTCHAR_PROTOTYPE_End */

	/* USER CODE END PUTCHAR_PROTOTYPE_End */
}
#endif /* __ICCARM__ */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  /* PendSV_IRQn interrupt configuration */
  NVIC_SetPriority(PendSV_IRQn, 3);
  /* SysTick_IRQn interrupt configuration */
  NVIC_SetPriority(SysTick_IRQn, 3);

  /** Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
  */
  LL_SYSCFG_DisableDBATT(LL_SYSCFG_UCPD1_STROBE | LL_SYSCFG_UCPD2_STROBE);

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
	int32_t ret = 0;
	char *resp = NULL;

	ret = spi_iface_init(&report_callback);

	spi_iface_command("AT+PWR=0", &resp);
	free(resp);

	Wifi_Init();
	MQTT_Init();
	BLE_Init();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	char rssi_data[64] = {0};

	while (1)
	{
		if (CHECK_EVENT_BIT(EVT_WIFI_CONNECT))
		{
			Wifi_Connected();
			MQTT_Connect();
			CLEAR_EVENT_BIT(EVT_WIFI_CONNECT);
		}
		if (CHECK_EVENT_BIT(EVT_MQTT_CONNECT))
		{
			spi_iface_command("AT+MQTTSUB?", &resp);
			printf("%s\n",resp);
			free(resp);
			LL_TIM_ClearFlag_UPDATE(TIM1);
			LL_TIM_EnableIT_UPDATE(TIM1);
			LL_TIM_EnableCounter(TIM1);
			CLEAR_EVENT_BIT(EVT_MQTT_CONNECT);
		}
		if (CHECK_EVENT_BIT(EVT_MQTT_RECEIVED))
		{
			parse_mqtt_received_event(mqtt_data_received_ptr);

			color_red_s   = (uint8_t)(color_red   * color_scale);
			color_green_s = (uint8_t)(color_green * color_scale);
			color_blue_s  = (uint8_t)(color_blue  * color_scale);

			free(mqtt_data_received_ptr);
			mqtt_data_received_ptr = NULL;
			CLEAR_EVENT_BIT(EVT_MQTT_RECEIVED);
		}
		if (CHECK_EVENT_BIT(EVT_BLE_WRITE))
		{
			BLE_Handle_Write(gatt_data_received_ptr);

			free(gatt_data_received_ptr);
			gatt_data_received_ptr = NULL;
			CLEAR_EVENT_BIT(EVT_BLE_WRITE);
		}
		if (CHECK_EVENT_BIT(EVT_WIFI_SCAN_END))
		{
			Wifi_Scan_End();
			CLEAR_EVENT_BIT(EVT_WIFI_SCAN_END);
		}
		if (CHECK_EVENT_BIT(EVT_TIM_UPDATE) && wifi_connected)
		{
			int rssi;

			spi_iface_command("AT+CWJAP?", &resp);

			char *last = strrchr(resp, ',');
			char *second_last = NULL;
			*last = '\0';
			second_last = strrchr(resp, ',');
			*last = ','; // restore string

			rssi = atoi(second_last + 1);
			free(resp);

			snprintf(rssi_data, 64, MQTT_RSSI_MSG, rssi);
			MQTT_Publish(MQTT_TOPIC_RSSI, (const char *) &rssi_data);
			CLEAR_EVENT_BIT(EVT_TIM_UPDATE);
		}


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2)
  {
  }

  /* HSI configuration and activation */
  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1)
  {
  }

  /* Main PLL configuration and activation */
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_Enable();
  LL_RCC_PLL_EnableDomain_SYS();
  while(LL_RCC_PLL_IsReady() != 1)
  {
  }

  /* Set AHB prescaler*/
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

  /* Sysclk activation on the main PLL */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  }

  /* Set APB1 prescaler*/
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_Init1msTick(64000000);
  /* Update CMSIS variable (which can be updated also through SystemCoreClockUpdate function) */
  LL_SetSystemCoreClock(64000000);
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  LL_SPI_InitTypeDef SPI_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  /**SPI1 GPIO Configuration
  PA5   ------> SPI1_SCK
  PA6   ------> SPI1_MISO
  PA7   ------> SPI1_MOSI
  */
  GPIO_InitStruct.Pin = SPI_SCK_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(SPI_SCK_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SPI_MISO_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(SPI_MISO_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SPI_MOSI_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(SPI_MOSI_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV2;
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
  SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
  SPI_InitStruct.CRCPoly = 7;
  LL_SPI_Init(SPI1, &SPI_InitStruct);
  LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);
  LL_SPI_EnableNSSPulseMgt(SPI1);
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  LL_TIM_InitTypeDef TIM_InitStruct = {0};

  LL_RCC_SetTIMClockSource(LL_RCC_TIM1_CLKSOURCE_PCLK1);

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);

  /* TIM1 interrupt Init */
  NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 0);
  NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  TIM_InitStruct.Prescaler = 12799;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 49999;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_InitStruct.RepetitionCounter = 0;
  LL_TIM_Init(TIM1, &TIM_InitStruct);
  LL_TIM_EnableARRPreload(TIM1);
  LL_TIM_SetClockSource(TIM1, LL_TIM_CLOCKSOURCE_INTERNAL);
  LL_TIM_SetTriggerOutput(TIM1, LL_TIM_TRGO_RESET);
  LL_TIM_SetTriggerOutput2(TIM1, LL_TIM_TRGO2_RESET);
  LL_TIM_DisableMasterSlaveMode(TIM1);
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  /**USART2 GPIO Configuration
  PA2   ------> USART2_TX
  PA3   ------> USART2_RX
  */
  GPIO_InitStruct.Pin = USART2_TX_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(USART2_TX_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = USART2_RX_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(USART2_RX_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
  USART_InitStruct.BaudRate = 115200;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART2, &USART_InitStruct);
  LL_USART_SetTXFIFOThreshold(USART2, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_SetRXFIFOThreshold(USART2, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_DisableFIFO(USART2);
  LL_USART_ConfigAsyncMode(USART2);

  /* USER CODE BEGIN WKUPType USART2 */

  /* USER CODE END WKUPType USART2 */

  LL_USART_Enable(USART2);

  /* Polling USART2 initialisation */
  while((!(LL_USART_IsActiveFlag_TEACK(USART2))) || (!(LL_USART_IsActiveFlag_REACK(USART2))))
  {
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOF);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

  /**/
  LL_GPIO_ResetOutputPin(SPI_CS_GPIO_Port, SPI_CS_Pin);

  /**/
  LL_GPIO_ResetOutputPin(BOOT_GPIO_Port, BOOT_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CHIP_EN_GPIO_Port, CHIP_EN_Pin);

  /**/
  LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTC, LL_EXTI_CONFIG_LINE13);

  /**/
  LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTA, LL_EXTI_CONFIG_LINE9);

  /**/
  LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTB, LL_EXTI_CONFIG_LINE3);

  /**/
  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_13;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
  LL_EXTI_Init(&EXTI_InitStruct);

  /**/
  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_9;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_FALLING;
  LL_EXTI_Init(&EXTI_InitStruct);

  /**/
  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_3;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;
  LL_EXTI_Init(&EXTI_InitStruct);

  /**/
  LL_GPIO_SetPinPull(B1_GPIO_Port, B1_Pin, LL_GPIO_PULL_NO);

  /**/
  LL_GPIO_SetPinPull(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin, LL_GPIO_PULL_UP);

  /**/
  LL_GPIO_SetPinPull(SPI_RDY_GPIO_Port, SPI_RDY_Pin, LL_GPIO_PULL_NO);

  /**/
  LL_GPIO_SetPinMode(B1_GPIO_Port, B1_Pin, LL_GPIO_MODE_INPUT);

  /**/
  LL_GPIO_SetPinMode(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin, LL_GPIO_MODE_INPUT);

  /**/
  LL_GPIO_SetPinMode(SPI_RDY_GPIO_Port, SPI_RDY_Pin, LL_GPIO_MODE_INPUT);

  /**/
  GPIO_InitStruct.Pin = SPI_CS_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(SPI_CS_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = BOOT_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(BOOT_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CHIP_EN_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CHIP_EN_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  NVIC_SetPriority(EXTI2_3_IRQn, 3);
  NVIC_EnableIRQ(EXTI2_3_IRQn);
  NVIC_SetPriority(EXTI4_15_IRQn, 3);
  NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void report_callback(char *report, int32_t report_len)
{
	// ignore the 'busy' messages
	if (report_len == 14 && strncmp(report, "\r\nbusy p...\r\n", 14) == 0) {free(report); return;}

	printf("Report: %s", report);

	if (strncmp(report, "+CW:GOTIP\r\n", 12) == 0)
	{
		SET_EVENT_BIT(EVT_WIFI_CONNECT);
		wifi_connected = 1;
	}
	else if (strncmp(report, "+CW:DISCONNECTED", 16) == 0)
	{
		SET_EVENT_BIT(EVT_WIFI_DISCONNECT);
		wifi_connected = 0;
	}
	else if (strncmp(report, "+CWLAP", 6) == 0)
	{
		Wifi_Scan_Report(report);
	}
	else if (strncmp(report, "+CW:SCAN_DONE", 13) == 0)
	{
		SET_EVENT_BIT(EVT_WIFI_SCAN_END);
	}
	else if (strncmp(report, "+MQTT:SUBRECV", 13) == 0)
	{
		mqtt_data_received_ptr = calloc(report_len, 1);
		memcpy(mqtt_data_received_ptr, report, report_len);
		SET_EVENT_BIT(EVT_MQTT_RECEIVED);
	}
	else if (strncmp(report, "+MQTT:CONNECTED", 15) == 0)
	{
		SET_EVENT_BIT(EVT_MQTT_CONNECT);
	}
	else if (strncmp(report, "+BLE:CONNECTED:", 15) == 0)
	{
	}
	else if (strncmp(report, "+BLE:DISCONNECTED:", 18) == 0)
	{
	}
	else if (strncmp(report, "+BLE:GATTWRITE:", 15) == 0)
	{
		gatt_data_received_ptr = calloc(report_len, 1);
		memcpy(gatt_data_received_ptr, report, report_len);
		SET_EVENT_BIT(EVT_BLE_WRITE);
	}

	free(report);
}

int parse_rgb(const char *data, uint8_t *red, uint8_t *green, uint8_t *blue)
{
	int r = -1, g = -1, b = -1;
	const char *p;

	// Find "R"
	p = strstr(data, "\"R\"");
	if (p) {
		p = strchr(p, ':');
		if (p) r = atoi(p + 1);
	}

	// Find "G"
	p = strstr(data, "\"G\"");
	if (p) {
		p = strchr(p, ':');
		if (p) g = atoi(p + 1);
	}

	// Find "B"
	p = strstr(data, "\"B\"");
	if (p) {
		p = strchr(p, ':');
		if (p) b = atoi(p + 1);
	}

	if (r >= 0 && g >= 0 && b >= 0) {
		*red = (uint8_t)r;
		*green = (uint8_t)g;
		*blue = (uint8_t)b;
		return 1;
	}
	return 0;
}

/**
 * @brief Parse Level value from a string and assign to output variable.
 * @param data      Input string containing JSON with "Level" field.
 * @param level     Pointer to uint8_t for the Level value.
 * @return 1 if value found, 0 otherwise.
 */
int parse_level(const char *data, uint8_t *level)
{
	int lvl = -1;
	const char *p;

	// Find "Level"
	p = strstr(data, "\"Level\"");
	if (p) {
		p = strchr(p, ':');
		if (p) lvl = atoi(p + 1);
	}

	if (lvl >= 0) {
		*level = (uint8_t)lvl;
		return 1;
	}
	return 0;
}

void parse_mqtt_received_event(char *data)
{
	int is_rgb, is_lvl;
	uint8_t raw_red = 0, raw_green = 0, raw_blue = 0, raw_level = 0;

	is_rgb = parse_rgb(data, &raw_red, &raw_green, &raw_blue);
	is_lvl = parse_level(data, &raw_level);

	if (is_rgb ) {
		color_red   = raw_red;
		color_green = raw_green;
		color_blue  = raw_blue;
	} else if (is_lvl) {
		color_scale = raw_level / 100.0f;
	}
}

void GPIO_EXTI_Rising_Callback(uint16_t pin)
{
	/* USER CODE BEGIN EXTI_Rising_Callback_1 */

	/* USER CODE END EXTI_Rising_Callback_1 */
	/* Callback when data is available in Network CoProcessor to enable SPI Clock */
	if (pin == SPI_RDY_Pin)
	{
		spi_iface_ncp_ready_high();
	}
	/* USER CODE BEGIN EXTI_Rising_Callback_End */

	/* USER CODE END EXTI_Rising_Callback_End */
}

void GPIO_EXTI_Falling_Callback(uint16_t pin)
{
	/* USER CODE BEGIN EXTI_Falling_Callback_1 */

	/* USER CODE END EXTI_Falling_Callback_1 */
	/* Callback when data is available in Network CoProcessor to enable SPI Clock */
	if (pin == SPI_RDY_Pin)
	{
		spi_iface_ncp_ready_low();
	}

	/* Callback when user button is pressed */
	if (pin == USER_BUTTON_Pin)
	{
		SET_EVENT_BIT(EVT_TIM_UPDATE);
	}
	/* USER CODE BEGIN EXTI_Falling_Callback_End */

	/* USER CODE END EXTI_Falling_Callback_End */
}

void TimerUpdate_Callback()
{
	SET_EVENT_BIT(EVT_TIM_UPDATE);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
