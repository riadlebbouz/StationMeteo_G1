/* Test des boutons : bouton miniature (vert) sur A4/PC1 et bouton B1 (rouge) sur PC13.
 * Les deux sont en pull-up : appuyé = 0. Tant qu'un bouton est appuyé, la LED RGB prend
 * sa couleur, et chaque appui/relâchement est affiché sur le Serial Monitor. */
#include "main.h"
#include "led.h"
#include "test_composants.h"
#include <stdio.h>

static uint8_t Bouton_Appuye(GPIO_TypeDef *port, uint16_t pin)
{
  return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;
}

void Test_Bouton(void)
{
  uint8_t miniature, b1;
  uint8_t miniature_avant = 0, b1_avant = 0;

  LED_Activer();
  printf("========================================\r\n");
  printf("   Test des boutons (miniature A4, B1)  \r\n");
  printf("========================================\r\n");
  printf("Appuie sur le bouton miniature -> LED verte, sur B1 -> LED rouge\r\n");
  while (1)
  {
    miniature = Bouton_Appuye(BTN2_GPIO_Port, BTN2_Pin);
    b1 = Bouton_Appuye(BTN1_GPIO_Port, BTN1_Pin);

    if (miniature != miniature_avant)
      printf("[BOUTON] miniature (A4) %s\r\n", miniature ? "appuye" : "relache");
    if (b1 != b1_avant)
      printf("[BOUTON] B1 (bleu de la carte) %s\r\n", b1 ? "appuye" : "relache");
    miniature_avant = miniature;
    b1_avant = b1;

    if (miniature && b1)
      LED_Couleur(LED_JAUNE);
    else if (miniature)
      LED_Couleur(LED_VERTE);
    else if (b1)
      LED_Couleur(LED_ROUGE);
    else
      LED_Couleur(LED_ETEINTE);

    HAL_Delay(20); /* anti-rebond simple */
  }
}
