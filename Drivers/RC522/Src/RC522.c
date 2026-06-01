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

/**
    @brief Initialize the MFRC522 module. This should be called before calling any other function.
    @param handle Pointer to MFRC522 handle struct.
    @param hspi Pointer to an initialized SPI handle for communication.
    @param cdPort GPIO port for chip select (CS) pin.
    @param cdPin GPIO pin for chip select (CS).
    @param rstPort GPIO port for reset (RST) pin.
    @param rstPin GPIO pin for reset (RST).
 */
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

/**
    @brief Write a value to a MFRC522 register.
    @param handle Pointer to MFRC522 handle struct.
    @param reg Register to write to.
    @param value Value to write.
 */
MFRC522_Status_t MFRC522_WriteRegister(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t value)
{
    MFRC522_Status_t ret = MFRC522_OK;
    uint8_t regAddr = ((uint8_t)reg << 1U) & 0x7EU;

    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL)) {
        return MFRC522_ERROR;
    }

    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_RESET);
    ret = (HAL_SPI_Transmit(handle->hspi, &regAddr, 1, HAL_MAX_DELAY) == HAL_OK) ? MFRC522_OK : MFRC522_ERROR;
    if(ret == MFRC522_OK) {
        ret = (HAL_SPI_Transmit(handle->hspi, &value, 1, HAL_MAX_DELAY) == HAL_OK) ? MFRC522_OK : MFRC522_ERROR;
    }
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    return ret;
}

MFRC522_Status_t MFRC522_WriteRegisterLong(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* data, uint16_t length)
{
    MFRC522_Status_t ret = MFRC522_OK;
    uint8_t regAddr = ((uint8_t)reg << 1U) & 0x7EU;

    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL) || (data == NULL)) {
        return MFRC522_ERROR;
    }

    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_RESET);
    ret = (HAL_SPI_Transmit(handle->hspi, &regAddr, 1, HAL_MAX_DELAY) == HAL_OK) ? MFRC522_OK : MFRC522_ERROR;
    if(ret == MFRC522_OK) {
        ret = (HAL_SPI_Transmit(handle->hspi, data, length, HAL_MAX_DELAY) == HAL_OK) ? MFRC522_OK : MFRC522_ERROR;
    }
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    return ret;
}

/**
    @brief Read a value from a MFRC522 register.
    @param handle Pointer to MFRC522 handle struct.
    @param reg Register to read from.
    @param out (Output) Pointer to store the read value.
 */
MFRC522_Status_t MFRC522_ReadRegister(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* out)
{
    MFRC522_Status_t ret = MFRC522_OK;
    uint8_t regAddr = (((uint8_t)reg << 1U) & 0x7EU) | 0x80U;

    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL) || (out == NULL)) {
        return MFRC522_ERROR;
    }

    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_RESET);
    ret = (HAL_SPI_Transmit(handle->hspi, &regAddr, 1, HAL_MAX_DELAY) == HAL_OK) ? MFRC522_OK : MFRC522_ERROR;
    if(ret == MFRC522_OK) {
        ret = (HAL_SPI_Receive(handle->hspi, out, 1, HAL_MAX_DELAY) == HAL_OK) ? MFRC522_OK : MFRC522_ERROR;
    }
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    return ret;
}

/**
    @brief Read multiple values from a MFRC522 register.
    @param handle Pointer to MFRC522 handle struct.
    @param reg Register to read from.
    @param out (Output) Pointer to store the read values.
    @param length Number of bytes to read.
 */
