/* Les 4 modes de fonctionnement (document "Modes de fonctionnement") :
 * couleur de la LED RGB, boutons (appuis de 5 s), console de configuration.
 *
 * Bouton "rouge" du diagramme = B1 (PC13, bouton bleu de la carte)
 * Bouton "vert"  du diagramme = bouton miniature (A4 / PC1)
 *
 *   Démarrage sans bouton             -> STANDARD      (LED verte)
 *   Démarrage avec rouge enfoncé      -> CONFIGURATION (LED jaune), retour STANDARD après 30 min sans activité
 *   STANDARD    : rouge 5 s           -> MAINTENANCE   (LED orange)
 *   STANDARD    : vert 5 s            -> ECONOMIQUE    (LED bleue)
 *   ECONOMIQUE  : rouge 5 s           -> MAINTENANCE
 *   MAINTENANCE : rouge 5 s           -> mode précédent
 *
 * Mesure : Mesure() lit l'heure RTC et lance l'acquisition (capteurs.c) ; la boucle
 * appelle Capteurs_Tache() jusqu'à la fin, puis Fin_Mesure() écrit la ligne sur la
 * carte SD (logger.c), ou sur le port série en maintenance. */
#include "main.h"
#include "led.h"
#include "led_modes.h"
#include "modes.h"
#include "config.h"
#include "capteurs.h"
#include "logger.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

extern UART_HandleTypeDef huart2;
extern RTC_HandleTypeDef hrtc;

#define APPUI_LONG_MS          5000UL
#define ANTI_REBOND_MS         30UL
#define CONFIG_TIMEOUT_MS      (30UL * 60UL * 1000UL) /* 30 min ; mettre 30000UL pour tester */
#define MAINTENANCE_PERIODE_MS 5000UL                 /* affichage direct des mesures */

/* ---------- Boutons (actifs à l'état bas, pull-up interne) ---------- */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t brut;       /* dernière lecture */
  uint8_t enfonce;    /* état après anti-rebond */
  uint8_t long_fait;  /* appui long déjà signalé pour cet appui */
  uint32_t t_change;
  uint32_t t_appui;
} Bouton;

static Bouton rouge = {.port = BTN1_GPIO_Port, .pin = BTN1_Pin};
static Bouton vert = {.port = BTN2_GPIO_Port, .pin = BTN2_Pin};

static uint8_t Lire(const Bouton *b)
{
  return HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_RESET;
}

/* Renvoie 1 une seule fois, au moment où le bouton atteint 5 s d'appui */
static uint8_t AppuiLong(Bouton *b, uint32_t now)
{
  uint8_t brut = Lire(b);

  if (brut != b->brut)
  {
    b->brut = brut;
    b->t_change = now;
  }
  if (b->enfonce != b->brut && now - b->t_change >= ANTI_REBOND_MS)
  {
    b->enfonce = b->brut;
    printf("[BOUTON] %s %s\r\n", (b->pin == BTN1_Pin) ? "rouge (B1)" : "vert (miniature A4)", b->enfonce ? "appuye" : "relache");
    if (b->enfonce)
    {
      b->t_appui = now;
      b->long_fait = 0;
    }
  }
  if (b->enfonce && !b->long_fait && now - b->t_appui >= APPUI_LONG_MS)
  {
    b->long_fait = 1;
    return 1;
  }
  return 0;
}

/* ---------- Modes ---------- */
static const char *NOMS[] = {"STANDARD", "CONFIGURATION", "ECONOMIQUE", "MAINTENANCE"};

static ModeSysteme mode = MODE_STANDARD;
static ModeSysteme mode_precedent = MODE_STANDARD;
static uint32_t t_activite;      /* dernière activité en configuration */
static uint32_t t_mesure;        /* dernière mesure */
static uint8_t mesure_immediate; /* première mesure dès l'entrée dans le mode */
static uint32_t num_mesure;
static RTC_TimeTypeDef t_horodatage; /* heure du début de la mesure en cours */
static RTC_DateTypeDef d_horodatage;
static SdEtat etat_sd = SD_OK;       /* résultat de la dernière écriture */
static uint8_t erreur_rtc = 0;

