/* Tests des TP du guide, un fichier par test dans Core/Src (test_serial.c, test_dht11.c, ...).
 * Chaque fonction affiche sa bannière puis boucle à l'infini (elle ne revient jamais).
 * Prérequis : périphériques initialisés par main() (MX_..._Init) et printf redirigé
 * vers USART2 par __io_putchar (main.c).
 * Exemple dans main(), USER CODE 2 :  Test_DHT11();
 */
#ifndef TEST_COMPOSANTS_H
#define TEST_COMPOSANTS_H

#include <stdint.h>

void Test_LED(void);     /* LED RGB : rouge, vert, bleu, éteinte (1 s chacune) */
void Test_Bouton(void);  /* bouton miniature A4 -> LED verte, B1 -> LED rouge */
void Test_Serial(void);  /* TP 1 : printf + clignotement LED RGB verte */
void Test_DHT11(void);   /* TP 4 : DHT11 sur PB0 (A3) */
void Test_RTC(uint8_t hours, uint8_t minutes, uint8_t seconds,
              uint8_t day, uint8_t month, uint8_t year); /* TP 5 : RTC interne, année 26 = 2026 */
void Test_BH1750(void);  /* TP 6 : luminosité I2C1 (D15/D14), adresse 0x23 */
void Test_SD(void);      /* TP 7 : carte SD SPI1 + FatFS (CS = D10/PB6) */
void Test_GPS(void);     /* TP 8 : GPS USART1 (D8 TX / D2 RX), 9600 bauds, verdict chaque seconde */

#endif