MFRC522_Status_t MFRC522_ReadRegisterLong(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t* out, uint16_t length)
{
    if ((handle == NULL) || (handle->hspi == NULL) || (handle->cdPort == NULL) || (out == NULL) || (length == 0U)) {
        return MFRC522_ERROR;
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
    MFRC522_Status_t ret = (HAL_SPI_TransmitReceive(handle->hspi, txBuf, rxBuf, txLen, HAL_MAX_DELAY) == HAL_OK) ? MFRC522_OK : MFRC522_ERROR;
    HAL_GPIO_WritePin(handle->cdPort, handle->csPin, GPIO_PIN_SET);

    if (ret == MFRC522_OK) {
        // rxBuf[0] is garbage (X), data starts at rxBuf[1]
        for (uint16_t i = 0; i < length; i++) {
            out[i] = rxBuf[i + 1U];
        }
    }

    return ret;
}

/**
    @brief Set specific bits in a MFRC522 register.
    @param handle Pointer to MFRC522 handle struct.
    @param reg Register to modify.
    @param mask Bit mask indicating which bits to set (1 = set, 0 = leave unchanged).
 */
MFRC522_Status_t MFRC522_SetBitMask(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t mask)
{
    uint8_t value = 0;
    MFRC522_Status_t ret = MFRC522_ReadRegister(handle, reg, &value);
    if (ret != MFRC522_OK) {
        return ret;
    }

    value |= mask;
    return MFRC522_WriteRegister(handle, reg, value);
}

/**
    @brief Clear specific bits in a MFRC522 register.
    @param handle Pointer to MFRC522 handle struct.
    @param reg Register to modify.
    @param mask Bit mask indicating which bits to clear (1 = clear, 0 = leave unchanged).
 */
MFRC522_Status_t MFRC522_ClearBitMask(MFRC522_Handle_t *handle, MFRC522_Register_t reg, uint8_t mask)
{
    uint8_t value = 0;
    MFRC522_Status_t ret = MFRC522_ReadRegister(handle, reg, &value);
    if (ret != MFRC522_OK) {
        return ret;
    }

    value &= (uint8_t)~mask;
    return MFRC522_WriteRegister(handle, reg, value);
}

/**
    @brief Enable the MFRC522 antenna.
    @param handle Pointer to MFRC522 handle struct.
 */
MFRC522_Status_t MFRC522_Enable_Antenna(MFRC522_Handle_t *handle)
{
    MFRC522_Status_t ret = MFRC522_SetBitMask(handle, MFRC522_TxControlReg, 0x03U);
    if (ret == MFRC522_OK) {
        DEBUG_LOG("Antenna On\r\n");
    } else {
        DEBUG_LOG("Failed to enable antenna\r\n");
    }

    return ret;
}

/**
    @brief Disable the MFRC522 antenna.
    @param handle Pointer to MFRC522 handle struct.
 */
MFRC522_Status_t MFRC522_Disable_Antenna(MFRC522_Handle_t *handle)
{
    MFRC522_Status_t ret = MFRC522_ClearBitMask(handle, MFRC522_TxControlReg, 0x03U);
    if (ret == MFRC522_OK) {
        DEBUG_LOG("Antenna Off\r\n");
    } else {
        DEBUG_LOG("Failed to disable antenna\r\n");
    }

    return ret;
}

/**
    @brief Generate CRC.
    @param handle Pointer to MFRC522 handle struct.
    @param data Pointer to the data buffer.
    @param length Length of the data buffer.
    @param out (Output) Pointer to store the calculated CRC.
 */
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

    // CRC result is in CRCResultRegL / CRCResultRegH
    if (MFRC522_ReadRegister(handle, MFRC522_CRCResultRegL, &out[0]) != MFRC522_OK ||
        MFRC522_ReadRegister(handle, MFRC522_CRCResultRegH, &out[1]) != MFRC522_OK) {
        status = MFRC522_ERROR;
        goto CalculateCRC_End;
    }

    CalculateCRC_End:
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    return status;
}

/**
    @brief Check the CRC of the given data.
    @param handle Pointer to MFRC522 handle struct.
    @param data Pointer to the data buffer.
    @param length Length of the data buffer.
 */
MFRC522_Status_t MFRC522_CheckCRC(MFRC522_Handle_t *handle, uint8_t* data, uint16_t length)
{
    if ((handle == NULL) || (data == NULL) || (length < 2U)) {
        return MFRC522_ERROR;
    }

    uint8_t calculatedCRC[2] = {0};
    MFRC522_Status_t status = MFRC522_CalculateCRC(handle, data, length - 2U, calculatedCRC);
    if (status != MFRC522_OK) {
        return status;
    }

    if ((calculatedCRC[0] != data[length - 2U]) || (calculatedCRC[1] != data[length - 1U])) {
        return MFRC522_CRC_WRONG;
    }

    return MFRC522_OK;
}

