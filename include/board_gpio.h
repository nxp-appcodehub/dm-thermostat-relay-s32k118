/*==================================================================================================
 * File : board_gpio.h
 *
 * Copyright 2026 NXP
 *
 *   NXP Proprietary. This software is owned or controlled by NXP and may only be
 *   used strictly in accordance with the applicable license terms. By expressly
 *   accepting such terms or by downloading, installing, activating and/or otherwise
 *   using the software, you are agreeing that you have read, and that you agree to
 *   comply with and are bound by, such license terms. If you do not agree to be
 *   bound by the applicable license terms, then you may not retain, install,
 *   activate or otherwise use the software.
 *
 *   Provides Board_Gpio_WritePin and Board_Gpio_ReadPin using direct S32K118
 *   register access (PSOR/PCOR/PDIR).
 *
 * Usage:
 *   Board_Gpio_WritePin(IP_PTB, 13U, 1U);  -- set PTB13 HIGH
 *   Board_Gpio_WritePin(IP_PTB, 13U, 0U);  -- set PTB13 LOW
 *   val = Board_Gpio_ReadPin(IP_PTD, 4U);  -- read PTD4
 *
 ==================================================================================================*/

#ifndef BOARD_GPIO_H
#define BOARD_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "S32K118_GPIO.h"   /* GPIO_Type, IP_PTA..IP_PTE */

/*!
 * @brief Write a single GPIO pin.
 *
 * @param base  GPIO port base pointer (IP_PTA, IP_PTB, IP_PTC, IP_PTD, IP_PTE)
 * @param pin   Pin index 0..31
 * @param val   0 = LOW, non-zero = HIGH
 */
static inline void Board_Gpio_WritePin(GPIO_Type * const base,
                                       uint32 pin,
                                       uint8  val)
{
    if (val != 0U) {
        base->PSOR = (uint32)(1UL << pin);
    } else {
        base->PCOR = (uint32)(1UL << pin);
    }
}

/*!
 * @brief Read a single GPIO pin.
 *
 * @param base  GPIO port base pointer
 * @param pin   Pin index 0..31
 * @return      0 if pin is LOW, 1 if pin is HIGH
 */
static inline uint8 Board_Gpio_ReadPin(const GPIO_Type * const base,
                                       uint32 pin)
{
    return (uint8)((base->PDIR >> pin) & 1UL);
}

#ifdef __cplusplus
}
#endif

#endif /* BOARD_GPIO_H */
