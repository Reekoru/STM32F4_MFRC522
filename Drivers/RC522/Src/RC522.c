#include "RC522.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include <stdint.h>
#include <string.h>
#include "utils.h"
const uint8_t mfrc522_v2_self_test[64] = {
    0x00, 0xEB, 0x66, 0xBA, 0x57, 0xBF, 0x23, 0x95,
    0xD0, 0xE3, 0x0D, 0x3D, 0x27, 0x89, 0x5C, 0xDE,
    0x9D, 0x3B, 0xA7, 0x00, 0x21, 0x5B, 0x89, 0x82,
    0x51, 0x3A, 0xEB, 0x02, 0x0C, 0xA5, 0x00, 0x49,
    0x7C, 0x84, 0x4D, 0xB3, 0xCC, 0xD2, 0x1B, 0x81,
    0x5D, 0x48, 0x76, 0xD5, 0x71, 0x61, 0x21, 0xA9,
    0x86, 0x96, 0x83, 0x38, 0xCF, 0x9D, 0x5B, 0x6D,
    0xDC, 0x15, 0xBA, 0x3E, 0x7D, 0x95, 0x3B, 0x2F
};

uint8_t atqa[2];

MFRC522_Status_t MFRC522_Init(MFRC522_Handle_t *handle, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cdPort, uint16_t cdPin, GPIO_TypeDef *rstPort, uint16_t rstPin)
{
    if (handle == NULL || hspi == NULL || cdPort == NULL || rstPort == NULL) {
        return MFRC522_ERROR;
    }

    handle->hspi = hspi;
    handle->cdPort = cdPort;
    handle->csPin = cdPin;
    handle->rstPort = rstPort;
    handle->rstPin = rstPin;

    // Reset the MFRC522
    HAL_GPIO_WritePin(handle->rstPort, handle->rstPin, GPIO_PIN_RESET); 
    HAL_Delay(50);
    HAL_GPIO_WritePin(handle->rstPort, handle->rstPin, GPIO_PIN_SET);
    HAL_Delay(50);

    // Soft Reset
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_SoftReset);
    HAL_Delay(50);

    // Clear interrupts
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);

    // Flush FIFO
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);

    // Reset baud rates and modulation width
    MFRC522_WriteRegister(handle, MFRC522_TxModeReg, 0x00);
    MFRC522_WriteRegister(handle, MFRC522_RxModeReg, 0x00);
    MFRC522_WriteRegister(handle, MFRC522_ModWidthReg, 0x26);

    // Timer: ~25ms timeout
    MFRC522_WriteRegister(handle, MFRC522_TModeReg, 0x80);      // TAuto = 1
    MFRC522_WriteRegister(handle, MFRC522_TPrescalerReg, 0xA9); // f_timer = 40kHz
    MFRC522_WriteRegister(handle, MFRC522_TReloadRegH, 0x03);   // 1000 ticks = 25ms
    MFRC522_WriteRegister(handle, MFRC522_TReloadRegL, 0xE8);

    // RF settings
    MFRC522_WriteRegister(handle, MFRC522_TxASKReg, 0x40);      // 100% ASK modulation
    MFRC522_WriteRegister(handle, MFRC522_ModeReg, 0x3D);       // CRC preset 0x6363
    MFRC522_WriteRegister(handle, MFRC522_RFCfgReg, 0x7F);      // Max gain (48dB)
    MFRC522_WriteRegister(handle, MFRC522_DemodReg, 0x4D);      // Sensitivity for clones

    MFRC522_Enable_Antenna(handle);
    HAL_Delay(10);

    // Other stuff to implement...
    return MFRC522_OK;
}

HAL_StatusTypeDef MFRC522_WriteRegister(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t value)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t regAddr = ((uint8_t)reg << 1U) & 0x7EU;

    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL)) {
        return HAL_ERROR;
    }

    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_RESET);
    ret = HAL_SPI_Transmit(handle->hspi, &regAddr, 1, HAL_MAX_DELAY);
    if(ret == HAL_OK) {
        ret = HAL_SPI_Transmit(handle->hspi, &value, 1, HAL_MAX_DELAY);
    }
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    return ret;
}