/**
    @brief Execute a command on the MFRC522 with the given data.
    @note This enabled the antenna, and does NOT disable it after the command is executed.
    @param handle Pointer to MFRC522 handle struct.
    @param cmd Command to execute (e.g. Transceive, Authenticate).
    @param txBuffer Pointer to the data to send (can be NULL if no data needs to be sent).
    @param txBufferLength Length of the data to send.
    @param bitFraming Bit framing settings (e.g. number of valid bits in the last byte).
 */
MFRC522_Status_t MFRC522_Execute_Command(MFRC522_Handle_t *handle, uint8_t cmd, uint8_t *txBuffer, uint16_t txBufferLength, uint8_t bitFraming)
{
    if(handle == NULL || txBuffer == NULL || txBufferLength == 0U) {
        return MFRC522_ERROR;
    }

    MFRC522_Enable_Antenna(handle);
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO
    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, bitFraming);

    if(txBuffer != NULL)
    {
        MFRC522_WriteRegisterLong(handle, MFRC522_FIFODataReg, txBuffer, txBufferLength);
    }
        
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, cmd);
    if (cmd == MFRC522_CMD_Transceive) {
        MFRC522_SetBitMask(handle, MFRC522_BitFramingReg, 0x80);
    }

    return MFRC522_OK;
}

/**
    @brief Transceive data with the MFRC522.
    @param handle Pointer to MFRC522 handle struct.
    @param txBuffer Pointer to the data to send.
    @param txBufferLength Length of the data to send.
    @param rxBuffer (Output) Pointer to store the received data.
    @param rxBufferLength Maximum length of the rxBuffer.
*/
MFRC522_Status_t MFRC522_Tranceive(MFRC522_Handle_t *handle, uint8_t *txBuffer, uint16_t txBufferLength, uint8_t *rxBuffer, uint8_t rxBufferLength)
{
    if(handle == NULL || txBuffer == NULL || txBufferLength == 0U || rxBuffer == NULL) {
        return MFRC522_ERROR;
    }

    MFRC522_Status_t status = MFRC522_Execute_Command(handle, MFRC522_CMD_Transceive, txBuffer, txBufferLength, 0x00);
    if (status != MFRC522_OK) {
        return status;
    }

    // Wait for the command to complete
    uint8_t irq = 0;
    uint32_t timeout = HAL_GetTick() + 50U;
    while (1) {
        if (MFRC522_ReadRegister(handle, MFRC522_CommIrqReg, &irq) != MFRC522_OK) {
            return MFRC522_ERROR;
        }
        if (irq & 0x30U) { // RxIRq or IdleIRq
            break;
        }
        if (HAL_GetTick() > timeout) {
            return MFRC522_TIMEOUT;
        }
    }

    // Read the received data from FIFO
    uint8_t fifoLevel = 0;
    if (MFRC522_ReadRegister(handle, MFRC522_FIFOLevelReg, &fifoLevel) != MFRC522_OK) {
        return MFRC522_ERROR;
    }

    if (fifoLevel > rxBufferLength) {
        return MFRC522_NO_ROOM;
    }
    
    MFRC522_ReadRegisterLong(handle, MFRC522_FIFODataReg, rxBuffer, fifoLevel);
    return status;
}

/***********************************************************
 PICC functions
***********************************************************/
/**
    @brief Send REQA command. Used to activate PICC to utilize IRQ.
    @param handle Pointer to MFRC522 handle struct.
 */
MFRC522_Status_t MFRC522_SendREQA(MFRC522_Handle_t *handle)
{
    MFRC522_Enable_Antenna(handle);
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);
    MFRC522_WriteRegister(handle, MFRC522_FIFODataReg, PICC_CMD_REQA);
    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, 0x07);
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Transceive);
    MFRC522_SetBitMask(handle, MFRC522_BitFramingReg, 0x80);

    return MFRC522_OK;
}

