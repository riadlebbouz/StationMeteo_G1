/* Test de la LED RGB (anode commune, R = PA0, G = PA1, B = PA4) :
 * rouge, vert, bleu puis éteinte, 1 s chacune, avec la couleur affichée sur le Serial Monitor. */
#include "main.h"
#include "led.h"
#include "test_composants.h"
#include <stdio.h>

void Test_LED(void)
{
  LED_Activer();
  printf("========================================\r\n");
  printf("   Test de la LED RGB (PA0/PA1/PA4)     \r\n");
  printf("========================================\r\n");
  while (1)
  {
    printf("[LED] Rouge\r\n");
    LED_Couleur(LED_ROUGE);
    HAL_Delay(1000);
    printf("[LED] Vert\r\n");
    LED_Couleur(LED_VERTE);
    HAL_Delay(1000);
    printf("[LED] Bleu\r\n");
    LED_Couleur(LED_BLEUE);
    HAL_Delay(1000);
    printf("[LED] Eteinte\r\n");
    LED_Couleur(LED_ETEINTE);
    HAL_Delay(1000);
  }
}
