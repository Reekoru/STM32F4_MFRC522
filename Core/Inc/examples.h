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
    Example_VersionNumber,
    Example_SelfTest,
    Example_ReadUIDPoll,
    Example_ReadUIDIRQ,
} MCRF522_Example_t;

void Examples_Run(MFRC522_Handle_t *mfrc522, MCRF522_Example_t example);

  #endif /* _EXAMPLES_H_ */