/**
    @brief Send a REQA command to the PIC.
    @param handle Pointer to MFRC522 handle struct.
    @param bufferATQA (Output) Pointer to store the ATQA response.
 */
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

    if ((status != MFRC522_OK) && (status != MFRC522_TIMEOUT) && (status != MFRC522_COLLISION)) 
    {
        DEBUG_LOG("RequestA failed...");
        MFRC522_Disable_Antenna(handle);
    }
    return status;
};

/**
    @brief Perform anti-collision detection to get the UID of a card.
    @param handle Pointer to MFRC522 handle struct.
    @param bufferUID (Output) Pointer to store the 4-byte UID of the card.
 */
MFRC522_Status_t MFRC522_AntiCollision(MFRC522_Handle_t *handle, uint8_t *serNum)
{
    MFRC522_Status_t status = MFRC522_OK;

    if ((handle == NULL) || (serNum == NULL)) {
        return MFRC522_INVALID;
    }

    DEBUG_LOG("Anti Collision\r\n");
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO
    MFRC522_ClearBitMask(handle, MFRC522_CollReg, 0x80);         // ValuesAfterColl = 0

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

    uint8_t crc[2] = {0};
    if (MFRC522_CalculateCRC(handle, selectBuffer, 7, crc) != MFRC522_OK) {
        DEBUG_LOG("Failed to calculate CRC for Select\r\n");
        status = MFRC522_ERROR;
        goto AntiCollision_End;
    }

    uint8_t selectFrame[9] = {
        selectBuffer[0], 
        selectBuffer[1], 
        selectBuffer[2], 
        selectBuffer[3], 
        selectBuffer[4],
        selectBuffer[5], 
        selectBuffer[6], 
        crc[0], 
        crc[1]
    };

    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO
    MFRC522_WriteRegisterLong(handle, MFRC522_FIFODataReg, selectFrame, sizeof(selectFrame));
    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, 0x00); // 8 bits for SEL + NVB + UID + BCC + CRC_A

    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Transceive);
    MFRC522_SetBitMask(handle, MFRC522_BitFramingReg, 0x80); // Transmit all and receive SAK

    timeout = HAL_GetTick() + 100U;

    uint8_t irq = 0;
    while (1) {
        MFRC522_ReadRegister(handle, MFRC522_CommIrqReg, &irq);
        if (irq & 0x30U) { // RxIRq or IdleIRq
            break;
        }
        if (HAL_GetTick() > timeout || (irq & 0x01U)) { // Timeout
            uint8_t errTimeout = 0;
            MFRC522_ReadRegister(handle, MFRC522_ErrorReg, &errTimeout);
            MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);
            DEBUG_LOG("Timeout waiting for SAK (ComIrq=0x%02X Error=0x%02X)\r\n", irq, errTimeout);
            status = MFRC522_TIMEOUT;
            goto AntiCollision_End;
        }
    }

    MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);

    MFRC522_ReadRegister(handle, MFRC522_ErrorReg, &err);
    if (err & 0x13U) // BufferOvfl, ParityErr, ProtocolErr
    {
        DEBUG_LOG("Error during Select: 0x%02X\r\n", err);
        status = MFRC522_ERROR;
        goto AntiCollision_End;
    }


    MFRC522_ReadRegister(handle, MFRC522_FIFOLevelReg, &fifoLevel);
    // 13) Receive SAK
    if (fifoLevel >= 1U) {
        uint8_t SAK = 0;
        MFRC522_ReadRegister(handle, MFRC522_FIFODataReg, &SAK);
        
        DEBUG_LOG("SAK Received: 0x%02X\r\n", SAK);

        uint8_t isComplete = (SAK & 0x04U) == 0; // Cascade bit not set means UID is complete
        if (!isComplete) {
            DEBUG_LOG("Cascade bit set in SAK, but only one UID CLn received\r\n");
            status = MFRC522_ERROR;
            goto AntiCollision_End;
        }

        status = MFRC522_OK;
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

/**
    @brief Disable encryption on the MFRC522 after MIFARE authentication.
    @param handle Pointer to MFRC522 handle struct.
 */
MFRC522_Status_t MFRC522_DisableCrypto1(MFRC522_Handle_t *handle)
{
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    return MFRC522_ClearBitMask(handle, MFRC522_Status2Reg, 0x08U); // Clear MFCrypto1On bit
}

/*****************************
    MIFARE Classic functions
*****************************/
/**
    @brief Authenticate a block of a MIFARE Classic card using the provided key and UID.
    @param handle Pointer to MFRC522 handle struct.
    @param cmd MIFARE authentication command (MIFARE_CMD_AUTH_KEY_A or MIFARE_CMD_AUTH_KEY_B).
    @param blockAddr Block address to authenticate.
    @param key Pointer to the 6-byte key for authentication.
    @param uid Pointer to the 4-byte UID of the card (must match the UID used in AntiCollision).
    @return MFRC522_OK if authentication was successful, otherwise an error code.
 */
MFRC522_Status_t MFRC522_MifareAuth(MFRC522_Handle_t *handle, MIFARE_Command_t cmd, uint8_t blockAddr, uint8_t *key, uint8_t *uid)
{
    if( (handle == NULL) || (key == NULL) || (uid == NULL) || ((cmd != MIFARE_CMD_AUTH_KEY_A) && (cmd != MIFARE_CMD_AUTH_KEY_B)) ) {
        return MFRC522_INVALID;
    }

    MFRC522_Status_t status = MFRC522_OK;

    // https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf: Section 10.3.1.9 - MFAuthent command
    // | Byte 0 | Byte 1 | Byte 2-7       | Byte 8-11       |
    // |--------|--------|-----------------|----------------|
    // | cmd    | block  | Key (6 bytes)  | UID (4 bytes)   |
    uint8_t txBuffer[12] = {0};
    txBuffer[0] = cmd;
    txBuffer[1] = blockAddr;
    memcpy(&txBuffer[2], key, 6);
    memcpy(&txBuffer[8], uid, 4);

    MFRC522_Enable_Antenna(handle);
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO
    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, 0x00);
    MFRC522_WriteRegisterLong(handle, MFRC522_FIFODataReg, txBuffer, sizeof(txBuffer));
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_MFAuthent); // Start authentication

    uint32_t timeout = HAL_GetTick() + 30U;
    while (1) {
        uint8_t irq = 0;
        MFRC522_ReadRegister(handle, MFRC522_CommIrqReg, &irq);
        if (irq & 0x30U) { // RxIRq or IdleIRq
            break;
        }
        if (irq & 0x01U || HAL_GetTick() > timeout) { // Timeout
            MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);
            DEBUG_LOG("Timeout during MIFARE Auth\r\n");
            status = MFRC522_TIMEOUT;
            goto MifareAuth_End;
        }
    }   

    uint8_t status2_reg = 0;
    MFRC522_ReadRegister(handle, MFRC522_Status2Reg, &status2_reg);
    status = (status2_reg &0x08) ? MFRC522_OK : MFRC522_ERROR; // MFCrypto1On bit indicates success


    MifareAuth_End:
    if( status != MFRC522_OK) {
        MFRC522_Disable_Antenna(handle);
    }

    return status;
}

