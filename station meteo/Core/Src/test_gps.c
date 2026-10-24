/* TP 8 : module GPS sur USART1 (TX = D8/PA9, RX = D2/PA10), 9600 bauds 8N1
 * (déplacé de USART3 PC10/PC11, inaccessibles, voir main.h).
 * Réception caractère par caractère en interruption. Les trames NMEA sont
 * analysées (RMC, GGA, GSV) et un résumé d'une ligne est affiché chaque seconde
 * sur le Serial Monitor (UART2), pour avoir un verdict sans attendre le fix :
 *   GPS OK : trames recues | heure UTC 13:20:05 | satellites vus 3 (signal max 29 dB) | utilises 0 | position : en attente
 *   POSITION TROUVEE : 48.85660 N, 2.35220 E | satellites utilises 5
 *   GPS : AUCUNE TRAME -> verifier cablage/alim   (après 10 s sans trame)
 * LED RGB : rouge = aucune trame, jaune = trames sans position, verte = position.
 * PA5 (LD2) est le SCK de la carte SD, d'où la LED RGB. */
#include "main.h"
#include "led.h"
#include "test_composants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart3;

#define GPS_TRAMES_BRUTES 0 /* 1 = afficher aussi chaque trame NMEA brute */
#define GPS_BUFFER_SIZE   100
#define NB_TRAMES         4 /* file d'attente ISR -> boucle principale */
#define SANS_TRAME_MS     10000UL

static char gps_rx_buffer[GPS_BUFFER_SIZE];
static uint8_t gps_rx_data;
static volatile uint16_t gps_index = 0;
static char trames[NB_TRAMES][GPS_BUFFER_SIZE];
static volatile uint8_t tete = 0, queue = 0;

/* Callback d'interruption UART (GPS) : une trame complète est copiée dans la file */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != GPS_USART) return;
  if (gps_rx_data == '\n' || gps_rx_data == '\r')
  {
    if (gps_index > 0)
    {
      uint8_t suivant = (tete + 1) % NB_TRAMES;
      gps_rx_buffer[gps_index] = '\0';
      if (suivant != queue) /* file pleine : trame perdue */
      {
        strcpy(trames[tete], gps_rx_buffer);
        tete = suivant;
      }
      gps_index = 0;
    }
  }
  else if (gps_rx_data == '$' || gps_index >= GPS_BUFFER_SIZE - 1)
  {
    gps_index = 0; /* début de trame (ou trame trop longue) */
    gps_rx_buffer[gps_index++] = gps_rx_data;
  }
  else
  {
    gps_rx_buffer[gps_index++] = gps_rx_data;
  }
  HAL_UART_Receive_IT(&huart3, &gps_rx_data, 1);
}

/* Erreur (overrun si le GPS émet avant le démarrage de la réception) : on relance */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != GPS_USART) return;
  gps_index = 0;
  HAL_UART_Receive_IT(&huart3, &gps_rx_data, 1);
}

/* ---------- Analyse NMEA ---------- */
static uint8_t Checksum_OK(const char *t)
{
  uint8_t x = 0;
  const char *p = t + 1;
  while (*p && *p != '*') x ^= (uint8_t)*p++;
  return *p == '*' && strtoul(p + 1, NULL, 16) == x;
}

/* Copie le champ n (0 = "$GPxxx") dans out ; renvoie la longueur */
static int Champ(const char *t, int n, char *out, int taille)
{
  int lg = 0;
  while (n > 0 && *t) if (*t++ == ',') n--;
  while (*t && *t != ',' && *t != '*' && lg < taille - 1) out[lg++] = *t++;
  out[lg] = '\0';
  return lg;
}

/* "4851.396" (ddmm.mmmm) -> degrés x 100000 */
static long Degres(const char *s, int nb_deg)
{
  char d[4] = {0};
  long minutes_e5 = 0, mult = 100000;
  const char *p;
  if (strlen(s) < (size_t)nb_deg + 2) return 0;
  memcpy(d, s, nb_deg);
  p = s + nb_deg;
  minutes_e5 = atol(p) * 100000L; /* partie entière des minutes */
  p = strchr(p, '.');
  if (p) for (p++; *p >= '0' && *p <= '9' && mult > 1; p++) { mult /= 10; minutes_e5 += (*p - '0') * mult; }
  return atol(d) * 100000L + minutes_e5 / 60;
}

