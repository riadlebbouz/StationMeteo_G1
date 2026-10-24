/* Acquisition des capteurs de la station : DHT11 (température, humidité),
 * BH1750 (luminosité) et GPS (position). Version simple, non optimisée.
 *
 * Capteurs_Demarrer() lance une acquisition, puis Capteurs_Tache() est appelée
 * à chaque tour de la boucle principale : elle essaie de lire chaque capteur
 * et renvoie 1 quand toutes les mesures sont prises ou que TIMEOUT est écoulé.
 * La boucle principale continue donc de tourner (boutons, LED) pendant la mesure.
 *
 * GPS : USART1 (D8 = PA9 TX, D2 = PA10 RX, 9600 bauds), caractères reçus sous interruption par
 * Capteurs_GPS_IRQ(), appelée depuis USART1_IRQHandler (stm32l4xx_it.c). */
#include "main.h"
#include "capteurs.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart3;

#define BH1750_ADRESSE   (0x23 << 1)
#define DHT11_PERIODE_MS 2000UL /* au moins 2 s entre deux lectures du DHT11 */
#define BH1750_PERIODE_MS 200UL
#define GPS_BUFFER_SIZE  128
#define DEMARRAGE_MS     1500UL /* DHT11 : 1 s après la mise sous tension ; BH1750 : 1re mesure après 180 ms */

/* Indices des capteurs dans nb_timeout[] */
enum { CAP_DHT11 = 0, CAP_BH1750, CAP_GPS, NB_CAPTEURS };

static Mesures mesures;
static CapteursErreurs erreurs;
static uint8_t nb_timeout[NB_CAPTEURS]; /* timeouts consécutifs par capteur */

/* État de l'acquisition en cours */
static uint8_t en_cours = 0;
static uint32_t t_debut;
static uint8_t dht_fini, lum_fini, gps_fini;
static uint32_t t_dht, t_lum; /* dernière tentative */
static uint32_t t_init;       /* instant de Capteurs_Init */

/* ---------- GPS : réception sous interruption ---------- */
static volatile uint8_t gps_actif = 0;
static char gps_rx_buffer[GPS_BUFFER_SIZE];
static char gps_trame[GPS_BUFFER_SIZE];
static volatile uint16_t gps_index = 0;
static volatile uint8_t gps_data_ready = 0;
static char gps_rmc[GPS_BUFFER_SIZE];   /* dernière trame RMC, pour régler l'horloge */
static volatile uint8_t gps_rmc_ready = 0;

/* Appelée par USART1_IRQHandler. Renvoie 1 si l'interruption a été traitée ici. */
uint8_t Capteurs_GPS_IRQ(void)
{
  USART_TypeDef *u = huart3.Instance;

  if (!gps_actif) return 0;

  if (u->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE))
  {
    u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF; /* caractères perdus : on continue */
  }
  while (u->ISR & USART_ISR_RXNE)
  {
    char c = (char)(u->RDR & 0xFF);
    if (c == '\n' || c == '\r')
    {
      if (gps_index > 0)
      {
        gps_rx_buffer[gps_index] = '\0';
        if (!gps_data_ready) /* la trame précédente a été lue */
        {
          strcpy(gps_trame, gps_rx_buffer);
          gps_data_ready = 1;
        }
        if (!gps_rmc_ready && strncmp(gps_rx_buffer + 3, "RMC", 3) == 0)
        {
          strcpy(gps_rmc, gps_rx_buffer); /* lue par Capteurs_GPS_Horloge(), même hors mesure */
          gps_rmc_ready = 1;
        }
        gps_index = 0;
      }
    }
    else if (gps_index < GPS_BUFFER_SIZE - 1)
    {
      gps_rx_buffer[gps_index++] = c;
    }
    else
    {
      gps_index = 0; /* trame trop longue : on recommence */
    }
  }
  return 1;
}

/* Renvoie le champ n (0 = "$GPGGA") d'une trame NMEA dans 'champ' */
static void Champ(const char *trame, int n, char *champ, int taille)
{
  int i = 0;
  while (n > 0 && *trame)
  {
    if (*trame == ',') n--;
    trame++;
  }
  while (*trame && *trame != ',' && *trame != '*' && i < taille - 1)
  {
    champ[i++] = *trame++;
  }
  champ[i] = '\0';
}

