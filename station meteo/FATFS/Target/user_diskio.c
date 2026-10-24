/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#include "main.h"

/* Driver carte SD en mode SPI (hspi1, CS = SD_CS PB6 actif bas) */

/* Commandes SD */
#define CMD0    (0)           /* GO_IDLE_STATE */
#define CMD1    (1)           /* SEND_OP_COND (MMC) */
#define ACMD41  (0x80 + 41)   /* SEND_OP_COND (SDC) */
#define CMD8    (8)           /* SEND_IF_COND */
#define CMD9    (9)           /* SEND_CSD */
#define CMD12   (12)          /* STOP_TRANSMISSION */
#define CMD16   (16)          /* SET_BLOCKLEN */
#define CMD17   (17)          /* READ_SINGLE_BLOCK */
#define CMD18   (18)          /* READ_MULTIPLE_BLOCK */
#define ACMD23  (0x80 + 23)   /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24   (24)          /* WRITE_BLOCK */
#define CMD25   (25)          /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (55)          /* APP_CMD */
#define CMD58   (58)          /* READ_OCR */

/* Type de carte */
#define CT_MMC    0x01
#define CT_SD1    0x02
#define CT_SD2    0x04
#define CT_BLOCK  0x08        /* Adressage par bloc (SDHC/SDXC) */

#define SD_SPI_TIMEOUT 100

extern SPI_HandleTypeDef hspi1;

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
static BYTE CardType;

/* ---------- Couche SPI ---------- */
static void SD_SetSpeed(uint32_t prescaler)
{
  __HAL_SPI_DISABLE(&hspi1);
  hspi1.Init.BaudRatePrescaler = prescaler;
  MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, prescaler);
  __HAL_SPI_ENABLE(&hspi1);
}

static BYTE SD_XferByte(BYTE data)
{
  BYTE rx = 0xFF;
  HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, SD_SPI_TIMEOUT);
  return rx;
}

static void SD_RxBlock(BYTE *buff, UINT len)
{
  while (len--) *buff++ = SD_XferByte(0xFF);
}

static void SD_TxBlock(const BYTE *buff, UINT len)
{
  while (len--) SD_XferByte(*buff++);
}

/* Attend que la carte soit prête (MISO = 0xFF) ; 1 = prête, 0 = timeout */
static int SD_WaitReady(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  BYTE d;
  do {
    d = SD_XferByte(0xFF);
  } while (d != 0xFF && (HAL_GetTick() - start) < timeout_ms);
  return (d == 0xFF);
}

static void SD_Deselect(void)
{
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  SD_XferByte(0xFF); /* Un cycle d'horloge pour libérer MISO */
}

static int SD_Select(void)
{
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
  SD_XferByte(0xFF);
  if (SD_WaitReady(500)) return 1;
  SD_Deselect();
  return 0;
}

/* Reçoit un bloc de données (jeton 0xFE) */
static int SD_RxDataBlock(BYTE *buff, UINT len)
{
  BYTE token;
  uint32_t start = HAL_GetTick();
  do {
    token = SD_XferByte(0xFF);
  } while (token == 0xFF && (HAL_GetTick() - start) < 200);
  if (token != 0xFE) return 0;
  SD_RxBlock(buff, len);
  SD_XferByte(0xFF); /* CRC ignoré */
  SD_XferByte(0xFF);
  return 1;
}

#if _USE_WRITE == 1
/* Envoie un bloc de 512 octets avec le jeton donné (0xFE, 0xFC, ou 0xFD = stop) */
static int SD_TxDataBlock(const BYTE *buff, BYTE token)
{
  if (!SD_WaitReady(500)) return 0;
  SD_XferByte(token);
  if (token != 0xFD)
  {
    SD_TxBlock(buff, 512);
    SD_XferByte(0xFF); /* CRC factice */
    SD_XferByte(0xFF);
    if ((SD_XferByte(0xFF) & 0x1F) != 0x05) return 0; /* Données refusées */
  }
  return 1;
}
#endif

