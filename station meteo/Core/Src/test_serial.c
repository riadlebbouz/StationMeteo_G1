/* TP 1 : test du Serial Monitor (USART2 via le ST-Link, 115200 bauds).
 * Copie autonome du test de main.c (TEST_SERIAL_MONITEUR). */
#include "main.h"
#include "led.h"
#include "test_composants.h"
#include <stdio.h>

void Test_Serial(void)
{
  LED_Activer();
  printf("========================================\r\n");
  printf("   TP 1 : Test du Serial Monitor UART   \r\n");
  printf("========================================\r\n");
  while (1)
  {
    printf("Message de vie : Le systeme tourne...\r\n");
    /* Clignotement : LED RGB verte (PA5/LD2 est pris par la carte SD) */
    LED_Couleur(LED_VERTE);
    HAL_Delay(500);
    LED_Couleur(LED_ETEINTE);
    HAL_Delay(500);
  }
}
