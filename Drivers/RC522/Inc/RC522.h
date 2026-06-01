#ifndef _MFRC522_H
#define _MFRC522_H

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_spi.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MFRC522_CommandReg = 0x01,
    MFRC522_ComIEnReg = 0x02,
    MFRC522_DivIEnReg = 0x03,
    MFRC522_CommIrqReg = 0x04,
    MFRC522_DivIrqReg = 0x05,
    MFRC522_ErrorReg = 0x06,
    MFRC522_Status1Reg = 0x07,
    MFRC522_Status2Reg = 0x08,
    MFRC522_FIFODataReg = 0x09,
    MFRC522_FIFOLevelReg = 0x0A,
    MFRC522_WaterLevelReg = 0x0B,
    MFRC522_ControlReg = 0x0C,
    MFRC522_BitFramingReg = 0x0D,
    MFRC522_CollReg = 0x0E,

    MFRC522_ModeReg = 0x11,
    MFRC522_TxModeReg = 0x12,
    MFRC522_RxModeReg = 0x13,
    MFRC522_TxControlReg = 0x14,
    MFRC522_TxASKReg = 0x15,
    MFRC522_TxSelReg = 0x16,
    MFRC522_RxSelReg = 0x17,
    MFRC522_RxThresholdReg = 0x18,
    MFRC522_DemodReg = 0x19,
    MFRC522_MfTxReg = 0x1C,
    MFRC522_MfRxReg = 0x1D,
    MFRC522_SerialSpeedReg = 0x1F,

    MFRC522_CRCResultRegH = 0x21,
    MFRC522_CRCResultRegL = 0x22,
    MFRC522_ModWidthReg = 0x24,
    MFRC522_RFCfgReg = 0x26,
    MFRC522_GsNReg = 0x27,
    MFRC522_CWGsPReg = 0x28,
    MFRC522_ModGsPReg = 0x29,
    MFRC522_TModeReg = 0x2A,
    MFRC522_TPrescalerReg = 0x2B,
    MFRC522_TReloadRegH = 0x2C,
    MFRC522_TReloadRegL = 0x2D,
    MFRC522_TxAutoReg = 0x2E,

    MFRC522_TestSel1Reg = 0x31,
    MFRC522_TestSel2Reg = 0x32,
    MFRC522_TestPinEnReg = 0x33,
    MFRC522_TestPinValueReg = 0x34,
    MFRC522_TestBusReg = 0x35,
    MFRC522_AutoTestReg = 0x36,
    MFRC522_VersionReg = 0x37,
    MFRC522_AnalogTestReg = 0x38,
    MFRC522_TestDAC1Reg = 0x39,
    MFRC522_TestDAC2Reg = 0x3A,
    MFRC522_TestADCReg = 0x3B
} MFRC522_Register_t;

typedef enum {
    MFRC522_CMD_Idle = 0x00,
    MFRC522_CMD_Mem = 0x01,
    MFRC522_CMD_GenerateRandomID = 0x02,
    MFRC522_CMD_CalcCRC = 0x03,
    MFRC522_CMD_Transmit = 0x04,
    MFRC522_CMD_NoCmdChange = 0x07,
    MFRC522_CMD_Receive = 0x08,
    MFRC522_CMD_Transceive = 0x0C,
    MFRC522_CMD_MFAuthent = 0x0E,
    MFRC522_CMD_SoftReset = 0x0F
} MFRC522_Command_t;

// https://wg8.de/wg8n1496_17n3613_Ballot_FCD14443-3.pdf or https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf
typedef enum {
    PICC_CMD_REQA = 0x26,
    PICC_CMD_WUPA = 0x52,
    PICC_CMD_CT = 0x88,

    PICC_CMD_HLTA = 0x50,

    PICC_CMD_Sel_CL1 = 0x93,
    PICC_CMD_Sel_CL2 = 0x95,
    PICC_CMD_Sel_CL3 = 0x97,
} PICC_Command_t;

// https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf - MIFARE Classic command: Section 9.1
typedef enum
{
    MIFARE_CMD_AUTH_KEY_A = 0x60,
    MIFARE_CMD_AUTH_KEY_B = 0x61,

    MIFARE_CMD_READ = 0x30,
    MIFARE_CMD_WRITE = 0xA0,
    MIFARE_CMD_DECREMENT = 0xC0,
    MIFARE_CNDMD_INCREMENT = 0xC1,
    MIFARE_CMD_RESTORE = 0xC2,
    MIFARE_CMD_TRANSFER = 0xB0,
} MIFARE_Command_t;

