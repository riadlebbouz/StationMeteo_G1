/* LED RGB d'état : couleur du mode et codes d'erreur clignotants (1 Hz).
 * Pilotée en PWM logiciel depuis l'interruption SysTick (LED_Tick, 1 ms).
 * Broches (CubeMX, GPIO_Output) : PA0 = rouge (A0), PA1 = vert (A1), PA4 = bleu (A2). */
#ifndef LED_H
#define LED_H

#include <stdint.h>

/* Type de LED RGB 4 pattes (la patte la plus longue est la commune) :
 * 0 = cathode commune (patte longue sur GND)  : couleur allumée = broche à 1
 * 1 = anode commune   (patte longue sur 3V3)  : couleur allumée = broche à 0
 * Si au démarrage les couleurs sont "à l'envers" (LED allumée en blanc/cyan
 * quand elle devrait être éteinte), changer cette valeur et recompiler. */
#define LED_ANODE_COMMUNE 1 /* ta LED s'allumait en rose en mode vert : anode commune */

/* 1 = codes d'erreur clignotants du document (comportement normal),
 * 0 = la LED affiche toujours la couleur du mode (erreurs seulement sur le port série).
 * A remettre à 1 une fois les capteurs et la carte SD branchés. */
#define LED_CODES_ERREUR 1

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

/* Couleurs des modes (intensités 0..10, à ajuster selon ta LED et tes résistances) */
#define LED_ETEINTE  0, 0, 0
#define LED_ROUGE   10, 0, 0
#define LED_VERTE    0, 10, 0
#define LED_BLEUE    0, 0, 10
#define LED_JAUNE   10, 5, 0
#define LED_ORANGE  10, 2, 0
#define LED_BLANCHE 10, 10, 10

#endif