static void Changer(ModeSysteme m)
{
  uint32_t now = HAL_GetTick();

  if (mode == MODE_MAINTENANCE && m != MODE_MAINTENANCE)
  {
    etat_sd = Logger_Monter();
    printf(etat_sd == SD_OK ? "Carte SD remontee, enregistrement repris\r\n"
                            : "Carte SD absente ou illisible\r\n");
  }
  if (m == MODE_MAINTENANCE)
  {
    mode_precedent = mode;
    Logger_Demonter();
  }
  Capteurs_Annuler(); /* une mesure en cours est abandonnée au changement de mode */

  mode = m;
  switch (m)
  {
    case MODE_STANDARD:      LED_Couleur(LED_VERTE);  break;
    case MODE_CONFIGURATION: LED_Couleur(LED_JAUNE);  break;
    case MODE_ECONOMIQUE:    LED_Couleur(LED_BLEUE);  break;
    case MODE_MAINTENANCE:   LED_Couleur(LED_ORANGE); break;
  }

  printf("\r\n=== Mode %s ===\r\n", NOMS[m]);
  if (m == MODE_CONFIGURATION)
  {
    t_activite = now;
    printf("Acquisition desactivee. Tapez HELP pour la liste des commandes.\r\n> ");
  }
  if (m == MODE_MAINTENANCE)
  {
    printf("Carte SD demontee : retrait possible. Mesures affichees ici.\r\n");
  }

  t_mesure = now;
  mesure_immediate = 1;
}

/* Intervalle entre 2 mesures selon le mode */
static uint32_t Periode(void)
{
  uint32_t p = (uint32_t)cfg.log_interval * (LOG_INTERVAL_EN_SECONDES ? 1000UL : 60000UL);

  if (mode == MODE_ECONOMIQUE) p *= 2;              /* LOG_INTERVAL x 2 */
  if (mode == MODE_MAINTENANCE) p = MAINTENANCE_PERIODE_MS;
  return p;
}

/* Choisit le code d'erreur à afficher sur la LED (le plus grave d'abord) */
static void Erreurs_LED(void)
{
  const CapteursErreurs *e = Capteurs_Erreurs();

  if (erreur_rtc)                 LED_Erreur(LED_ERR_RTC);
  else if (etat_sd == SD_ERREUR)  LED_Erreur(LED_ERR_SD);
  else if (etat_sd == SD_PLEINE)  LED_Erreur(LED_ERR_SD_PLEINE);
  else if (e->capteur)            LED_Erreur(LED_ERR_CAPTEUR);
  else if (e->gps)                LED_Erreur(LED_ERR_GPS);
  else if (e->incoherent)         LED_Erreur(LED_ERR_INCOHERENT);
  else                            LED_Erreur(LED_ERR_AUCUNE);
}

/* Début d'une mesure : horodatage puis lancement de l'acquisition */
static void Mesure(void)
{
  uint8_t avec_gps = 1;

  num_mesure++;
  if (mode == MODE_ECONOMIQUE) avec_gps = (num_mesure % 2) == 0; /* GPS une mesure sur deux */

  /* l'heure doit être lue avant la date */
  if (HAL_RTC_GetTime(&hrtc, &t_horodatage, RTC_FORMAT_BIN) != HAL_OK ||
      HAL_RTC_GetDate(&hrtc, &d_horodatage, RTC_FORMAT_BIN) != HAL_OK)
  {
    erreur_rtc = 1;
    Erreurs_LED();
    printf("Erreur d'acces a l'horloge RTC\r\n");
    return;
  }
  erreur_rtc = 0;

  Capteurs_Demarrer(avec_gps);
}

/* Fin d'une mesure : ligne "JJ/MM/AAAA HH:MM:SS;T;H;L;GPS" vers la carte SD ou le port série */
static void Fin_Mesure(void)
{
  char ligne[96];
  int n;

  n = snprintf(ligne, sizeof(ligne), "%02d/%02d/20%02d %02d:%02d:%02d;",
               d_horodatage.Date, d_horodatage.Month, d_horodatage.Year,
               t_horodatage.Hours, t_horodatage.Minutes, t_horodatage.Seconds);
  Capteurs_Formater(ligne + n, sizeof(ligne) - n - 2);
  strcat(ligne, "\r\n");

  if (mode == MODE_MAINTENANCE)
  {
    printf("%s", ligne); /* carte SD démontée : affichage direct */
  }
  else
  {
    etat_sd = Logger_Ecrire(ligne, d_horodatage.Year, d_horodatage.Month, d_horodatage.Date);
    printf("[%s] %s", etat_sd == SD_OK ? "SD" : (etat_sd == SD_PLEINE ? "SD PLEINE" : "SD ERREUR"), ligne);
  }
  Erreurs_LED();
}