/* Somme de contrôle NMEA (XOR entre '$' et '*') */
static uint8_t GPS_Checksum_OK(const char *t)
{
  uint8_t x = 0;
  const char *p = t + 1;
  if (t[0] != '$') return 0;
  while (*p && *p != '*') x ^= (uint8_t)*p++;
  return *p == '*' && strtoul(p + 1, NULL, 16) == x;
}

static int Deux_Chiffres(const char *s)
{
  return (s[0] - '0') * 10 + (s[1] - '0');
}

/* Règle la RTC avec l'heure d'une trame RMC (UTC + FUSEAU_HORAIRE_H), seulement si
 * l'heure n'a pas été réglée (après une coupure de courant : pas de pile sur la carte).
 * Une heure réglée à la main (CLOCK/DATE) reste donc prioritaire. Le GPS donne
 * l'heure dès qu'il capte un satellite, même sans position. */
static void GPS_Regler_RTC(const char *trame)
{
  static const uint8_t jours_mois[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  char hms[12], date[8];
  int h, mi, s, j, mo, a, jmax;
  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};

  if (strncmp(trame + 3, "RMC", 3) != 0 || Config_RTC_Reglee()) return;
  Champ(trame, 1, hms, sizeof(hms));
  Champ(trame, 9, date, sizeof(date));
  if (strlen(hms) < 6 || strlen(date) != 6) return; /* GPS sans heure pour l'instant */

  h = Deux_Chiffres(hms); mi = Deux_Chiffres(hms + 2); s = Deux_Chiffres(hms + 4);
  j = Deux_Chiffres(date); mo = Deux_Chiffres(date + 2); a = Deux_Chiffres(date + 4);
  if (a < 24 || mo < 1 || mo > 12 || j < 1 || j > 31 || h > 23 || mi > 59 || s > 59) return;

  /* UTC -> heure locale, avec passage au jour suivant / précédent */
  h += FUSEAU_HORAIRE_H;
  jmax = jours_mois[mo - 1] + (mo == 2 && a % 4 == 0);
  if (h >= 24) { h -= 24; if (++j > jmax) { j = 1; if (++mo > 12) { mo = 1; a++; } } }
  if (h < 0)   { h += 24; if (--j < 1) { if (--mo < 1) { mo = 12; a--; } j = jours_mois[mo - 1] + (mo == 2 && a % 4 == 0); } }

  t.Hours = h; t.Minutes = mi; t.Seconds = s;
  t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  t.StoreOperation = RTC_STOREOPERATION_RESET;
  d.Date = j; d.Month = mo; d.Year = a;
  {
    /* Jour de la semaine (Sakamoto) : 0 = dimanche -> RTC 1 = lundi ... 7 = dimanche */
    static const int k[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = 2000 + a - (mo < 3);
    int js = (y + y / 4 - y / 100 + y / 400 + k[mo - 1] + j) % 7;
    d.WeekDay = js == 0 ? RTC_WEEKDAY_SUNDAY : js;
  }
  if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) == HAL_OK &&
      HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) == HAL_OK)
  {
    Config_RTC_Marquer();
    printf("[GPS] Horloge reglee automatiquement : %02d/%02d/20%02d %02d:%02d:%02d (UTC%+d)\r\n",
           j, mo, a, h, mi, s, FUSEAU_HORAIRE_H);
  }
}

/* Appelée à chaque tour de la boucle principale : règle l'horloge avec la dernière
 * trame RMC reçue (indépendamment des mesures, qui s'arrêtent dès la trame GGA). */
void Capteurs_GPS_Horloge(void)
{
  char trame[GPS_BUFFER_SIZE];

  if (!gps_rmc_ready) return;
  strcpy(trame, gps_rmc);
  gps_rmc_ready = 0;
  if (GPS_Checksum_OK(trame)) GPS_Regler_RTC(trame);
}

/* Cherche une trame GGA avec un fix ; remplit mesures.gps. Renvoie 1 si position trouvée. */
static uint8_t GPS_Lire_Position(void)
{
  char trame[GPS_BUFFER_SIZE];
  char lat[16], ns[4], lon[16], ew[4], fix[4];

  if (!gps_data_ready) return 0;
  strcpy(trame, gps_trame);
  gps_data_ready = 0;

  if (!GPS_Checksum_OK(trame)) return 0; /* trame abîmée */

  /* $GPGGA (GPS seul) ou $GNGGA (plusieurs constellations) */
  if (strncmp(trame + 3, "GGA", 3) != 0) return 0;

  Champ(trame, 2, lat, sizeof(lat));
  Champ(trame, 3, ns, sizeof(ns));
  Champ(trame, 4, lon, sizeof(lon));
  Champ(trame, 5, ew, sizeof(ew));
  Champ(trame, 6, fix, sizeof(fix));

  if (fix[0] == '\0' || fix[0] == '0' || lat[0] == '\0' || lon[0] == '\0') return 0; /* pas de fix */

  snprintf(mesures.gps, sizeof(mesures.gps), "%s%s,%s%s", lat, ns, lon, ew);
  return 1;
}