HAL_StatusTypeDef MFRC522_WriteRegisterLong(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* data, uint16_t length)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t regAddr = ((uint8_t)reg << 1U) & 0x7EU;

    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL) || (data == NULL)) {
        return HAL_ERROR;
    }

    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_RESET);
    ret = HAL_SPI_Transmit(handle->hspi, &regAddr, 1, HAL_MAX_DELAY);
    if(ret == HAL_OK) {
        ret = HAL_SPI_Transmit(handle->hspi, data, length, HAL_MAX_DELAY);
    }
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    return ret;
}

HAL_StatusTypeDef MFRC522_ReadRegister(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* out)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t regAddr = (((uint8_t)reg << 1U) & 0x7EU) | 0x80U;

    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL) || (out == NULL)) {
        return HAL_ERROR;
    }

    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_RESET);
    ret = HAL_SPI_Transmit(handle->hspi, &regAddr, 1, HAL_MAX_DELAY);
    if(ret == HAL_OK) {
        ret = HAL_SPI_Receive(handle->hspi, out, 1, HAL_MAX_DELAY);
    }
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    return ret;
}

HAL_StatusTypeDef MFRC522_ReadRegisterLong(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* out, uint16_t length)
{
    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL) || (out == NULL) || (length == 0U)) {
        return HAL_ERROR;
    }

    uint8_t regAddr = (((uint8_t)reg << 1U) & 0x7EU) | 0x80U;
    uint16_t txLen = length + 1U;

    uint8_t txBuf[txLen];
    uint8_t rxBuf[txLen];

    for (uint16_t i = 0; i < length; i++) {
        txBuf[i] = regAddr;
    }
    txBuf[length] = 0x00U;  // final 0x00 to stop

    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_RESET);
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(handle->hspi, txBuf, rxBuf, txLen, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    if (ret == HAL_OK) {
        // rxBuf[0] is garbage (X), data starts at rxBuf[1]
        for (uint16_t i = 0; i < length; i++) {
            out[i] = rxBuf[i + 1U];
        }
    }

    return ret;
}

HAL_StatusTypeDef MFRC522_SetBitMask(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t mask)
{
    uint8_t value = 0;
    HAL_StatusTypeDef ret = MFRC522_ReadRegister(handle, reg, &value);
    if (ret != HAL_OK) {
        return ret;
    }

    value |= mask;
    return MFRC522_WriteRegister(handle, reg, value);
}

HAL_StatusTypeDef MFRC522_ClearBitMask(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t mask)
{
    uint8_t value = 0;
    HAL_StatusTypeDef ret = MFRC522_ReadRegister(handle, reg, &value);
    if (ret != HAL_OK) {
        return ret;
    }

    value &= (uint8_t)~mask;
    return MFRC522_WriteRegister(handle, reg, value);
}

MFRC522_Status_t MFRC522_Enable_Antenna(MFRC522_Handle_t *handle)
{
    HAL_StatusTypeDef ret = MFRC522_SetBitMask(handle, MFRC522_TxControlReg, 0x03U);
    if (ret == HAL_OK) {
        DEBUG_LOG("Antenna On\r\n");
    } else {
        DEBUG_LOG("Failed to enable antenna\r\n");
    }

    return ret;
}

MFRC522_Status_t MFRC522_Disable_Antenna(MFRC522_Handle_t *handle)
{
    HAL_StatusTypeDef ret = MFRC522_ClearBitMask(handle, MFRC522_TxControlReg, 0x03U);
    if (ret == HAL_OK) {
        DEBUG_LOG("Antenna Off\r\n");
    } else {
        DEBUG_LOG("Failed to disable antenna\r\n");
    }

    return ret;
}