/* ---------- Console de configuration (USART2, lue sans bloquer) ---------- */
static char ligne[64];
static uint8_t lg = 0;

static void Console(uint32_t now)
{
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) __HAL_UART_CLEAR_OREFLAG(&huart2);

  while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
  {
    char c = (char)(huart2.Instance->RDR & 0xFF);
    t_activite = now;

    if (c == '\r' || c == '\n')
    {
      if (lg > 0)
      {
        ligne[lg] = '\0';
        lg = 0;
        printf("\r\n");
        if (!Config_Commande(ligne)) printf("Commande inconnue (HELP pour la liste)\r\n");
        printf("> ");
      }
    }
    else if ((c == '\b' || c == 127) && lg > 0)
    {
      lg--;
      printf("\b \b");
    }
    else if (c >= ' ' && lg < sizeof(ligne) - 1)
    {
      ligne[lg++] = (char)toupper((unsigned char)c);
      putchar(c);
    }
  }
}

/* ---------- Points d'entrée (appelés dans main.c) ---------- */
void LedModes_Init(void)
{
  uint32_t now;

  setvbuf(stdout, NULL, _IONBF, 0); /* printf sans tampon (écho de la console) */
  HAL_DBGMCU_EnableDBGSleepMode();  /* le débogueur (ST-Link, VS Code) reste connectable pendant la veille __WFI() */
  Config_Charger();
  Capteurs_Init();
  LED_Activer();
  HAL_Delay(50); /* laisse les pull-up se stabiliser avant de lire les boutons */

  now = HAL_GetTick();
  rouge.brut = rouge.enfonce = Lire(&rouge);
  vert.brut = vert.enfonce = Lire(&vert);
  rouge.t_change = vert.t_change = now;
  rouge.long_fait = vert.long_fait = 1; /* l'appui du démarrage ne compte pas comme appui long */

  printf("\r\nEtat au demarrage : rouge (B1)=%s, vert (miniature A4)=%s\r\n", rouge.enfonce ? "appuye" : "relache", vert.enfonce ? "appuye" : "relache");
  printf("\r\nStation meteo %s\r\n", VERSION_PROGRAMME);
  if (!rouge.enfonce)
  {
    etat_sd = Logger_Monter();
    if (etat_sd != SD_OK) printf("Carte SD absente ou illisible\r\n");
  }
  Changer(rouge.enfonce ? MODE_CONFIGURATION : MODE_STANDARD);
}

void LedModes_Loop(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t long_rouge = AppuiLong(&rouge, now); /* B1 */
  uint8_t long_vert = AppuiLong(&vert, now);  /* bouton miniature */

  Capteurs_GPS_Horloge(); /* heure automatique depuis le GPS après une coupure de courant */

  switch (mode)
  {
    case MODE_CONFIGURATION:
      Console(now);
      if (rouge.enfonce || vert.enfonce) t_activite = now;
      if (now - t_activite >= CONFIG_TIMEOUT_MS)
      {
        printf("\r\n30 min sans activite\r\n");
        Changer(MODE_STANDARD);
      }
      return; /* pas d'acquisition en configuration */

    case MODE_STANDARD:
      if (long_rouge)     Changer(MODE_MAINTENANCE);
      else if (long_vert) Changer(MODE_ECONOMIQUE);
      break;

    case MODE_ECONOMIQUE:
      if (long_rouge)   Changer(MODE_MAINTENANCE);
      break;

    case MODE_MAINTENANCE:
      if (long_rouge)   Changer(mode_precedent);
      break;
  }

  if (Capteurs_EnCours())
  {
    if (Capteurs_Tache()) Fin_Mesure();
  }
  else if (mesure_immediate || now - t_mesure >= Periode())
  {
    mesure_immediate = 0;
    t_mesure = now;
    Mesure();
  }

  /* Économie d'énergie : entre deux mesures, le processeur dort jusqu'à la prochaine
   * interruption (SysTick toutes les 1 ms : LED et boutons ; réception GPS). Pas de
   * veille pendant une mesure (capteurs lus en scrutation) ni en configuration
   * (console lue sans interruption, on a quitté la fonction plus haut). */
  if (!Capteurs_EnCours()) __WFI();
}
