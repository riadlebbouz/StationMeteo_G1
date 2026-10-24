/* Enregistrement des mesures sur la carte SD avec FatFs.
 * Nom des fichiers : AAMMJJ_n.LOG (ex. 261013_0.LOG). On écrit toujours en révision 0 ;
 * quand il est plein, il est renommé en révision n (première libre) et un
 * nouveau fichier révision 0 est créé. */
#include "main.h"
#include "logger.h"
#include "config.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>

static FATFS fs;
static uint8_t monte = 0;

SdEtat Logger_Monter(void)
{
  if (monte) return SD_OK;
  if (f_mount(&fs, "0:/", 1) != FR_OK)
  {
    f_mount(NULL, "0:/", 0);
    return SD_ERREUR;
  }
  monte = 1;
  return SD_OK;
}

void Logger_Demonter(void)
{
  f_mount(NULL, "0:/", 0);
  monte = 0;
}

static SdEtat Erreur(FRESULT fr)
{
  if (fr == FR_DENIED) return SD_PLEINE; /* plus de place (FAT ou répertoire plein) */
  Logger_Demonter(); /* on remontera à la prochaine écriture */
  return SD_ERREUR;
}

static SdEtat Archiver(const char *nom0, uint8_t a, uint8_t m, uint8_t j)
{
  char nom[24];
  FILINFO info;
  /* Noms 8.3 (pas de noms longs) : AAMMJJ_n avec n de 1 à 9 */
  for (int n = 1; n <= 9; n++)
  {
    snprintf(nom, sizeof(nom), "%02u%02u%02u_%d.LOG", a, m, j, n);
    FRESULT fr = f_stat(nom, &info);
    if (fr == FR_NO_FILE)
    {
      fr = f_rename(nom0, nom);
      if (fr != FR_OK) return Erreur(fr);
      printf("[SD] %s plein, archive en %s\r\n", nom0, nom);
      return SD_OK;
    }
    if (fr != FR_OK) return Erreur(fr);
  }
  return SD_PLEINE; /* 9 archives pour ce jour */
}

SdEtat Logger_Ecrire(const char *ligne, uint8_t annee, uint8_t mois, uint8_t jour)
{
  char nom0[24];
  FIL fil;
  UINT ecrit;
  FRESULT fr;
  UINT len = (UINT)strlen(ligne);
  SdEtat etat;

  if ((etat = Logger_Monter()) != SD_OK) return etat;

  snprintf(nom0, sizeof(nom0), "%02u%02u%02u_0.LOG", annee, mois, jour);

  fr = f_open(&fil, nom0, FA_OPEN_ALWAYS | FA_WRITE);
  if (fr != FR_OK) return Erreur(fr);

  if (f_size(&fil) > 0 && f_size(&fil) + len > (FSIZE_t)cfg.file_max_size)
  {
    f_close(&fil);
    if ((etat = Archiver(nom0, annee, mois, jour)) != SD_OK) return etat;
    fr = f_open(&fil, nom0, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return Erreur(fr);
  }

  fr = f_lseek(&fil, f_size(&fil));
  if (fr == FR_OK) fr = f_write(&fil, ligne, len, &ecrit);
  if (fr != FR_OK)
  {
    f_close(&fil);
    return Erreur(fr);
  }
  fr = f_close(&fil);
  if (fr != FR_OK) return Erreur(fr);
  if (ecrit < len) return SD_PLEINE; /* volume plein */
  return SD_OK;
}