typedef struct
{
  char heure[9];       /* HH:MM:SS UTC */
  int vus;             /* satellites vus (GSV) */
  int snr_max;         /* meilleur signal (dB) */
  int utilises;        /* satellites utilisés (GGA) */
  int qualite;         /* 0 = pas de position */
  char lat[16], ns[2], lon[16], ew[2];
} EtatGPS;

static EtatGPS gps;

static void Analyser(const char *t)
{
  char c[16];
  const char *type = t + 3; /* $GPRMC, $GNRMC... */

  if (strncmp(type, "RMC", 3) == 0 || strncmp(type, "GGA", 3) == 0)
  {
    if (Champ(t, 1, c, sizeof(c)) >= 6) /* heure hhmmss.ss */
      snprintf(gps.heure, sizeof(gps.heure), "%.2s:%.2s:%.2s", c, c + 2, c + 4);
  }
  if (strncmp(type, "GGA", 3) == 0)
  {
    Champ(t, 6, c, sizeof(c)); gps.qualite = atoi(c);
    Champ(t, 7, c, sizeof(c)); gps.utilises = atoi(c);
    Champ(t, 2, gps.lat, sizeof(gps.lat));
    Champ(t, 3, gps.ns, sizeof(gps.ns));
    Champ(t, 4, gps.lon, sizeof(gps.lon));
    Champ(t, 5, gps.ew, sizeof(gps.ew));
  }
  else if (strncmp(type, "GSV", 3) == 0)
  {
    Champ(t, 3, c, sizeof(c)); gps.vus = atoi(c);
    for (int k = 7; k <= 19; k += 4) /* SNR des 4 satellites de la trame */
      if (Champ(t, k, c, sizeof(c)) > 0 && atoi(c) > gps.snr_max) gps.snr_max = atoi(c);
  }
}

static void Resume(void)
{
  if (gps.qualite > 0 && gps.lat[0] && gps.lon[0])
  {
    long la = Degres(gps.lat, 2), lo = Degres(gps.lon, 3);
    LED_Couleur(LED_VERTE);
    printf("POSITION TROUVEE : %ld.%05ld %s, %ld.%05ld %s | heure UTC %s | satellites utilises %d\r\n",
           la / 100000, la % 100000, gps.ns, lo / 100000, lo % 100000, gps.ew,
           gps.heure[0] ? gps.heure : "--:--:--", gps.utilises);
  }
  else
  {
    LED_Couleur(LED_JAUNE);
    printf("GPS OK : trames recues | heure UTC %s | satellites vus %d (signal max %d dB) | utilises %d | position : en attente\r\n",
           gps.heure[0] ? gps.heure : "pas encore", gps.vus, gps.snr_max, gps.utilises);
  }
  gps.snr_max = 0; /* recalculé à chaque seconde */
}

void Test_GPS(void)
{
  uint32_t t_resume, t_trame, nb_trames = 0;

  LED_Activer();
  LED_Couleur(LED_ROUGE);
  printf("=== Test GPS USART1 (D8 = TX vers RX du GPS, D2 = RX depuis TX du GPS) ===\r\n");
  printf("Resume chaque seconde. Position : GPS pres d'une fenetre ou dehors (5 a 15 min).\r\n");
  HAL_UART_Receive_IT(&huart3, &gps_rx_data, 1);
  t_resume = t_trame = HAL_GetTick();

  while (1)
  {
    while (queue != tete)
    {
      const char *t = trames[queue];
#if GPS_TRAMES_BRUTES
      printf("[GPS] %s\r\n", t);
#endif
      if (t[0] == '$' && Checksum_OK(t))
      {
        Analyser(t);
        nb_trames++;
        t_trame = HAL_GetTick();
      }
      queue = (queue + 1) % NB_TRAMES;
    }

    if (HAL_GetTick() - t_resume >= 1000)
    {
      t_resume = HAL_GetTick();
      if (HAL_GetTick() - t_trame >= SANS_TRAME_MS)
      {
        LED_Couleur(LED_ROUGE);
        printf("GPS : AUCUNE TRAME -> verifier cablage/alim (TX GPS -> D2, RX GPS -> D8, VCC 5V, GND)\r\n");
        memset(&gps, 0, sizeof(gps));
      }
      else if (nb_trames > 0)
      {
        Resume();
      }
    }
  }
}
