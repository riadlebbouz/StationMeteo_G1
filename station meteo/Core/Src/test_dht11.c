/* TP 4 : capteur DHT11 sur PB0 (A3), affichage sur le Serial Monitor.
 * Copie autonome du test de main.c (TEST_DHT11) : code du guide, adapté. */
#include "main.h"
#include "led.h"
#include "test_composants.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

static inline void DWT_Init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void delay_us(uint32_t us) {
  uint32_t startTicks = DWT->CYCCNT;
  uint32_t targetTicks = us * (SystemCoreClock / 1000000);
  while ((DWT->CYCCNT - startTicks) < targetTicks);
}

static inline void Set_Pin_Output_PB0(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

static inline void Set_Pin_Input_PB0(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP; /* filet de sécurité si le capteur n'a pas de résistance de tirage */
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

/* Attend tant que PB0 vaut 'etat' ; 0 si ça dure plus de timeout_us (capteur absent) */
static uint8_t DHT11_Attendre(GPIO_PinState etat, uint32_t timeout_us) {
  uint32_t startTicks = DWT->CYCCNT;
  uint32_t maxTicks = timeout_us * (SystemCoreClock / 1000000);
  while (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) == etat) {
    if ((DWT->CYCCNT - startTicks) > maxTicks) return 0;
  }
  return 1;
}

/* Remplace la fonction imbriquée read_byte du guide (extension GCC) ; *ok = 0 si timeout */
static uint8_t DHT11_ReadByte(uint8_t *ok) {
  uint8_t data = 0;
  for (int i = 0; i < 8; i++) {
    if (!DHT11_Attendre(GPIO_PIN_RESET, 1000)) { *ok = 0; return 0; }
    delay_us(40);
    if (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) == GPIO_PIN_SET)
      data |= (1 << (7 - i));
    if (!DHT11_Attendre(GPIO_PIN_SET, 1000)) { *ok = 0; return 0; }
  }
  return data;
}

/* Cause du dernier échec de DHT11_Read, affichée avec [ERREUR] */
static const char *dht11_diag = "";

static uint8_t DHT11_Read(uint8_t *Temperature, uint8_t *Humidity) {
  uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2, sum;
  uint8_t ok = 1;

  // 1. Signal de Start (MCU maintient PB0 à 0 pendant 18ms)
  Set_Pin_Output_PB0();
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
  HAL_Delay(18);

  // 2. Relâcher la ligne et passer en entrée pour écouter le capteur
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
  delay_us(30);
  Set_Pin_Input_PB0();

  // 3. Réponse du DHT11 (descente, 80us bas, 80us haut) : timing critique, sans interruptions
  __disable_irq();
  if (!DHT11_Attendre(GPIO_PIN_SET, 1000)) {
    __enable_irq();
    dht11_diag = "le capteur ne repond pas (ligne DATA reste a 1 : fil DATA/VCC/GND ?)";
    return 0;
  }
  if (!DHT11_Attendre(GPIO_PIN_RESET, 1000) ||
      !DHT11_Attendre(GPIO_PIN_SET, 1000)) {
    __enable_irq();
    dht11_diag = "reponse du capteur incomplete (ligne DATA bloquee a 0 ?)";
    return 0;
  }

  Rh_byte1 = DHT11_ReadByte(&ok);
  Rh_byte2 = DHT11_ReadByte(&ok);
  Temp_byte1 = DHT11_ReadByte(&ok);
  Temp_byte2 = DHT11_ReadByte(&ok);
  sum = DHT11_ReadByte(&ok);
  __enable_irq();

  // 4. Vérification du checksum
  if (ok && sum == ((Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2) & 0xFF)) {
    *Humidity = Rh_byte1;
    *Temperature = Temp_byte1;
    return 1; // Succès
  }
  dht11_diag = ok ? "erreur de checksum" : "lecture des 40 bits interrompue";
  return 0; // Timeout ou erreur de checksum
}

void Test_DHT11(void)
{
  uint8_t temperature = 0;
  uint8_t humidity = 0;
  LED_Activer();
  DWT_Init();
  printf("========================================\r\n");
  printf("   TP 4 : Test du capteur DHT11 (PB0)   \r\n");
  printf("========================================\r\n");
  HAL_Delay(1000); /* le DHT11 a besoin d'1 s après la mise sous tension */
  /* Diagnostic : capteurs I2C présents sur SCL/D15 (PB8) - SDA/D14 (PB9) */
  printf("Scan I2C1 (D15/D14) :");
  for (uint16_t adr = 0x08; adr < 0x78; adr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, adr << 1, 2, 10) == HAL_OK)
      printf(" 0x%02X", adr);
  }
  printf(" (fin)\r\n");
  while (1)
  {
    if (DHT11_Read(&temperature, &humidity))
    {
      printf("[SUCCES] Temperature : %d C  |  Humidite : %d %%\r\n", temperature, humidity);
      /* PA5/LD2 sert à la carte SD : flash vert sur la LED RGB à la place */
      LED_Couleur(LED_VERTE);
      HAL_Delay(100);
      LED_Couleur(LED_ETEINTE);
    }
    else
    {
      printf("[ERREUR] Echec de lecture du DHT11 sur PB0 ! (%s)\r\n", dht11_diag);
      LED_Couleur(LED_ROUGE);
      HAL_Delay(400);
      LED_Couleur(LED_ETEINTE);
    }
    HAL_Delay(2000); /* au moins 2 s entre deux lectures */
  }
}