/* Envoie une commande ; renvoie la réponse R1 (0xFF = pas de réponse) */
static BYTE SD_SendCmd(BYTE cmd, DWORD arg)
{
  BYTE n, res;

  if (cmd & 0x80) /* ACMD<n> = CMD55 + CMD<n> */
  {
    cmd &= 0x7F;
    res = SD_SendCmd(CMD55, 0);
    if (res > 1) return res;
  }

  /* Sélection de la carte (sauf pour CMD12). Pour CMD0, pas d'attente de MISO = 0xFF :
     après une mise sous tension la carte n'est pas encore en mode SPI et ce module
     laisse MISO à 0, l'attente échouerait et CMD0 ne serait jamais envoyée. */
  if (cmd == CMD0)
  {
    SD_Deselect();
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
    SD_XferByte(0xFF);
  }
  else if (cmd != CMD12)
  {
    SD_Deselect();
    if (!SD_Select()) return 0xFF;
  }

  SD_XferByte(0x40 | cmd);
  SD_XferByte((BYTE)(arg >> 24));
  SD_XferByte((BYTE)(arg >> 16));
  SD_XferByte((BYTE)(arg >> 8));
  SD_XferByte((BYTE)arg);
  n = 0x01;                     /* CRC factice + stop */
  if (cmd == CMD0) n = 0x95;    /* CRC valide pour CMD0(0) */
  if (cmd == CMD8) n = 0x87;    /* CRC valide pour CMD8(0x1AA) */
  SD_XferByte(n);

  if (cmd == CMD12) SD_XferByte(0xFF); /* Octet parasite après CMD12 */
  n = 10;
  do {
    res = SD_XferByte(0xFF);
  } while ((res & 0x80) && --n);
  return res;
}

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
  BYTE n, ty, ocr[4];
  uint32_t start;

  if (pdrv) return STA_NOINIT;

  /* Initialisation à basse vitesse (< 400 kHz) : prescaler 256 -> ~312 kHz */
  SD_SetSpeed(SPI_BAUDRATEPRESCALER_256);

  /* Au moins 74 cycles d'horloge avec CS haut */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  for (n = 10; n; n--) SD_XferByte(0xFF);

  ty = 0;
  /* Pull-up sur MISO (PA6), recommandé pour les cartes SD */
  MODIFY_REG(GPIOA->PUPDR, GPIO_PUPDR_PUPD6, GPIO_PUPDR_PUPD6_0);
  /* La 1re commande CMD0 après la mise sous tension (ou une longue pause) est souvent
     mal reçue par le module : on la renvoie jusqu'à 10 fois */
  for (n = 10; n && SD_SendCmd(CMD0, 0) != 1; n--) SD_Deselect();
  if (n) /* Passage en mode idle réussi */
  {
    start = HAL_GetTick();
    if (SD_SendCmd(CMD8, 0x1AA) == 1) /* SD v2 */
    {
      for (n = 0; n < 4; n++) ocr[n] = SD_XferByte(0xFF);
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) /* Carte compatible 2.7-3.6 V */
      {
        /* ACMD41 avec HCS jusqu'à sortie du mode idle (1 s max) */
        while ((HAL_GetTick() - start) < 1000 && SD_SendCmd(ACMD41, 1UL << 30));
        if ((HAL_GetTick() - start) < 1000 && SD_SendCmd(CMD58, 0) == 0)
        {
          for (n = 0; n < 4; n++) ocr[n] = SD_XferByte(0xFF);
          ty = (ocr[0] & 0x40) ? (CT_SD2 | CT_BLOCK) : CT_SD2; /* SDHC/SDXC ou SD v2 */
        }
      }
    }
    else /* SD v1 ou MMC */
    {
      BYTE cmd;
      if (SD_SendCmd(ACMD41, 0) <= 1) { ty = CT_SD1; cmd = ACMD41; }
      else                            { ty = CT_MMC; cmd = CMD1; }
      while ((HAL_GetTick() - start) < 1000 && SD_SendCmd(cmd, 0));
      /* Taille de bloc fixée à 512 octets */
      if ((HAL_GetTick() - start) >= 1000 || SD_SendCmd(CMD16, 512) != 0) ty = 0;
    }
  }
  CardType = ty;
  SD_Deselect();

  if (ty)
  {
    Stat &= ~STA_NOINIT;
    SD_SetSpeed(SPI_BAUDRATEPRESCALER_16); /* ~5 MHz une fois la carte initialisée */
  }
  else
  {
    Stat = STA_NOINIT;
  }
  return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
  if (pdrv) return STA_NOINIT;
  return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
  if (pdrv || !count) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  if (!(CardType & CT_BLOCK)) sector *= 512; /* Adressage en octets (SDSC) */

  if (count == 1)
  {
    if (SD_SendCmd(CMD17, sector) == 0 && SD_RxDataBlock(buff, 512)) count = 0;
  }
  else
  {
    if (SD_SendCmd(CMD18, sector) == 0)
    {
      do {
        if (!SD_RxDataBlock(buff, 512)) break;
        buff += 512;
      } while (--count);
      SD_SendCmd(CMD12, 0); /* Fin de lecture multiple */
    }
  }
  SD_Deselect();
  return count ? RES_ERROR : RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
  if (pdrv || !count) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  if (Stat & STA_PROTECT) return RES_WRPRT;

  if (!(CardType & CT_BLOCK)) sector *= 512; /* Adressage en octets (SDSC) */

  if (count == 1)
  {
    if (SD_SendCmd(CMD24, sector) == 0 && SD_TxDataBlock(buff, 0xFE)) count = 0;
  }
  else
  {
    if (CardType & (CT_SD1 | CT_SD2)) SD_SendCmd(ACMD23, count); /* Pré-effacement */
    if (SD_SendCmd(CMD25, sector) == 0)
    {
      do {
        if (!SD_TxDataBlock(buff, 0xFC)) break;
        buff += 512;
      } while (--count);
      if (!SD_TxDataBlock(NULL, 0xFD)) count = 1; /* Jeton de fin */
    }
  }
  SD_Deselect();
  return count ? RES_ERROR : RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
  DRESULT res = RES_ERROR;
  BYTE n, csd[16];
  DWORD csize;

  if (pdrv) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd)
  {
  case CTRL_SYNC: /* Attend la fin des écritures internes */
    if (SD_Select()) res = RES_OK;
    break;

  case GET_SECTOR_COUNT: /* Nombre de secteurs (lecture du CSD) */
    if (SD_SendCmd(CMD9, 0) == 0 && SD_RxDataBlock(csd, 16))
    {
      if ((csd[0] >> 6) == 1) /* CSD v2.0 (SDHC/SDXC) */
      {
        csize = csd[9] + ((DWORD)csd[8] << 8) + ((DWORD)(csd[7] & 0x3F) << 16) + 1;
        *(DWORD *)buff = csize << 10;
      }
      else /* CSD v1.0 (SDSC/MMC) */
      {
        n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
        *(DWORD *)buff = csize << (n - 9);
      }
      res = RES_OK;
    }
    break;

  case GET_SECTOR_SIZE:
    *(WORD *)buff = 512;
    res = RES_OK;
    break;

  case GET_BLOCK_SIZE: /* Taille du bloc d'effacement en secteurs */
    *(DWORD *)buff = 1; /* Inconnu : 1 (valeur sûre) */
    res = RES_OK;
    break;

  default:
    res = RES_PARERR;
  }

  SD_Deselect();
  return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