/* ---------- DHT11 sur PB0 (code du TP 4) ---------- */
static void DWT_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000);
  while ((DWT->CYCCNT - start) < ticks);
}

static void DHT11_Sortie(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = DHT11_Pin;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_GPIO_Port, &g);
}

static void DHT11_Entree(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = DHT11_Pin;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT11_GPIO_Port, &g);
}

/* Attend tant que PB0 vaut 'etat' ; 0 si ça dure plus de timeout_us */
static uint8_t DHT11_Attendre(GPIO_PinState etat, uint32_t timeout_us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t max = timeout_us * (SystemCoreClock / 1000000);
  while (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) == etat)
  {
    if ((DWT->CYCCNT - start) > max) return 0;
  }
  return 1;
}

static uint8_t DHT11_LireOctet(uint8_t *ok)
{
  uint8_t data = 0;
  for (int i = 0; i < 8; i++)
  {
    if (!DHT11_Attendre(GPIO_PIN_RESET, 1000)) { *ok = 0; return 0; }
    delay_us(40);
    if (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) == GPIO_PIN_SET) data |= (1 << (7 - i));
    if (!DHT11_Attendre(GPIO_PIN_SET, 1000)) { *ok = 0; return 0; }
  }
  return data;
}

static uint8_t DHT11_Lire(int16_t *temperature, int16_t *humidite)
{
  uint8_t o[5], ok = 1;

  /* 1. Signal de départ : ligne à 0 pendant 18 ms */
  DHT11_Sortie();
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
  HAL_Delay(18);
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
  delay_us(30);
  DHT11_Entree();

  /* 2. Réponse du capteur, puis 3. les 40 bits, sans interruptions */
  __disable_irq();
  if (!DHT11_Attendre(GPIO_PIN_SET, 1000) || !DHT11_Attendre(GPIO_PIN_RESET, 1000) ||
      !DHT11_Attendre(GPIO_PIN_SET, 1000))
  {
    __enable_irq();
    return 0;
  }
  for (int i = 0; i < 5; i++) o[i] = DHT11_LireOctet(&ok);
  __enable_irq();

  /* 4. Somme de contrôle */
  if (!ok || o[4] != ((o[0] + o[1] + o[2] + o[3]) & 0xFF)) return 0;
  if (o[0] == 0 && o[2] == 0) return 0; /* trame vide (capteur pas prêt) : la somme 0 passerait le contrôle */
  *humidite = o[0];
  *temperature = o[2];
  return 1;
}

/* ---------- BH1750 sur I2C1 (code du TP 6) ---------- */
static void BH1750_Init(void)
{
  uint8_t cmd = 0x10; /* mode continu haute résolution (1 lux) */
  HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADRESSE, &cmd, 1, 10);
}

static uint8_t BH1750_Lire(uint16_t *lux)
{
  uint8_t rx[2] = {0};
  if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADRESSE, rx, 2, 10) != HAL_OK)
  {
    BH1750_Init(); /* relance si le capteur vient d'être branché */
    return 0;
  }
  *lux = (uint16_t)((((uint32_t)rx[0] << 8) | rx[1]) * 10 / 12); /* lux = brut / 1,2 */
  return 1;
}

