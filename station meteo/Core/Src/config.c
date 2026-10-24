/* Paramètres de la station et commandes de la console de configuration.
 * La STM32L476 n'a pas d'EEPROM : les paramètres sont stockés dans la dernière
 * page de 2 Ko de la flash (banque 2, page 255, adresse 0x080FF800). */
#include "main.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern RTC_HandleTypeDef hrtc;

Config cfg;

/* ---------- Table des paramètres ---------- */
typedef struct
{
  const char *nom;
  int32_t *valeur;
  int32_t min, max, defaut;
} ParamDef;

static const ParamDef PARAMS[] =
{
  {"LOG_INTERVAL",  &cfg.log_interval,  1,    1440, 10},
  {"FILE_MAX_SIZE", &cfg.file_max_size, 256,  1048576, 2048},
  {"TIMEOUT",       &cfg.timeout,       1,    300,  30},
  {"LUMIN",         &cfg.lumin,         0,    1,    1},
  {"LUMIN_LOW",     &cfg.lumin_low,     0,    1023, 255},
  {"LUMIN_HIGH",    &cfg.lumin_high,    0,    1023, 768},
  {"TEMP_AIR",      &cfg.temp_air,      0,    1,    1},
  {"MIN_TEMP_AIR",  &cfg.min_temp_air,  -40,  85,   -10},
  {"MAX_TEMP_AIR",  &cfg.max_temp_air,  -40,  85,   60},
  {"HYGR",          &cfg.hygr,          0,    1,    1},
  {"HYGR_MINT",     &cfg.hygr_mint,     -40,  85,   0},
  {"HYGR_MAXT",     &cfg.hygr_maxt,     -40,  85,   50},
};
#define NB_PARAMS (sizeof(PARAMS) / sizeof(PARAMS[0]))

/* ---------- Stockage en flash ---------- */
#define CFG_ADRESSE  0x080FF800UL
#define CFG_BANQUE   FLASH_BANK_2
#define CFG_PAGE     255
#define CFG_MAGIC    0x57575702UL /* "WWW" v2 : sans les paramètres de pression */
#define CFG_MAGIC_V1 0x57575701UL /* v1 : mêmes paramètres + PRESSURE, PRESSURE_MIN, PRESSURE_MAX à la fin */
#define CFG_MOTS_V1  (sizeof(Config) / 4 + 3)

typedef struct
{
  uint32_t magic;
  Config   cfg;
  uint32_t somme;
} ConfigFlash;

static uint32_t Somme_Mots(const uint32_t *p, uint32_t nb)
{
  uint32_t s = 0x1234ABCD;
  for (uint32_t i = 0; i < nb; i++) s = (s << 3 | s >> 29) ^ p[i];
  return s;
}

static uint32_t Somme(const Config *c)
{
  return Somme_Mots((const uint32_t *)c, sizeof(Config) / 4);
}

static void Config_Sauver(void)
{
  ConfigFlash img __attribute__((aligned(8)));
  uint64_t mots[(sizeof(ConfigFlash) + 7) / 8];
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t err = 0;

  img.magic = CFG_MAGIC;
  img.cfg = cfg;
  img.somme = Somme(&cfg);
  memset(mots, 0xFF, sizeof(mots));
  memcpy(mots, &img, sizeof(img));

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks = CFG_BANQUE;
  erase.Page = CFG_PAGE;
  erase.NbPages = 1;
  if (HAL_FLASHEx_Erase(&erase, &err) == HAL_OK)
  {
    for (uint32_t i = 0; i < sizeof(mots) / 8; i++)
    {
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, CFG_ADRESSE + i * 8, mots[i]) != HAL_OK)
      {
        printf("Erreur d'ecriture en flash\r\n");
        break;
      }
    }
  }
  else
  {
    printf("Erreur d'effacement de la flash\r\n");
  }
  HAL_FLASH_Lock();
}

void Config_Defaut(void)
{
  for (uint32_t i = 0; i < NB_PARAMS; i++) *PARAMS[i].valeur = PARAMS[i].defaut;
}

void Config_Charger(void)
{
  const ConfigFlash *f = (const ConfigFlash *)CFG_ADRESSE;
  const uint32_t *v1 = (const uint32_t *)CFG_ADRESSE; /* magic, 15 paramètres, somme */

  if (f->magic == CFG_MAGIC && f->somme == Somme(&f->cfg))
  {
    cfg = f->cfg;
  }
  else if (v1[0] == CFG_MAGIC_V1 && v1[1 + CFG_MOTS_V1] == Somme_Mots(v1 + 1, CFG_MOTS_V1))
  {
    /* Ancien format : les paramètres gardés sont au début, dans le même ordre */
    memcpy(&cfg, v1 + 1, sizeof(Config));
    Config_Sauver(); /* réécrit au nouveau format */
  }
  else
  {
    Config_Defaut();
  }
}

void Config_Afficher(void)
{
  for (uint32_t i = 0; i < NB_PARAMS; i++)
    printf("  %-14s = %ld\r\n", PARAMS[i].nom, (long)*PARAMS[i].valeur);
}

/* ---------- Horloge RTC ---------- */
static const char *JOURS[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

void Config_RTC_Marquer(void)
{
  /* Empêche MX_RTC_Init de remettre l'heure à zéro au prochain reset */
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);
}