MFRC522_Status_t MFRC522_CalculateCRC(MFRC522_Handle_t *handle, uint8_t *data, uint16_t length, uint8_t *out)
{
    MFRC522_Status_t status = MFRC522_OK;
    uint8_t n = 0;

    if ((handle == NULL) || (data == NULL) || (out == NULL)) {
        return MFRC522_ERROR;
    }

    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_DivIrqReg, 0x04); // Clear CRCIRq
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80); // Flush FIFO

    for (n = 0; n < length; n++) {
        MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, data[n]);
    }
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_CalcCRC);

    uint32_t timeout = HAL_GetTick() + 50U;
    while (1) {
        uint8_t irq = 0;
        MFRC522_ReadRegister(handle, MFRC522_DivIrqReg, &irq);
        if (irq & 0x04U) { // CRCIRq bit set - calculation done
            break;
        }
        if (HAL_GetTick() > timeout) {
            DEBUG_LOG("Timeout calculating CRC\r\n");
            status = MFRC522_TIMEOUT;
            goto CalculateCRC_End;
        }
    }

    // CRC result is in FIFO
    if (MFRC522_ReadRegister(handle, MFRC522_FIFODataReg, &out[0]) != HAL_OK ||
        MFRC522_ReadRegister(handle, MFRC522_FIFODataReg, &out[1]) != HAL_OK) {
        status = MFRC522_ERROR;
        goto CalculateCRC_End;
    }

    CalculateCRC_End:
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    return status;
}


/***********************************************************
 PICC functions
***********************************************************/
MFRC522_Status_t MFRC522_RequestA(MFRC522_Handle_t *handle, uint8_t *bufferATQA)
{
    MFRC522_Status_t status = MFRC522_OK;
    DEBUG_LOG("Request A\r\n");

    if ((handle == NULL) || (bufferATQA == NULL)) {
        return MFRC522_INVALID;
    }
    
    MFRC522_Enable_Antenna(handle);

    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO
    MFRC522_ClearBitMask(handle, MFRC522_CollReg, 0x80);         // ValuesAfterColl = 0

    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, PICC_CMD_REQA);
    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, 0x07);  // 7 bits for REQA

    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Transceive);
    MFRC522_SetBitMask(handle, MFRC522_BitFramingReg, 0x80); // Transmit REQA Command and receive response

    uint8_t out = 0;
    uint32_t timeout = HAL_GetTick() + 50U;
    while(1)
    {
            MFRC522_ReadRegister(handle, MFRC522_CommIrqReg, &out);
            if (out & 0x30U) { // RxIRq or IdleIRq
                break;
            }

            if (HAL_GetTick() > timeout || (out & 0x01U)) { // Timeout
                MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);
                DEBUG_LOG("Timeout waiting for card\r\n");
                status = MFRC522_TIMEOUT;
                goto RequestA_End;
            }
    }

    MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);

    uint8_t err = 0;
    MFRC522_ReadRegister(handle, MFRC522_ErrorReg, &err);
    if (err & 0x13U ) // BufferOvfl, ParityErr, ProtocolErr
    { 
        DEBUG_LOG("Error has occured: 0x%02X\r\n", err);
        status = MFRC522_ERROR;
        goto RequestA_End;
    }

    if (err & 0x08U) { // Collision
        status = MFRC522_COLLISION;
        goto RequestA_End;
    }

    uint8_t fifoLevel = 0;
    uint8_t validBits = 0;
    MFRC522_ReadRegister(handle, MFRC522_FIFOLevelReg, &fifoLevel);
    MFRC522_ReadRegister(handle, MFRC522_ControlReg, &validBits);
    validBits &= 0x07U;

    if ((fifoLevel == 2U) && (validBits == 0U))
    {
        MFRC522_ReadRegister(handle, MFRC522_FIFODataReg, &bufferATQA[0]);
        MFRC522_ReadRegister(handle, MFRC522_FIFODataReg, &bufferATQA[1]);
        DEBUG_LOG("ATQA: 0x%02X 0x%02X\r\n", bufferATQA[0], bufferATQA[1]);
        goto RequestA_End;
    }

    status = MFRC522_ERROR;

    RequestA_End:
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);

    if( status != MFRC522_OK) 
    {
        DEBUG_LOG("RequestA failed...");
        MFRC522_Disable_Antenna(handle);
    }
    return status;
};