/**
    @brief Read a block of data from a MIFARE Classic card. Must be authenticated first.
    @note This does not return the 2 CRC bytes, and the rxBuffer msut be of size 16 bytes to hold the block data.
    @param handle Pointer to MFRC522 handle struct.
    @param blockAddr Block address to read from.
    @param rxBuffer (Output) Pointer to store the 16-byte block data.
    @return MFRC522_OK if read was successful, otherwise an error code.
 */
MFRC522_Status_t MFRC522_MifareRead(MFRC522_Handle_t *handle, uint8_t blockAddr, uint8_t *rxData)
{
    if ((handle == NULL) || (rxData == NULL)) 
    {
        return MFRC522_INVALID;
    }

    MFRC522_Status_t status = MFRC522_OK;

    uint8_t temp_buffer[2] = {0};
    temp_buffer[0] = MIFARE_CMD_READ;
    temp_buffer[1] = blockAddr;
    uint8_t crc[2] = {0};

    if (MFRC522_CalculateCRC(handle, temp_buffer, 2, crc) != MFRC522_OK) {
        DEBUG_LOG("Failed to calculate CRC for MIFARE Read\r\n");
        return MFRC522_ERROR;
    }

    uint8_t txBuffer[4] = {0};
    txBuffer[0] = temp_buffer[0];
    txBuffer[1] = temp_buffer[1];
    txBuffer[2] = crc[0];
    txBuffer[3] = crc[1];

    MFRC522_Enable_Antenna(handle);
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(handle, MFRC522_CommIrqReg, 0x7F);      // Clear IRQs
    MFRC522_WriteRegister(handle, MFRC522_FIFOLevelReg, 0x80);   // Flush FIFO
    MFRC522_WriteRegister(handle, MFRC522_BitFramingReg, 0x00);
    MFRC522_WriteRegisterLong(handle, MFRC522_FIFODataReg, txBuffer, sizeof(txBuffer));
    
    // Send command
    MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_Transceive);
    MFRC522_SetBitMask(handle, MFRC522_BitFramingReg, 0x80);
    
    uint32_t timeout = HAL_GetTick() + 30U;
    uint8_t irq = 0;
    while (1) {
        MFRC522_ReadRegister(handle, MFRC522_CommIrqReg, &irq);
        if (irq & 0x30U) { // RxIRq or IdleIRq
            break;
        }
        if (irq & 0x01U || HAL_GetTick() > timeout) { // Timeout
            MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);
            DEBUG_LOG("Timeout during MIFARE Read\r\n");
            status = MFRC522_TIMEOUT;
            goto MifareRead_End;
        }
    }

    MFRC522_ClearBitMask(handle, MFRC522_BitFramingReg, 0x80U);

    uint8_t fifoLevel = 0;
    uint8_t err = 0;
    MFRC522_ReadRegister(handle, MFRC522_FIFOLevelReg, &fifoLevel);
    MFRC522_ReadRegister(handle, MFRC522_ErrorReg, &err);

    if (err & 0x13U) // BufferOvfl, ParityErr, ProtocolErr
    {
        DEBUG_LOG("Error during MIFARE Read: 0x%02X\r\n", err);
        status = MFRC522_ERROR;
        goto MifareRead_End;
    }

    if(fifoLevel != 18U)
    {
        DEBUG_LOG("Unexpected FIFO level after MIFARE Read: %d\r\n", fifoLevel);
        status = MFRC522_ERROR;
        goto MifareRead_End;
    }

    uint8_t rxBuffer_crc[18] = {0};
    MFRC522_ReadRegisterLong(handle, MFRC522_FIFODataReg, rxBuffer_crc, 18);

    if(MFRC522_CheckCRC(handle, rxBuffer_crc, 18) != MFRC522_OK) {
        DEBUG_LOG("CRC check failed for MIFARE Read\r\n");
        status = MFRC522_CRC_WRONG;
        goto MifareRead_End;
    }

    memcpy(rxData, rxBuffer_crc, 16);

    MifareRead_End:
    if( status != MFRC522_OK) {
        DEBUG_LOG("MIFARE Read failed (ComIrq=0x%02X Error=0x%02X)\r\n", irq, err);
        MFRC522_Disable_Antenna(handle);
        return status;
    }
    return status;
}

