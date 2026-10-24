/* LED RGB d'état : couleur du mode et codes d'erreur clignotants (1 Hz).
 * Pilotée en PWM logiciel depuis l'interruption SysTick (LED_Tick, 1 ms). */
#ifndef LED_H
#define LED_H

#include <stdint.h>

/* 1 = codes d'erreur clignotants du document (comportement normal),
 * 0 = la LED affiche toujours la couleur du mode (erreurs seulement sur le port série).
 * A remettre à 1 une fois les capteurs et la carte SD branchés. */
#define LED_CODES_ERREUR 0

typedef enum
{
  LED_ERR_AUCUNE = 0,
  LED_ERR_RTC,         /* rouge / bleu, durées égales */
  LED_ERR_GPS,         /* rouge / jaune, durées égales */
  LED_ERR_CAPTEUR,     /* rouge / vert, durées égales */
  LED_ERR_INCOHERENT,  /* rouge / vert, vert 2 fois plus long */
  LED_ERR_SD_PLEINE,   /* rouge / blanc, durées égales */
  LED_ERR_SD,          /* rouge / blanc, blanc 2 fois plus long */
} LedErreur;

void LED_Activer(void);
void LED_Couleur(uint8_t r, uint8_t g, uint8_t b); /* intensités 0..10 */
void LED_Erreur(LedErreur e);
void LED_Tick(void); /* à appeler toutes les 1 ms (SysTick) */

#endif
