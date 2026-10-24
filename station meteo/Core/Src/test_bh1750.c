/* TP 6 : capteur de luminosité BH1750 en I2C1 (SCL = D15/PB8, SDA = D14/PB9, adresse 0x23).
 * Copie autonome du test de main.c (TEST_BH1750) : code du guide, affichage corrigé. */
#include "main.h"
#include "test_composants.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

static void BH1750_Init(I2C_HandleTypeDef *hi2c) {
  uint8_t cmd = 0x10; // Mode Continu Haute Résolution (1 lux)
  HAL_I2C_Master_Transmit(hi2c, (0x23 << 1), &cmd, 1, HAL_MAX_DELAY);
}

void Test_BH1750(void)
{
  printf("========================================\r\n");
  printf("   TP 6 : Capteur de luminosite BH1750  \r\n");
  printf("========================================\r\n");
  /* Diagnostic : capteurs présents sur le bus I2C (le BH1750 doit répondre en 0x23) */
  printf("Scan I2C1 (D15/D14) :");
  for (uint16_t adr = 0x08; adr < 0x78; adr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, adr << 1, 2, 10) == HAL_OK)
      printf(" 0x%02X", adr);
  }
  printf(" (fin)\r\n");
  BH1750_Init(&hi2c1);
  HAL_Delay(180); /* première mesure haute résolution prête après ~180 ms */
  while (1)
  {
    uint8_t rx_data[2] = {0};
    if (HAL_I2C_Master_Receive(&hi2c1, (0x23 << 1), rx_data, 2, HAL_MAX_DELAY) == HAL_OK)
    {
      uint16_t raw_val = (rx_data[0] << 8) | rx_data[1];
      /* lux = raw_val / 1.2 ; calculé en centièmes de lux pour afficher 2 décimales sans float */
      uint32_t lux_x100 = ((uint32_t)raw_val * 1000) / 12;
      printf("[BH1750] Luminosite : %lu.%02lu Lux\r\n", lux_x100 / 100, lux_x100 % 100);
    }
    else
    {
      printf("[ERREUR] Le BH1750 ne repond pas en 0x23 (cablage SCL/SDA/VCC/GND ?)\r\n");
      BH1750_Init(&hi2c1); /* relance la mesure si le capteur vient d'être branché */
    }
    HAL_Delay(1000);
  }
}
