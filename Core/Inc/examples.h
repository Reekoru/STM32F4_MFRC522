#ifndef _EXAMPLES_H_
#define _EXAMPLES_H_
/*
  ******************************************************************************
  * @file           : examples.h
  * @brief          : Header for examples.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
*/

#include "main.h"
#include "stm32f4xx_it.h"
#include "RC522.h"

typedef enum {
    Example_VersionNumber,  // Get and print MFRC522 version number
    Example_SelfTest,       // Run MFRC522 self test and print result
    Example_ReadUID,        // Wait for card to be present, then read and print UID
    Example_ReadUIDIRQ,     // Send REQA to activate PICC and wait for IRQ, then read and print UID
    Example_RegisterUID,    // Register a new UID by storing it in NVMEM (not implemented yet)
    Example_AuthenticateUID,   // Authenticate with a card and read some data (not implemented yet)
} MCRF522_Example_t;

void Examples_Run(MFRC522_Handle_t *mfrc522, MCRF522_Example_t example);

  #endif /* _EXAMPLES_H_ */