/**
    @brief Write a block of data to a MIFARE Classic card. Must be authenticated first.
    @note The data buffer must be of size 16 bytes to hold the block data. This function does not append the 2 CRC bytes, as the MFRC522 will calculate and append them automatically.
    @param handle Pointer to MFRC522 handle struct.
    @param blockAddr Block address to write to.
    @param data Pointer to the 16-byte block data to write.
    @return MFRC522_OK if write was successful, otherwise an error code.
 */
MFRC522_Status_t MFRC522_MifareWrite(MFRC522_Handle_t *handle, uint8_t blockAddr, uint8_t *data)
{
    if(handle == NULL || data == NULL) {
        return MFRC522_INVALID;
    }

    MFRC522_Status_t status = MFRC522_OK;

    uint8_t temp_buffer[2] = {0};
    temp_buffer[0] = MIFARE_CMD_WRITE;
    temp_buffer[1] = blockAddr;

    uint8_t crc[2] = {0};
    if (MFRC522_CalculateCRC(handle, temp_buffer, 2, crc) != MFRC522_OK) {
        DEBUG_LOG("Failed to calculate CRC for MIFARE Write\r\n");
        status = MFRC522_ERROR;
        goto MifareWrite_End;
    }

    uint8_t txBuffer[18] = {0};
    txBuffer[0] = temp_buffer[0];
    txBuffer[1] = temp_buffer[1];
    txBuffer[2] = crc[0];
    txBuffer[3] = crc[1];

    // Write 1
    uint8_t rxBuffer[18] = {0};
    if (MFRC522_Tranceive(handle, txBuffer, 4, rxBuffer, 18) != MFRC522_OK) {
        DEBUG_LOG("Failed to send MIFARE Write command\r\n");
        status = MFRC522_ERROR;
        goto MifareWrite_End;
    }

    // ACK

    // Write 2
    memcpy(txBuffer, data, 16);
    MFRC522_CalculateCRC(handle, txBuffer, 16, crc);
    txBuffer[16] = crc[0];
    txBuffer[17] = crc[1];
    if (MFRC522_Tranceive(handle, txBuffer, 18, rxBuffer, 18) != MFRC522_OK) {
        DEBUG_LOG("Failed to send MIFARE Write data\r\n");
        status = MFRC522_ERROR;
        goto MifareWrite_End;
    }

    MifareWrite_End:
    if( status != MFRC522_OK) {
        DEBUG_LOG("MIFARE Write failed\r\n");
        MFRC522_Disable_Antenna(handle);
    }
    return status;
}

