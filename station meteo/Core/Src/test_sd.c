/* TP 7 : carte SD en SPI1 + FatFS (SCK = D13, MISO = D12, MOSI = D11, CS = D10/PB6).
 * Écrit test.txt au démarrage puis un compteur dans log.csv chaque seconde.
 * Copie autonome du test de main.c (TEST_SD) : code du guide + diagnostic. */
#include "main.h"
#include "fatfs.h"
#include "test_composants.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;

static FATFS fs;
static FIL fil;
static FRESULT fres;
static UINT bw;

/* Diagnostic SPI brut : CMD0 (reset) ; la carte doit répondre 0x01. 0xFF = rien sur MISO */
static void SD_Diagnostic(void)
{
  uint8_t ff = 0xFF, rx = 0, cmd0[6] = {0x40, 0, 0, 0, 0, 0x95};
  uint8_t repos = 0, rep = 0xFF;

  /* Pull-up sur MISO (PA6, recommandé pour les cartes SD) : si la broche reste à 0,
     quelque chose la tire activement vers GND ; à 1, rien ne la pilote */
  MODIFY_REG(GPIOA->PUPDR, GPIO_PUPDR_PUPD6, GPIO_PUPDR_PUPD6_0);
  HAL_Delay(1);
  printf("[SD] Diagnostic : niveau MISO (D12) avec pull-up = %d\r\n",
         (GPIOA->IDR & GPIO_PIN_6) ? 1 : 0);

  __HAL_SPI_DISABLE(&hspi1);
  MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_256); /* ~300 kHz */
  __HAL_SPI_ENABLE(&hspi1);

  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  for (int i = 0; i < 10; i++) HAL_SPI_TransmitReceive(&hspi1, &ff, &repos, 1, 10);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
  for (int i = 0; i < 6; i++) HAL_SPI_TransmitReceive(&hspi1, &cmd0[i], &rx, 1, 10);
  for (int i = 0; i < 10 && (rep & 0x80); i++) HAL_SPI_TransmitReceive(&hspi1, &ff, &rep, 1, 10);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  HAL_SPI_TransmitReceive(&hspi1, &ff, &rx, 1, 10);

  printf("[SD] Diagnostic : MISO au repos = 0x%02X, reponse CMD0 = 0x%02X -> %s\r\n", repos, rep,
         rep == 0x01 ? "la carte repond (cablage SPI OK)" :
         rep == 0xFF ? "aucune reponse (MISO/SCK/CS/VCC, carte inseree ?)" :
         repos == 0x00 ? "MISO colle a 0 (MISO sur GND ou module non alimente ?)" :
                         "reponse anormale (cablage MOSI/SCK ?)");
}

void Test_SD(void)
{
  printf("========================================\r\n");
  printf("   TP 7 : Carte SD (SPI1 + FatFS)       \r\n");
  printf("========================================\r\n");
  // S'assurer que le CS est bien à l'état haut (désélectionné)
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
  SD_Diagnostic();

  // Tentative de montage de la carte SD
  fres = f_mount(&fs, "0:/", 1);
  if (fres == FR_OK) {
    printf("[SD] Carte montee avec succes !\r\n");

    // Test d'écriture d'un fichier de démarrage
    fres = f_open(&fil, "0:/test.txt", FA_OPEN_ALWAYS | FA_WRITE);
    if (fres == FR_OK) {
      f_puts("Test de demarrage de la carte SD !\r\n", &fil);
      f_close(&fil);
      printf("[SD] Fichier 'test.txt' cree et ecrit avec succes.\r\n");
    } else {
      printf("[SD] Erreur d'ouverture du fichier (Code: %d).\r\n", fres);
    }
  } else {
    printf("[SD] Erreur de montage de la carte SD (Code: %d). Verifiez le cablage ou le formatage (FAT32).\r\n", fres);
  }
  uint32_t counter = 0;
  while (1)
  {
    counter++;

    // Écriture incrémentale d'un compteur dans un fichier CSV
    fres = f_open(&fil, "0:/log.csv", FA_OPEN_ALWAYS | FA_WRITE);
    if (fres == FR_OK) {
      // Se placer à la fin du fichier (Append)
      f_lseek(&fil, f_size(&fil));
      char buffer[32];
      int len = snprintf(buffer, sizeof(buffer), "%lu\r\n", counter);
      f_write(&fil, buffer, len, &bw);
      f_close(&fil);
      printf("[SD] Ligne ecrite : %lu\r\n", counter);
    } else {
      printf("[SD] Erreur d'ecriture log (Code: %d).\r\n", fres);
    }
    HAL_Delay(1000);
  }
}
