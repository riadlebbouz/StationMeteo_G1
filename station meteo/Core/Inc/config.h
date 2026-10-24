/* Paramètres de la station (document "Modes de fonctionnement"),
 * sauvegardés dans la dernière page de la flash (émulation d'EEPROM). */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define VERSION_PROGRAMME "WWW v1.0"
#define NUMERO_LOT        "LOT-001"

/* 1 = LOG_INTERVAL compté en secondes (démonstration), 0 = en minutes (document) */
#define LOG_INTERVAL_EN_SECONDES 0

/* Décalage de l'heure locale par rapport à l'heure UTC du GPS (Algérie : UTC+1, pas d'heure d'été) */
#define FUSEAU_HORAIRE_H 1

typedef struct
{
  int32_t log_interval;   /* minutes */
  int32_t file_max_size;  /* octets */
  int32_t timeout;        /* secondes */
  int32_t lumin;
  int32_t lumin_low;
  int32_t lumin_high;
  int32_t temp_air;
  int32_t min_temp_air;
  int32_t max_temp_air;
  int32_t hygr;
  int32_t hygr_mint;
  int32_t hygr_maxt;
} Config; /* pas de capteur de pression dans le projet : PRESSURE* retirés */

extern Config cfg;

void Config_Charger(void);   /* lit la flash, ou valeurs par défaut */
void Config_Defaut(void);
void Config_Afficher(void);

/* Exécute une commande de la console (ligne déjà en majuscules).
 * Renvoie 1 si la commande est connue. */
uint8_t Config_Commande(char *cmd);

/* RTC : marque "heure réglée" (registre de sauvegarde, perdu à la coupure de courant) */
void Config_RTC_Marquer(void);
uint8_t Config_RTC_Reglee(void);

#endif