/* ---------- Interface du module ---------- */
void Capteurs_Init(void)
{
  DWT_Init();
  BH1750_Init();
  t_init = HAL_GetTick();

  /* GPS : réception caractère par caractère sous interruption USART1 (D8/D2) */
  gps_actif = 1;
  SET_BIT(huart3.Instance->CR1, USART_CR1_RXNEIE);
  HAL_NVIC_SetPriority(GPS_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(GPS_IRQn);
}

void Capteurs_Demarrer(uint8_t avec_gps)
{
  uint32_t now = HAL_GetTick();

  memset(&mesures, 0, sizeof(mesures));
  mesures.gps_lu = avec_gps;

  /* Un capteur désactivé dans la configuration est considéré comme déjà lu */
  dht_fini = !(cfg.temp_air || cfg.hygr);
  lum_fini = !cfg.lumin;
  gps_fini = !avec_gps;

  t_debut = now;
  t_dht = now - DHT11_PERIODE_MS; /* première tentative tout de suite */
  t_lum = now - BH1750_PERIODE_MS;
  gps_data_ready = 0;             /* on veut une trame récente */
  en_cours = 1;
}

void Capteurs_Annuler(void)
{
  en_cours = 0;
}

uint8_t Capteurs_EnCours(void)
{
  return en_cours;
}

/* Valeurs hors des seuils de la configuration */
static uint8_t Valeurs_Incoherentes(void)
{
  if (mesures.temp_ok && (mesures.temp < cfg.min_temp_air || mesures.temp > cfg.max_temp_air)) return 1;
  if (mesures.hygr_ok && (mesures.hygr < 0 || mesures.hygr > 100)) return 1;
  return 0;
}

/* Compte les timeouts consécutifs et met à jour les erreurs */
static void Fin_Acquisition(void)
{
  if (cfg.temp_air || cfg.hygr)
    nb_timeout[CAP_DHT11] = dht_fini ? 0 : nb_timeout[CAP_DHT11] + 1;
  if (cfg.lumin)
    nb_timeout[CAP_BH1750] = lum_fini ? 0 : nb_timeout[CAP_BH1750] + 1;
  if (mesures.gps_lu)
    nb_timeout[CAP_GPS] = mesures.gps_ok ? 0 : nb_timeout[CAP_GPS] + 1;

  erreurs.capteur = (nb_timeout[CAP_DHT11] >= 2) || (nb_timeout[CAP_BH1750] >= 2);
  erreurs.gps = (nb_timeout[CAP_GPS] >= 2);
  erreurs.incoherent = Valeurs_Incoherentes();
  en_cours = 0;
}

uint8_t Capteurs_Tache(void)
{
  uint32_t now = HAL_GetTick();
  int16_t t, h;
  uint16_t lux;

  if (!en_cours) return 1;
  if (now - t_init < DEMARRAGE_MS) return 0; /* capteurs pas encore prêts juste après le reset */

  /* --- DHT11 : une tentative toutes les 2 s jusqu'à réussite --- */
  if (!dht_fini && now - t_dht >= DHT11_PERIODE_MS)
  {
    t_dht = now;
    if (DHT11_Lire(&t, &h))
    {
      mesures.temp = t;
      mesures.temp_ok = cfg.temp_air ? 1 : 0;
      /* l'hygrométrie n'est enregistrée que si la température est dans HYGR_MINT..HYGR_MAXT */
      mesures.hygr = h;
      mesures.hygr_ok = (cfg.hygr && t >= cfg.hygr_mint && t <= cfg.hygr_maxt) ? 1 : 0;
      dht_fini = 1;
    }
  }

  /* --- BH1750 --- */
  if (!lum_fini && now - t_lum >= BH1750_PERIODE_MS)
  {
    t_lum = now;
    if (BH1750_Lire(&lux))
    {
      mesures.lux = lux;
      mesures.lum_ok = 1;
      lum_fini = 1;
    }
  }

  /* --- GPS --- */
  if (!gps_fini && GPS_Lire_Position())
  {
    mesures.gps_ok = 1;
    gps_fini = 1;
  }

  /* --- Fin : tout est lu, ou TIMEOUT écoulé --- */
  if ((dht_fini && lum_fini && gps_fini) || now - t_debut >= (uint32_t)cfg.timeout * 1000UL)
  {
    Fin_Acquisition();
    return 1;
  }
  return 0;
}

const Mesures *Capteurs_Resultat(void)
{
  return &mesures;
}

const CapteursErreurs *Capteurs_Erreurs(void)
{
  return &erreurs;
}

/* Ligne "T;H;L;GPS" ("NA" quand la valeur manque) */
void Capteurs_Formater(char *buf, uint32_t taille)
{
  char t[8] = "NA", h[8] = "NA", l[8] = "NA";

  if (mesures.temp_ok) snprintf(t, sizeof(t), "%d", mesures.temp);
  if (mesures.hygr_ok) snprintf(h, sizeof(h), "%d", mesures.hygr);
  if (mesures.lum_ok)  snprintf(l, sizeof(l), "%u", mesures.lux);

  snprintf(buf, taille, "%s;%s;%s;%s", t, h, l, mesures.gps_ok ? mesures.gps : "NA");
}
