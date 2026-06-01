#include "examples.h"
#include "main.h"
#include "stm32f4xx_hal_gpio.h"
#include "RC522.h"
#include "utils.h"

/********************************
    Function Prototypes
********************************/
static void _SelfTest(MFRC522_Handle_t *mfrc522);
static void _VersionNumber(MFRC522_Handle_t *mfrc522);
static void _ReadUID(MFRC522_Handle_t *mfrc522);
static void _ReadUIDIRQ(MFRC522_Handle_t *mfrc522);
static void _ReadWrite(MFRC522_Handle_t *mfrc522);
/********************************
    For IRQ Example
********************************/
static volatile uint8_t mfrc522_irqFlag = 0; 
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == MFRC522_EXTI_Pin) {
        mfrc522_irqFlag = 1;
    }
}

void Examples_Run(MFRC522_Handle_t *mfrc522, MCRF522_Example_t example) 
{
    switch (example) {
        case Example_ReadUID:
            _ReadUID(mfrc522);
            break;
        case Example_ReadUIDIRQ:
            _ReadUIDIRQ(mfrc522);
            break;
        case Example_SelfTest:
            _SelfTest(mfrc522);
            break;
        case Example_VersionNumber:
            _VersionNumber(mfrc522);
            break;
        case Example_ReadWrite:
            _ReadWrite(mfrc522);
            break;
        default:
            DEBUG_LOG("Unknown example selected\r\n");
            break;
    }

    DEBUG_LOG("Example finished\r\n");
}

void _ReadUID(MFRC522_Handle_t *mfrc522)
{
    DEBUG_LOG("Running ReadUID Polling Example\r\n");
    while (1)
    {
        if (MFRC522_IsCardPresent(mfrc522) != MFRC522_OK) continue;
        if (MFRC522_ReadUID(mfrc522) != MFRC522_OK) continue;

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
        DEBUG_LOG("Card UID: %02X %02X %02X %02X\r\n", mfrc522->uid[0], mfrc522->uid[1], mfrc522->uid[2], mfrc522->uid[3]);
        HAL_Delay(1000);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_Delay(200);
    }
}

void _ReadUIDIRQ(MFRC522_Handle_t *mfrc522)
{
    MFRC522_WriteRegister(mfrc522, MFRC522_CommandReg, MFRC522_CMD_Idle);
    MFRC522_WriteRegister(mfrc522, MFRC522_CommIrqReg, 0x7F);

    // Enable RxIrq + IdleIrq
    MFRC522_WriteRegister(mfrc522, MFRC522_ComIEnReg, 0x30);

    DEBUG_LOG("Running ReadUID IRQ Example\r\n");
    while (1)
    {
        mfrc522_irqFlag = 0;
        MFRC522_SendREQA(mfrc522);

        // Wait for IRQ
        uint32_t timeout = HAL_GetTick() + 30U;
        while ((!mfrc522_irqFlag) && (HAL_GetTick() < timeout)) {
        }

        MFRC522_ClearBitMask(mfrc522, MFRC522_BitFramingReg, 0x80U);

        if (mfrc522_irqFlag) 
        {
            if(MFRC522_IsCardPresent(mfrc522) != MFRC522_OK) continue;
            if (MFRC522_ReadUID(mfrc522) != MFRC522_OK) continue;
            
            DEBUG_LOG("Card UID: %02X %02X %02X %02X\r\n", mfrc522->uid[0], mfrc522->uid[1], mfrc522->uid[2], mfrc522->uid[3]);
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

            MFRC522_WriteRegister(mfrc522, MFRC522_CommandReg, MFRC522_CMD_Idle);
            MFRC522_WriteRegister(mfrc522, MFRC522_CommIrqReg, 0x7F);
        }
        HAL_Delay(50);
    }
}

void _ReadWrite(MFRC522_Handle_t *mfrc522)
{
    DEBUG_LOG("Running ReadWrite Example\r\n");
    while(1)
    {
        MFRC522_Status_t opStatus = MFRC522_OK;

        if(MFRC522_IsCardPresent(mfrc522) != MFRC522_OK) continue;
        if(MFRC522_ReadUID(mfrc522) != MFRC522_OK) continue;
        
        uint8_t block = 4;
        uint8_t keyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Default key for MIFARE Classic
        opStatus = MFRC522_MifareAuth(mfrc522, MIFARE_CMD_AUTH_KEY_A, block, keyA, mfrc522->uid);
        if(opStatus != MFRC522_OK) 
        {
            DEBUG_LOG("Authentication failed for block %d\r\n", block);
            goto ReadWrite_Cleanup;
        }

        DEBUG_LOG("Authentication successful for block %d\r\n", block);
        uint8_t readBuffer[16] = {0}; // 16 bytes data

        opStatus = MFRC522_MifareRead(mfrc522, block, readBuffer);
        if (opStatus != MFRC522_OK) {
            DEBUG_LOG("Failed to read from block %d\r\n", block);
            goto ReadWrite_Cleanup;
        }
    
        DEBUG_LOG_HEX("Data read from block:", readBuffer, 16);

        // Increment array values by 1 for writing back to the card
        for (int i = 0; i < 16; i++) {
            readBuffer[i]++;

            if(readBuffer[i] != 0) { // Check for overflow
                break;
            }
        }

        opStatus = MFRC522_MifareWrite(mfrc522, block, readBuffer);
        if (opStatus != MFRC522_OK) {
            DEBUG_LOG("Failed to write to block %d\r\n", block);
            goto ReadWrite_Cleanup;
        }

        DEBUG_LOG("Data written to block %d\r\n", block);

        ReadWrite_Cleanup:
        MFRC522_DisableCrypto1(mfrc522);
        MFRC522_WriteRegister(mfrc522, MFRC522_CommandReg, MFRC522_CMD_Idle);
        HAL_Delay(20);
    }
}

void _SelfTest(MFRC522_Handle_t *mfrc522)
{
    DEBUG_LOG("Running Self Test Example\r\n");
    uint8_t selfTestResult[64] = {0};

    MFRC522_Status_t status = MFRC522_Exec_SelfTest(mfrc522, selfTestResult);
    DEBUG_LOG("MFRC522 Self-test result: %d\r\n", status == MFRC522_OK);
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, status == MFRC522_OK);

    DEBUG_LOG_HEX("Result:", selfTestResult, 64);
}

void _VersionNumber(MFRC522_Handle_t *mfrc522)
{
    DEBUG_LOG("Running Version Number Example\r\n");
    uint8_t mfrc522Version = 0;
    HAL_StatusTypeDef ret = MFRC522_ReadRegister(mfrc522, MFRC522_VersionReg, &mfrc522Version);

    if (ret == HAL_OK) {
        DEBUG_LOG("MFRC522 Version: 0x%02X\r\n", mfrc522Version);
        if ((mfrc522Version != 0x91U) && (mfrc522Version != 0x92U)) {
          DEBUG_LOG("Unexpected version value. You are probably using a clone MFRC522 module with a different chip (e.g. 0x88 for FM17522) or there is an issue with SPI communication.\r\n");
        }
    } else {
        DEBUG_LOG("Failed to read MFRC522 version\r\n");
    }
}