MFRC522_Status_t MFRC522_AntiCollision(MFRC522_Handle_t *handle, uint8_t *serNum)
{
    MFRC522_Status_t status = MFRC522_OK;
    DEBUG_LOG("Anti Collision\r\n");
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO

    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, PICC_CMD_Sel_CL1); // 1) Assign SEL cascade level
    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, 0x20); // 2) assign NVB
    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, 0x00);  // 7 bits for REQA

    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Transceive);
    MFRC522_SetBitMask(handle, MFRC522_BitFramingReg, 0x80); // 3) Transmite SEL and NVB

    // 4) PICCs respond with UID CLn (cascade level n) and BCC (Block Check Character)

    // Wait for response
    uint32_t timeout = HAL_GetTick() + 100U;
    uint8_t out = 0;
    while(1)
    {
        MFRC522_ReadRegister(handle, MFRC522_CommIrqReg, &out);
        if (out & 0x30U) { // RxIRq or IdleIRq
            break;
        }

        if (HAL_GetTick() > timeout || (out & 0x01U)) { // Timeout
            MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);
            DEBUG_LOG("Timeout waiting for card\r\n");
            status = MFRC522_TIMEOUT;
            goto AntiCollision_End;
        }
    }

    MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);

    uint8_t err = 0;
    MFRC522_ReadRegister(handle, MFRC522_ErrorReg, &err);
    if (err & 0x13U ) // BufferOvfl, ParityErr, ProtocolErr
    {
        DEBUG_LOG("Error has occured: 0x%02X\r\n", err);
        status = MFRC522_ERROR;
        goto AntiCollision_End;
    }

    if (err & 0x08U) { // Collision
        status = MFRC522_COLLISION;
        goto AntiCollision_End;
    }

    uint8_t fifoLevel = 0;
    MFRC522_ReadRegister(handle, MFRC522_FIFOLevelReg, &fifoLevel);
    if (fifoLevel != 5U) // 4 bytes UID + 1 byte BCC
    {
        DEBUG_LOG("Unexpected FIFO level: %d\r\n", fifoLevel);
        status = MFRC522_ERROR;
        goto AntiCollision_End;
    }

    // Read UID + BCC from FIFO
    for (uint8_t i = 0; i < 5U; i++) {
        MFRC522_ReadRegister(handle, MFRC522_FIFODataReg, &serNum[i]);
    }

    // Check BCC
    uint8_t bcc = serNum[0] ^ serNum[1] ^ serNum[2] ^ serNum[3];
    if (bcc != serNum[4]) {
        DEBUG_LOG("BCC check failed. Received BCC: 0x%02X, Calculated BCC: 0x%02X\r\n", serNum[4], bcc);
        status = MFRC522_ERROR;
        goto AntiCollision_End;
    }

    // ... 11 & 12) PCD assign NVB 0x70U, calculate CRC_A and transmit
    uint8_t selectBuffer[7] = {0};
    selectBuffer[0] = PICC_CMD_Sel_CL1;      // Cascade Level 1
    selectBuffer[1] = 0x70;      // NVB 
    selectBuffer[2] = serNum[0];
    selectBuffer[3] = serNum[1];
    selectBuffer[4] = serNum[2];
    selectBuffer[5] = serNum[3];
    selectBuffer[6] = serNum[4]; // BCC

    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO

    // | SEL CLn | NVB | UID CLn + BCC | CRC_A |
    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, selectBuffer[0]); // SEL
    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, selectBuffer[1]); // NVB
    for (uint8_t i = 0; i < 5U; i++) {
        MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, selectBuffer[i + 2]); // UID CLn + BCC
    }

    uint8_t crc[2] = {0};
    MFRC522_CalculateCRC(handle, selectBuffer, 7, crc);
    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, crc[0]);
    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, crc[1]);

    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, 0x00); // 8 bits for SEL + NVB + UID + BCC + CRC_A

    MFRC522_ReadRegister(handle, MFRC522_FIFOLevelReg, &fifoLevel);

    // 13) Receive SAK
    if (fifoLevel >= 1) {
        uint8_t SAK = 0;
        MFRC522_ReadRegister(handle, MFRC522_FIFODataReg, &SAK);
        
        DEBUG_LOG("SAK Received: 0x%02X\r\n", SAK);

        uint8_t isComplete = (SAK & 0x04U) == 0; // Cascade bit not set means UID is complete
        if (!isComplete) {
            DEBUG_LOG("Cascade bit set in SAK, but only one UID CLn received\r\n");
            status = MFRC522_ERROR;
            goto AntiCollision_End;
        }

        uint8_t isISOCompliant = (SAK & 0x20U) != 0; // ISO/IEC 14443-4 compliant if bit 6 is set
        if (!isISOCompliant) {
            DEBUG_LOG("Card does not support ISO/IEC 14443-4\r\n");
            status = MFRC522_ERROR;
        }
        goto AntiCollision_End;
    }

    status = MFRC522_ERROR;

    AntiCollision_End:
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);

    if( status != MFRC522_OK && status != MFRC522_COLLISION) 
    {
        DEBUG_LOG("AntiCollision failed...");
        MFRC522_Disable_Antenna(handle);
    }

    return status;
}


