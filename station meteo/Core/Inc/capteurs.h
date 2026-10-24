/* Acquisition des capteurs de la station (DHT11, BH1750, GPS), non bloquante :
 * Capteurs_Demarrer() lance une acquisition, Capteurs_Tache() est appelée en boucle
 * et renvoie 1 quand toutes les mesures sont prises ou que TIMEOUT est écoulé. */
#ifndef CAPTEURS_H
#define CAPTEURS_H

#include <stdint.h>

typedef struct
{
  uint8_t temp_ok;  int16_t temp;     /* °C */
  uint8_t hygr_ok;  int16_t hygr;     /* % */
  uint8_t lum_ok;   uint16_t lux;
  uint8_t gps_lu;                     /* GPS demandé pour cette mesure */
  uint8_t gps_ok;   char gps[40];     /* "4851.1234N,00221.5678E" */
} Mesures;

typedef struct
{
  uint8_t capteur;    /* un capteur a fait 2 timeouts de suite */
  uint8_t gps;        /* le GPS a fait 2 timeouts de suite */
  uint8_t incoherent; /* valeur hors des seuils MIN/MAX */
} CapteursErreurs;

void Capteurs_Init(void);
void Capteurs_Demarrer(uint8_t avec_gps);
void Capteurs_Annuler(void);
uint8_t Capteurs_EnCours(void);
uint8_t Capteurs_Tache(void);
const Mesures *Capteurs_Resultat(void);
const CapteursErreurs *Capteurs_Erreurs(void);

/* Réception GPS, appelée par USART1_IRQHandler : 1 si traitée ici */
uint8_t Capteurs_GPS_IRQ(void);

/* Règle l'horloge RTC avec l'heure GPS si elle n'est pas réglée (à appeler en boucle) */
void Capteurs_GPS_Horloge(void);

/* Formate la ligne de mesures (sans horodatage) : T;H;L;GPS */
void Capteurs_Formater(char *buf, uint32_t taille);

#endif