/**************************
    Convenience functions
**************************/
/**
    @brief Check if a card is present.
    @param handle Pointer to MFRC522 handle struct.
    @return MFRC522_OK if a card is present, otherwise an error code.
 */
MFRC522_Status_t MFRC522_IsCardPresent(MFRC522_Handle_t *handle)
{
    uint8_t atqa[2] = {0};
    return MFRC522_RequestA(handle, atqa);
}

/**
    @brief Read the UID of a card. This is a convenience function that calls AntiCollision and extracts the UID.
    @param handle Pointer to MFRC522 handle struct. The read UID will be stored in handle->uid.
    @return MFRC522_OK if the UID was successfully read, otherwise an error code.
 */
MFRC522_Status_t MFRC522_ReadUID(MFRC522_Handle_t *handle)
{
    uint8_t _uid[5] = {0}; // 4 bytes UID + 1 byte BCC
    MFRC522_Status_t status = MFRC522_AntiCollision(handle, _uid);
    if (status == MFRC522_OK) {
        memcpy(handle->uid, _uid, 4); // Copy only the 4 bytes UID, excluding BCC
    }
    return status;
}

/**************************
        Testing
**************************/
/**
    @brief Execute the self-test of the MFRC522.
    @param handle Pointer to MFRC522 handle struct.
    @param selfTestResult (Output) Pointer to store the 64-byte self-test result.
    @return MFRC522_OK if self-test passed, otherwise an error code.
 */
MFRC522_Status_t MFRC522_Exec_SelfTest(MFRC522_Handle_t *handle, uint8_t *selfTestResult)
{
    if (handle == NULL || selfTestResult == NULL) {
        return MFRC522_ERROR;
    }

    // Soft reset
    MFRC522_Status_t ret = MFRC522_WriteRegister(handle, MFRC522_CommandReg, MFRC522_CMD_SoftReset);
    if(ret != MFRC522_OK) {
        return MFRC522_ERROR;
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

    memcpy(selfTestResult, test_result, 64);

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

    return  passed ? MFRC522_OK : MFRC522_ERROR;
}