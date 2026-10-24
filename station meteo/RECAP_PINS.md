# Station météo : récapitulatif CubeMX (NUCLEO-L476RG)

Fichier : `StationMeteo.ioc` (copié aussi dans `C:\Users\user\Documents\StationMeteo\`).
Ouvrir avec STM32CubeMX (File > Load Project), vérifier, puis **GENERATE CODE** (Toolchain : CMake, déjà réglé).

## Pins

| Étape | Composant | Broche | Mode CubeMX | Label | Réglages |
|---|---|---|---|---|---|
| 1 | Quartz RTC (LSE) | PC14 / PC15 | RCC_OSC32_IN / OUT | | LSE = Crystal/Ceramic Resonator |
| 2 | Bouton 1 (bleu, B1) | PC13 | GPIO_EXTI13 | BTN1 | Front descendant, Pull-up, NVIC EXTI[15:10] activé |
| 3 | Bouton 2 externe | PC1 (A4) | GPIO_Input | BTN2 | Pull-up |
| 4 | LED RGB rouge | PA0 | GPIO_Output | LED_R | Niveau initial Low |
| 4 | LED RGB verte | PA1 | GPIO_Output | LED_G | Niveau initial Low |
| 4 | LED RGB bleue | PA4 | GPIO_Output | LED_B | Niveau initial Low |
| 5 | DHT11 | PB0 | GPIO_Output | DHT11 | Speed High, No pull |
| 6 | Luminosité BH1750 SCL | PB8 | I2C1_SCL | LUX_SCL | I2C standard 100 kHz |
| 6 | Luminosité BH1750 SDA | PB9 | I2C1_SDA | LUX_SDA | |
| 7 | Carte SD SCK | PA5 | SPI1_SCK | SD_SCK | Full-Duplex Master, 8 bits, prescaler 256 (312,5 kbit/s) |
| 7 | Carte SD MISO | PA6 | SPI1_MISO | SD_MISO | |
| 7 | Carte SD MOSI | PA7 | SPI1_MOSI | SD_MOSI | |
| 7 | Carte SD CS | PB6 | GPIO_Output | SD_CS | Niveau initial High (carte désélectionnée) |
| 8 | GPS TX | PC10 | USART3_TX | GPS_TX | Asynchrone 9600 8N1, interruption USART3 activée |
| 8 | GPS RX | PC11 | USART3_RX | GPS_RX | |
| (tests) | Moniteur série (ST-Link) | PA2 / PA3 | USART2_TX / RX | | 115200 8N1 (utilisé par printf / huart2) |
| | Débogage SWD | PA13 / PA14 | SYS Serial Wire | | |

## Périphériques

- **Horloge** : HSI 16 MHz -> PLL (M=1, N=10, R=2) -> SYSCLK **80 MHz**, AHB/APB1/APB2 = 80 MHz.
- **RTC** : activé, calendrier activé, source **LSE 32,768 kHz**.
- **I2C1** : mode I2C, 100 kHz.
- **SPI1** : Full-Duplex Master, NSS désactivé (CS manuel sur PB6).
- **USART3** : asynchrone, 9600 bauds, 8 bits, sans parité, 1 stop.
- **USART2** : asynchrone, 115200 bauds (port COM virtuel du ST-Link).
- **FatFS** : activé en mode *User-defined* (le driver SPI de la carte SD est à écrire dans `user_diskio.c`).
- **Projet** : nom StationMeteo, Toolchain **CMake**.

## Points à connaître (incohérences du guide)

1. **SPI1, pas SPI2** : l'étape 7 parle de SPI2, mais PA5/PA6/PA7 ne sont reliés qu'à SPI1 (le tableau et l'étape 9 disent bien SPI1).
2. **PA5 = LED verte LD2** : PA5 est aussi la LED verte de la carte. Une fois SPI1 actif, LD2 clignote avec l'horloge SPI et les tests du guide qui pilotent `LD2_Pin`/`GPIO_PIN_5` ne doivent plus être utilisés (sinon ils perturbent la carte SD). Utilise plutôt la LED RGB pour les indications.
3. **GPS à 9600 bauds** : la capture d'écran montre 115200 mais le texte de l'étape 8 (et la valeur par défaut des modules GPS) est 9600, c'est ce qui est configuré.
4. **FatFS "SPI"** : CubeMX ne propose pas d'option "SPI" pour FatFS sur STM32L4, seulement *User-defined*. C'est ce mode qui est choisi.
5. **USART2 ajouté** : le guide ne le cite pas dans les étapes 1 à 9 mais tous les tests (printf, `huart2`) en ont besoin.