/*
    Convienience functions
*/
MFRC522_Status_t MFRC522_IsCardPresent(MFRC522_Handle_t *handle)
{
    uint8_t atqa[2] = {0};
    return MFRC522_RequestA(handle, atqa);
}

MFRC522_Status_t MFRC522_ReadUID(MFRC522_Handle_t *handle)
{
    uint8_t _uid[5] = {0}; // 4 bytes UID + 1 byte BCC
    MFRC522_Status_t status = MFRC522_AntiCollision(handle, _uid);
    if (status == MFRC522_OK) {
        memcpy(handle->uid, _uid, 4); // Copy only the 4 bytes UID, excluding BCC
    }
    return status;
}

/* Testing */
bool MFRC522_Exec_SelfTest(MFRC522_Handle_t *handle, uint8_t *selfTestResult)
{
    if (handle == NULL || selfTestResult == NULL) {
        return false;
    }

    // Soft reset
    HAL_StatusTypeDef ret = MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_SoftReset);
    if(ret != HAL_OK) {
        return false;
    }
    // Wait for reset to complete: poll CommandReg[4] (PowerDown bit) or just wait 50ms
    HAL_Delay(50);

    // Clear internal buffer
    uint8_t ZERO[25] = {0x00};
    ret = MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);
    ret = MFRC522_WriteRegisterLong(handle, MFRC522_FIFODataReg, ZERO, sizeof(ZERO));
    ret = MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Mem);

    // Enable self-test
    ret = MFRC522_WriteRegister(handle, MFRC522_AutoTestReg, 0x09);

    // Write 0x00 to FIFO
    ret = MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, 0x00);

    // Start self-test
    ret = MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_CalcCRC);


    // Wait for self-test to complete (max 255 polls, matching Arduino reference)
    uint8_t out = 0;
    for (uint8_t i = 0; i < 0xFF; i++) {
        MFRC522_ReadRegister(handle, MFRC522_FIFOLevelReg, &out);
        if (out >= 64) {
            break;
        }
    }

    ret = MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);

    uint8_t test_result[64] = {0};
    ret = MFRC522_ReadRegisterLong(handle, MFRC522_FIFODataReg, test_result, 64);

    MFRC522_WriteRegister(handle, MFRC522_AutoTestReg, 0x00);

    bool passed = (memcmp(test_result, mfrc522_v2_self_test, 64) == 0);

    // Restore STATE
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_TxModeReg, 0x00);
    MFRC522_WriteRegister(handle, MFRC522_RxModeReg, 0x00);
    MFRC522_WriteRegister(handle, MFRC522_ModWidthReg, 0x26);
    MFRC522_WriteRegister(handle, MFRC522_TModeReg, 0x80);
    MFRC522_WriteRegister(handle, MFRC522_TPrescalerReg, 0xA9);
    MFRC522_WriteRegister(handle, MFRC522_TReloadRegH, 0x03);
    MFRC522_WriteRegister(handle, MFRC522_TReloadRegL, 0xE8);
    MFRC522_WriteRegister(handle, MFRC522_TxASKReg, 0x40);
    MFRC522_WriteRegister(handle, MFRC522_ModeReg, 0x3D);
    MFRC522_Enable_Antenna(handle);

    return passed;
}