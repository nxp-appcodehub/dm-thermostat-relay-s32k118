/*==================================================================================================
 * File : pct2075dp.h
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
 *   PCT2075DP temperature sensor driver for S32K118 (RTD AUTOSAR 4.7 LPI2C IP).
 *
 *   This is a self-contained port of the register map and temperature
 *   conversion logic from the MCUXpresso ISSDK PCT2075DP driver, adapted to
 *   the S32K1 RTD Lpi2c_Ip blocking master API. It provides simple polled
 *   access to the sensor (no ALERT/OS interrupt line support).
 ==================================================================================================*/

#ifndef PCT2075DP_H
#define PCT2075DP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"

/* PCT2075DP 7-bit I2C slave address (A2/A1/A0 tied low, shield default). */
#define PCT2075DP_SLAVE_ADDRESS   (0x48U)

/* PCT2075DP internal register map. */
#define PCT2075DP_REG_TEMP        (0x00U)
#define PCT2075DP_REG_CONFIG      (0x01U)
#define PCT2075DP_REG_THYS        (0x02U)
#define PCT2075DP_REG_TOS         (0x03U)
#define PCT2075DP_REG_TIDLE       (0x04U)

/* Conversion constants. */
#define PCT2075DP_CELSIUS_CONV_VAL     (0.125f)
#define PCT2075DP_TEMP_IGNORE_SHIFT    (5U)
#define PCT2075DP_TEMP_NEGPOS_MASK     (0x8000U)

/* Driver return codes. */
typedef enum
{
    PCT2075DP_OK    = 0,
    PCT2075DP_ERROR = 1,
} Pct2075dp_StatusType;

/*!
 * @brief Initialize the PCT2075DP driver.
 *
 * Initializes the LPI2C master instance and stores the target slave address.
 * The LPI2C peripheral must already be configured via the S32 Config Tool
 * (channel config I2c_Lpi2cMasterChannel0).
 *
 * @return PCT2075DP_OK on success, PCT2075DP_ERROR otherwise.
 */
Pct2075dp_StatusType PCT2075DP_Init(void);

/*!
 * @brief Read the current temperature in degrees Celsius.
 *
 * @param[out] pTempC  pointer that receives the temperature in Celsius.
 * @return PCT2075DP_OK on success, PCT2075DP_ERROR on I2C failure.
 */
Pct2075dp_StatusType PCT2075DP_GetTemp(float *pTempC);

#ifdef __cplusplus
}
#endif

#endif /* PCT2075DP_H */
