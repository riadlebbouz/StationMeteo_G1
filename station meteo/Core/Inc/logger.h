/* Enregistrement des mesures sur la carte SD (fichiers AAMMJJ_n.LOG). */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>

typedef enum
{
  SD_OK = 0,
  SD_ERREUR,  /* carte absente, montage ou écriture impossible */
  SD_PLEINE,
} SdEtat;

SdEtat Logger_Monter(void);
void Logger_Demonter(void);

/* Ajoute une ligne (terminée par \r\n) au fichier du jour, révision 0.
 * Quand le fichier dépasserait FILE_MAX_SIZE, il est archivé en révision n
 * et l'écriture recommence dans un fichier révision 0 vide. */
SdEtat Logger_Ecrire(const char *ligne, uint8_t annee, uint8_t mois, uint8_t jour);

#endif
