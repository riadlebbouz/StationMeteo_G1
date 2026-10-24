/* LED RGB d'état : couleur du mode et codes d'erreur clignotants (1 Hz).
 * PWM logiciel à 100 Hz (10 niveaux d'intensité) appelé toutes les 1 ms
 * depuis SysTick_Handler (stm32l4xx_it.c, USER CODE SysTick_IRQn 1).
 * Broches (CubeMX) : PA0 = rouge, PA1 = vert, PA4 = bleu (même port GPIOA). */
#include "main.h"
#include "led.h"

static volatile uint8_t actif = 0;
static volatile uint8_t niv_r = 0, niv_g = 0, niv_b = 0;
static volatile LedErreur erreur = LED_ERR_AUCUNE;
static uint32_t ms = 0;

void LED_Activer(void)
{
  actif = 1;
}

void LED_Couleur(uint8_t r, uint8_t g, uint8_t b)
{
  niv_r = (r > 10) ? 10 : r;
  niv_g = (g > 10) ? 10 : g;
  niv_b = (b > 10) ? 10 : b;
}

void LED_Erreur(LedErreur e)
{
  erreur = e;
}

/* Allume (1) ou éteint (0) chaque couleur en tenant compte du type de LED */
static void Ecrire(uint8_t r, uint8_t g, uint8_t b)
{
  uint32_t allumees = (r ? LED_R_Pin : 0) | (g ? LED_G_Pin : 0) | (b ? LED_B_Pin : 0);
  uint32_t eteintes = (LED_R_Pin | LED_G_Pin | LED_B_Pin) & ~allumees;

#if LED_ANODE_COMMUNE
  /* anode commune : couleur allumée = broche à 0 */
  LED_R_GPIO_Port->BSRR = (allumees << 16) | eteintes;
#else
  /* cathode commune : couleur allumée = broche à 1 */
  LED_R_GPIO_Port->BSRR = allumees | (eteintes << 16);
#endif
}

void LED_Tick(void)
{
  uint8_t r, g, b, pas;

  ms++;
  if (!actif)
  {
    Ecrire(0, 0, 0);
    return;
  }

  r = niv_r;
  g = niv_g;
  b = niv_b;

#if LED_CODES_ERREUR
  if (erreur != LED_ERR_AUCUNE)
  {
    /* 1 Hz : rouge puis 2e couleur ; "2 fois plus long" = 1/3 rouge, 2/3 l'autre */
    uint32_t t = ms % 1000;
    uint32_t duree_rouge = (erreur == LED_ERR_INCOHERENT || erreur == LED_ERR_SD) ? 333 : 500;

    if (t < duree_rouge)
    {
      r = 10; g = 0; b = 0;
    }
    else
    {
      switch (erreur)
      {
        case LED_ERR_RTC:        r = 0;  g = 0;  b = 10; break; /* bleu  */
        case LED_ERR_GPS:        r = 10; g = 5;  b = 0;  break; /* jaune */
        case LED_ERR_CAPTEUR:
        case LED_ERR_INCOHERENT: r = 0;  g = 10; b = 0;  break; /* vert  */
        default:                 r = 10; g = 10; b = 10; break; /* blanc */
      }
    }
  }
#endif

  pas = ms % 10;
  Ecrire(pas < r, pas < g, pas < b);
}