typedef enum {
    MFRC522_OK,
    MFRC522_ERROR,
    MFRC522_COLLISION,
    MFRC522_TIMEOUT,
    MFRC522_INVALID,
    MFRC522_CRC_WRONG,
    MFRC522_NOT_ISO_COMPLIANT,
    MFRC522_NOT_AUTHED,
    MFRC522_NO_ROOM
} MFRC522_Status_t;

typedef enum {
    PICC_TYPE_UNKNOWN = 0XFF,
    PICC_TYPE_ULTRALIGHT_C_CL2 = 0x00,
    PICC_TYPE_MIFARE_1K = 0x08,
    PICC_TYPE_MIFARE_4K = 0x18,
    PICC_TYPE_MIFARE_PLUS_2K = 0X10,
    PICC_TYPE_MIFARE_PLUS_4K = 0x11,
    PICC_TYPE_MIFARE_PLUS_2K_SE = 0x20
} PICC_Type_t;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef * cdPort;
    uint16_t csPin;
    GPIO_TypeDef *rstPort;
    uint16_t rstPin;
    uint8_t uid[4];
    PICC_Type_t piccType;
} MFRC522_Handle_t;

MFRC522_Status_t MFRC522_Init(MFRC522_Handle_t *handle, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cdPort, uint16_t cdPin, GPIO_TypeDef *rstPort, uint16_t rstPin);

/*******************************
    Low level register access
*******************************/
MFRC522_Status_t MFRC522_WriteRegister(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t value);
MFRC522_Status_t MFRC522_WriteRegisterLong(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* data, uint16_t length);
MFRC522_Status_t MFRC522_ReadRegister(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* out);
MFRC522_Status_t MFRC522_ReadRegisterLong(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* out, uint16_t length);
MFRC522_Status_t MFRC522_SetBitMask(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t mask);
MFRC522_Status_t MFRC522_ClearBitMask(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t mask);

/*****************************
    MFRC522 operations
******************************/
MFRC522_Status_t MFRC522_Enable_Antenna(MFRC522_Handle_t *handle);
MFRC522_Status_t MFRC522_Disable_Antenna(MFRC522_Handle_t *handle);
MFRC522_Status_t MFRC522_CalculateCRC(MFRC522_Handle_t *handle, uint8_t* data, uint16_t length, uint8_t* out);
MFRC522_Status_t MFRC522_CheckCRC(MFRC522_Handle_t *handle, uint8_t* data, uint16_t length);
MFRC522_Status_t MFRC522_DisableCrypto1(MFRC522_Handle_t *handle);

/*********************************
    Communication with PIC
**********************************/
MFRC522_Status_t MFRC522_RequestA(MFRC522_Handle_t *handle, uint8_t *bufferATQA);
MFRC522_Status_t MFRC522_AntiCollistion(MFRC522_Handle_t *handle, uint8_t *bufferUID);
MFRC522_Status_t MFRC522_SendREQA(MFRC522_Handle_t *handle);
MFRC522_Status_t MFRC522_Execute_Command(MFRC522_Handle_t *handle, uint8_t cmd, uint8_t *txBuffer, uint16_t txBufferLength, uint8_t bitFraming);
MFRC522_Status_t MFRC522_Tranceive(MFRC522_Handle_t *handle, uint8_t *txBuffer, uint16_t txBufferLength, uint8_t *rxBuffer, uint8_t rxBufferLength);

/*****************************
    MIFARE Classic functions
*****************************/

// https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf - MIFARE Classic Commands: Section 8.6
MFRC522_Status_t MFRC522_MifareAuth(MFRC522_Handle_t *handle, MIFARE_Command_t cmd, uint8_t blockAddr, uint8_t *key, uint8_t *uid);
MFRC522_Status_t MFRC522_MifareRead(MFRC522_Handle_t *handle, uint8_t blockAddr, uint8_t *rxData);
MFRC522_Status_t MFRC522_MifareWrite(MFRC522_Handle_t *handle, uint8_t blockAddr, uint8_t *data);

/**************************
    Convenience functions
**************************/
MFRC522_Status_t MFRC522_IsCardPresent(MFRC522_Handle_t *handle);
MFRC522_Status_t MFRC522_ReadUID(MFRC522_Handle_t *handle);
MFRC522_Status_t MFRC522_Exec_SelfTest(MFRC522_Handle_t *handle, uint8_t *selfTestResult);

#endif /* _MFRC522_H */