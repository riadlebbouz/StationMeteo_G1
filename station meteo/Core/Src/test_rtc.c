/* TP 5 : RTC interne, date et heure affichées chaque seconde sur le Serial Monitor.
 * Copie autonome du test de main.c (TEST_RTC) : code du guide. */
#include "main.h"
#include "test_composants.h"
#include <stdio.h>

extern RTC_HandleTypeDef hrtc;

static void RTC_SetDateTime(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t day, uint8_t month, uint8_t year) {
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  // Configuration de l'heure en format BIN (décimal classique)
  sTime.Hours = hours;
  sTime.Minutes = minutes;
  sTime.Seconds = seconds;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
    Error_Handler();
  }

  // Configuration de la date en format BIN
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = month;
  sDate.Date = day;
  sDate.Year = year; // Ex: 26 pour 2026
  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
    Error_Handler();
  }
}

static void RTC_GetDateTime(void) {
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  // La lecture de l'heure met à jour le calendrier du RTC (doit toujours se faire avant la date)
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  // Affichage formaté sur le moniteur série
  printf("[RTC] Date : %02d/%02d/20%02d | Heure : %02d:%02d:%02d\r\n",
         sDate.Date, sDate.Month, sDate.Year,
         sTime.Hours, sTime.Minutes, sTime.Seconds);
}

void Test_RTC(uint8_t hours, uint8_t minutes, uint8_t seconds,
              uint8_t day, uint8_t month, uint8_t year)
{
  printf("========================================\r\n");
  printf("   TP 5 : Test du RTC interne           \r\n");
  printf("========================================\r\n");
  /* réglé à chaque appel, donc à chaque reset */
  RTC_SetDateTime(hours, minutes, seconds, day, month, year);
  while (1)
  {
    RTC_GetDateTime();
    HAL_Delay(1000);
  }
}