uint8_t Config_RTC_Reglee(void)
{
  return HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0x32F2;
}

static uint8_t Cmd_Clock(const char *arg)
{
  int h, m, s = 0;
  RTC_TimeTypeDef t = {0};
  int nb = sscanf(arg, "%d:%d:%d", &h, &m, &s); /* HH:MM accepté (secondes à 0) */
  if (nb < 2 || h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59)
  {
    printf("Format : CLOCK=HH:MM:SS\r\n");
    return 1;
  }
  t.Hours = h; t.Minutes = m; t.Seconds = s;
  t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  t.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) == HAL_OK)
  {
    Config_RTC_Marquer();
    printf("Heure = %02d:%02d:%02d\r\n", h, m, s);
  }
  else printf("Erreur d'acces a l'horloge RTC\r\n");
  return 1;
}

static uint8_t Cmd_Date(const char *arg)
{
  int mois, jour, annee;
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  char sep1, sep2; /* séparateur , / ou - accepté (10,18,2026 ou 10/18/2026) */
  if (sscanf(arg, "%d%c%d%c%d", &mois, &sep1, &jour, &sep2, &annee) != 5 ||
      (sep1 != ',' && sep1 != '/' && sep1 != '-') || sep2 != sep1 || mois < 1 || mois > 12 ||
      jour < 1 || jour > 31 || annee < 2000 || annee > 2099)
  {
    printf("Format : DATE=MOIS,JOUR,ANNEE (ex. DATE=10,13,2026)\r\n");
    return 1;
  }
  HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN); /* garde le jour de la semaine */
  d.Month = mois; d.Date = jour; d.Year = annee - 2000;
  if (HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) == HAL_OK)
  {
    Config_RTC_Marquer();
    printf("Date = %02d/%02d/%04d\r\n", jour, mois, annee);
  }
  else printf("Erreur d'acces a l'horloge RTC\r\n");
  return 1;
}

static uint8_t Cmd_Day(const char *arg)
{
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  for (int i = 0; i < 7; i++)
  {
    if (strlen(arg) >= 3 && strncmp(arg, JOURS[i], 3) == 0) /* SUN ou SUNDAY */
    {
      HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
      HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
      d.WeekDay = i + 1; /* RTC_WEEKDAY_MONDAY = 1 ... SUNDAY = 7 */
      if (HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) == HAL_OK)
      {
        Config_RTC_Marquer();
        printf("Jour = %s\r\n", JOURS[i]);
      }
      else printf("Erreur d'acces a l'horloge RTC\r\n");
      return 1;
    }
  }
  printf("Format : DAY=MON|TUE|WED|THU|FRI|SAT|SUN\r\n");
  return 1;
}

/* Enlève les espaces au début et à la fin (ex. "CLOCK = 21:18:40") */
static char *Nettoyer(char *s)
{
  char *fin;
  while (*s == ' ') s++;
  fin = s + strlen(s);
  while (fin > s && fin[-1] == ' ') *--fin = '\0';
  return s;
}

/* ---------- Commandes ---------- */
uint8_t Config_Commande(char *cmd)
{
  cmd = Nettoyer(cmd);
  char *arg = strchr(cmd, '=');
  if (arg == NULL) arg = strchr(cmd, ' ');
  if (arg != NULL)
  {
    *arg++ = '\0';
    cmd = Nettoyer(cmd);
    arg = Nettoyer(arg);
  }

  if (strcmp(cmd, "RESET") == 0)
  {
    Config_Defaut();
    Config_Sauver();
    printf("Parametres reinitialises aux valeurs par defaut\r\n");
    return 1;
  }
  if (strcmp(cmd, "VERSION") == 0)
  {
    printf("%s - lot %s\r\n", VERSION_PROGRAMME, NUMERO_LOT);
    return 1;
  }
  if (strcmp(cmd, "PARAMS") == 0 || strcmp(cmd, "HELP") == 0)
  {
    Config_Afficher();
    printf("Commandes : <PARAM>=<valeur>, RESET, VERSION, PARAMS,\r\n"
           "  CLOCK=HH:MM:SS, DATE=MOIS,JOUR,ANNEE, DAY=MON..SUN\r\n");
    return 1;
  }
  if (arg == NULL) return 0;

  if (strcmp(cmd, "CLOCK") == 0) return Cmd_Clock(arg);
  if (strcmp(cmd, "DATE") == 0)  return Cmd_Date(arg);
  if (strcmp(cmd, "DAY") == 0)   return Cmd_Day(arg);

  if (strcmp(cmd, "LOG_INTERVALL") == 0) cmd = "LOG_INTERVAL"; /* orthographe du document */

  for (uint32_t i = 0; i < NB_PARAMS; i++)
  {
    if (strcmp(cmd, PARAMS[i].nom) == 0)
    {
      char *fin;
      long v = strtol(arg, &fin, 10);
      if (fin == arg || *fin != '\0' || v < PARAMS[i].min || v > PARAMS[i].max)
      {
        printf("Valeur invalide pour %s (%ld a %ld)\r\n", PARAMS[i].nom,
               (long)PARAMS[i].min, (long)PARAMS[i].max);
        return 1;
      }
      *PARAMS[i].valeur = (int32_t)v;
      Config_Sauver();
      printf("%s = %ld\r\n", PARAMS[i].nom, v);
      return 1;
    }
  }
  return 0;
}
