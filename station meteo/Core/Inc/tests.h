/* Tests composant par composant (codes du guide du prof, adaptés au projet). */
#ifndef TESTS_H
#define TESTS_H

#define TEST_LED_RGB   1  /* LED RGB PA0/PA1/PA4 : rouge, vert, bleu 300 ms */
#define TEST_BOUTONS   2  /* BTN2 A4/PC1 allume le rouge, BTN1 PC13 (EXTI) bascule le bleu */
#define TEST_SERIAL    3  /* printf sur le ST-Link (USART2, 115200) */
#define TEST_DHT11     4  /* DHT11 sur PB0 */
#define TEST_RTC       5  /* RTC interne (LSE) */
#define TEST_BH1750    6  /* Luminosité I2C1 PB8/PB9, adresse 0x23 */
#define TEST_SD        7  /* Carte SD SPI1 + FatFS, CS PB6 */
#define TEST_GPS       8  /* GPS USART3 PC10/PC11, 9600 */
#define TEST_STATION  10  /* Modes de fonctionnement (diagramme) */
#define TEST_LED_IDENT 11 /* Identification des pattes : PA0, PA1, PA4 seules 3 s chacune */
#define TEST_LED_MODES 12 /* 4 modes : couleurs de la LED + boutons, sans capteurs ni SD */

/* >>> Changer cette ligne pour choisir le test, puis recompiler <<< */
#define TEST_MODE TEST_LED_MODES

void Tests_Init(void);
void Tests_Loop(void);

#endif
