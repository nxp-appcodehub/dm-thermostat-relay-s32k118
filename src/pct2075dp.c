/*==================================================================================================
 * File : pct2075dp.c
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
 *   PCT2075DP temperature sensor driver implementation for S32K118.
 *
 *   Uses the S32K1 RTD Lpi2c_Ip blocking master API. The temperature
 *   conversion math is ported from the MCUXpresso ISSDK PCT2075DP driver.
 ==================================================================================================*/

#include "pct2075dp.h"
#include "Lpi2c_Ip.h"

/* LPI2C instance number that maps to channel I2c_Lpi2cMasterChannel0 (LPI2C0). */
#define PCT2075DP_LPI2C_INSTANCE   (0U)

/* Blocking transfer timeout in milliseconds. */
#define PCT2075DP_I2C_TIMEOUT_MS   (100U)

/* -------------------------------------------------------------------------
 * LPI2C master callback.
 *
 * Referenced by the generated Lpi2c_Ip_PBcfg.c (I2c_Lpi2cMasterChannel0).
 * The blocking transfer APIs handle completion internally, so this callback
 * body is intentionally empty; it only needs to exist for linking.
 * ------------------------------------------------------------------------- */
void LPI2C0_Master_Callback(Lpi2c_Ip_MasterEventType event, uint8 userData)
{
    (void)event;
    (void)userData;
}

/* -------------------------------------------------------------------------
 * Read a sensor register (write register offset, then read 'length' bytes).
 * ------------------------------------------------------------------------- */
static Pct2075dp_StatusType PCT2075DP_ReadReg(uint8 offset,
                                              uint8 *pOutBuffer,
                                              uint8 length)
{
    Lpi2c_Ip_StatusType status;

    /* Point the sensor to the target register (no STOP - repeated start). */
    status = Lpi2c_Ip_MasterSendDataBlocking(PCT2075DP_LPI2C_INSTANCE,
                                             &offset, 1U, FALSE,
                                             PCT2075DP_I2C_TIMEOUT_MS);
    if (status != LPI2C_IP_SUCCESS_STATUS)
    {
        return PCT2075DP_ERROR;
    }

    /* Read the register contents. */
    status = Lpi2c_Ip_MasterReceiveDataBlocking(PCT2075DP_LPI2C_INSTANCE,
                                                pOutBuffer, length, TRUE,
                                                PCT2075DP_I2C_TIMEOUT_MS);
    if (status != LPI2C_IP_SUCCESS_STATUS)
    {
        return PCT2075DP_ERROR;
    }

    return PCT2075DP_OK;
}

/* -------------------------------------------------------------------------
 * Convert the 2-byte raw TEMP register value to degrees Celsius.
 * Ported from PCT2075DP_I2C_ConvertRegToTempValue (DeviceTemp path).
 * ------------------------------------------------------------------------- */
static float PCT2075DP_ConvertTemp(const uint8 *pBuffer)
{
    uint16 temp;
    uint16 value;
    boolean negative;
    float result;

    temp = (uint16)(((uint16)pBuffer[0] << 8) | (uint16)pBuffer[1]);
    negative = ((temp & PCT2075DP_TEMP_NEGPOS_MASK) != 0U) ? TRUE : FALSE;

    temp = (uint16)(temp >> PCT2075DP_TEMP_IGNORE_SHIFT);

    if (negative != FALSE)
    {
        value = (uint16)(~temp + 1U);
        value = (uint16)(value & 0x07FFU);
        result = -1.0f * PCT2075DP_CELSIUS_CONV_VAL * (float)value;
    }
    else
    {
        result = PCT2075DP_CELSIUS_CONV_VAL * (float)temp;
    }

    return result;
}

Pct2075dp_StatusType PCT2075DP_Init(void)
{
    /* Initialize the LPI2C master using the generated channel config. */
    Lpi2c_Ip_MasterInit(PCT2075DP_LPI2C_INSTANCE, &I2c_Lpi2cMasterChannel0);

    /* Select the PCT2075DP 7-bit slave address for subsequent transfers. */
    Lpi2c_Ip_MasterSetSlaveAddr(PCT2075DP_LPI2C_INSTANCE,
                                (uint16)PCT2075DP_SLAVE_ADDRESS, FALSE);

    return PCT2075DP_OK;
}

Pct2075dp_StatusType PCT2075DP_GetTemp(float *pTempC)
{
    uint8 reg[2];
    Pct2075dp_StatusType status;

    if (pTempC == NULL_PTR)
    {
        return PCT2075DP_ERROR;
    }

    status = PCT2075DP_ReadReg(PCT2075DP_REG_TEMP, &reg[0], 2U);
    if (status != PCT2075DP_OK)
    {
        return PCT2075DP_ERROR;
    }

    *pTempC = PCT2075DP_ConvertTemp(&reg[0]);

    return PCT2075DP_OK;
}
