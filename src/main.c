/*==================================================================================================
* Project : RTD AUTOSAR 4.7
* Platform : CORTEXM
* Peripheral : S32K14X
* Dependencies : none
*
* Autosar Version : 4.7.0
* Autosar Revision : ASR_REL_4_7_REV_0000
* Autosar Conf.Variant :
* SW Version : 3.0.0
* Build Version : S32K1_RTD_3_0_0_QLP04_D2509_ASR_REL_4_7_REV_0000_20250930
*
* Copyright 2020-2026 NXP
*
* 	NXP Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms.  By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms.  If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/
#include "Mcal.h"
#include "Clock_Ip.h"
#include "Port_Ci_Port_Ip.h"
#include "OsIf.h"
#include "Flexio_Mcl_Ip.h"
#include "Flexio_I2c_Ip.h"
#include "Lpuart_Uart_Ip.h"
#include "board_gpio.h"
#include <stdbool.h>
#include <string.h>

#include "ssd1306.h"
#include "pct2075dp.h"

/*==================================================================================================
*                                     DEFINES
==================================================================================================*/
#define OLED_FLEXIO_INSTANCE    0U
#define OLED_FLEXIO_CHANNEL     0U
#define UART_INSTANCE           1U

/* Temperature Settings */
#define TEMP_SETPOINT_DEFAULT   24
#define TEMP_SETPOINT_MIN       16
#define TEMP_SETPOINT_MAX       35
#define TEMP_HYSTERESIS         1       /* Hysteresis: 1 deg C */

#define TEMP_OFFSET             3.7f    /* Temperature offset correction (MCU board heat) */

/* Update intervals */
#define TEMP_READ_INTERVAL      10000U  /* Check temperature every 10 seconds */
#define DISPLAY_UPDATE_INTERVAL 1000U   /* Update display every 1 second */
#define BUTTON_CHECK_INTERVAL   50U     /* Check buttons every 50ms */
#define UART_SEND_INTERVAL      5000U   /* Send UART data every 5 seconds */

#define RELAY_TEST_DELAY        1000U
#define UART_BUFFER_SIZE        128U
#define UART_MESSAGE_DELAY      100U    /* Delay between UART messages (ms) */

/*==================================================================================================
*                                      GLOBAL VARIABLES
==================================================================================================*/
volatile int exit_code = 0;

/*==================================================================================================
*                                      LOCAL FUNCTIONS
==================================================================================================*/

static void Delay(uint32 ms)
{
    uint32 cur = OsIf_GetCounter(OSIF_COUNTER_SYSTEM);
    uint32 elapsed = 0U;
    uint32 timeout = OsIf_MicrosToTicks(ms * 1000U, OSIF_COUNTER_SYSTEM);

    while (elapsed < timeout) {
        elapsed += OsIf_GetElapsed(&cur, OSIF_COUNTER_SYSTEM);
    }
}

static void UART_SendString(const char* str)
{
    uint32 len = 0;
    const char* p = str;

    while (*p != '\0')
    {
        len++;
        p++;
    }

    if (len > 0)
    {
        Lpuart_Uart_Ip_SyncSend(UART_INSTANCE, (const uint8*)str, len, 1000000U);
    }
}

static void UART_SendStringWithDelay(const char* str)
{
    UART_SendString(str);
    Delay(UART_MESSAGE_DELAY);
}

static void IntToString(sint16 value, char* buffer)
{
    uint8 idx = 0;
    sint16 temp = value;
    uint8 digits[6];
    uint8 digitCount = 0;

    if (value < 0)
    {
        buffer[idx++] = '-';
        temp = -value;
    }

    if (temp == 0)
    {
        buffer[idx++] = '0';
    }
    else
    {
        while (temp > 0)
        {
            digits[digitCount++] = (uint8)(temp % 10);
            temp /= 10;
        }

        while (digitCount > 0)
        {
            buffer[idx++] = '0' + digits[--digitCount];
        }
    }

    buffer[idx] = '\0';
}

static void OLED_PeripheralInit(void)
{
    Flexio_Mcl_Ip_InitDevice(&Flexio_Ip_xFlexioInit);
    Flexio_I2c_Ip_MasterInit(OLED_FLEXIO_INSTANCE, OLED_FLEXIO_CHANNEL, &Flexio_I2cMasterChannel0);
    SSD1306_Init();
}

static void FloatToString(float value, char* buffer)
{
    sint16 intPart = (sint16)value;
    sint16 decPart = (sint16)((value - (float)intPart) * 10.0f);
    uint8 idx = 0;

    if (decPart < 0)
    {
        decPart = -decPart;
    }

    if (intPart < 0)
    {
        buffer[idx++] = '-';
        intPart = -intPart;
    }

    if (intPart >= 10)
    {
        buffer[idx++] = '0' + (uint8)(intPart / 10);
        buffer[idx++] = '0' + (uint8)(intPart % 10);
    }
    else
    {
        buffer[idx++] = '0' + (uint8)intPart;
    }

    buffer[idx++] = '.';
    buffer[idx++] = '0' + (uint8)decPart;
    buffer[idx] = '\0';
}

static void OLED_UpdateDisplay(float currentTemp, sint16 setpoint, bool heatingState, bool coolingState, bool sensorError)
{
    char tempStr[8];
    char setpointStr[8];

    /* When cooling is active, always show heating OFF (Heat:OFF Cool:ON) */
    bool showHeat = heatingState && !coolingState;

    SSD1306_ClearBuffer();

    SSD1306_DrawString(2, 0, "Temp Controller", 1, true);
    SSD1306_DrawLine(0, 9, 127, 9, true);

    /* Current Temperature */
    SSD1306_DrawString(1, 12, "Temp:", 1, true);
    if (sensorError)
    {
        SSD1306_DrawString(40, 12, "ERROR", 1, true);
    }
    else
    {
        FloatToString(currentTemp, tempStr);
        SSD1306_DrawString(40, 12, tempStr, 1, true);
        SSD1306_DrawString(75, 12, "C", 1, true);
    }

    /* Setpoint */
    SSD1306_DrawString(1, 22, "Set:", 1, true);
    IntToString(setpoint, setpointStr);
    SSD1306_DrawString(40, 22, setpointStr, 1, true);
    SSD1306_DrawString(75, 22, "C", 1, true);

    /* Combined Heating/Cooling Status on a single line */
    if (sensorError)
    {
        SSD1306_DrawString(0, 32, "RL1:--- RL2:---", 1, true);
    }
    else
    {
        char statusStr[24];
        strcpy(statusStr, showHeat ? "RL1:ON " : "RL1:OFF ");
        strcat(statusStr, coolingState ? "RL2:ON" : "RL2:OFF");
        SSD1306_DrawString(1, 32, statusStr, 1, true);
    }


    /* Instructions */
    SSD1306_DrawLine(0, 52, 127, 52, true);
    SSD1306_DrawString(1, 54, "SW3:+ SW4:-", 1, true);
    SSD1306_UpdateScreen();
}

static void OLED_ShowStartupScreen(void)
{
    SSD1306_ClearBuffer();
    SSD1306_DrawString(10, 10, "FRDM-A-S32K118", 1, true);
    SSD1306_DrawString(15, 25, "Thermostat", 1, true);
    SSD1306_DrawLine(0, 40, 127, 40, true);
    SSD1306_DrawString(10, 45, "Initializing...", 1, true);
    SSD1306_UpdateScreen();
    Delay(2000U);
}

static void TestRelaysAtStartup(void)
{
    SSD1306_ClearBuffer();
    SSD1306_DrawString(2,  0,  "Relay Test", 1, true);
    SSD1306_DrawLine(0, 9, 127, 9, true);

    /* Test RL1 (Heating) */
    UART_SendStringWithDelay("\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("  RELAY TEST - RL1 (HEATING)\r\n");
    UART_SendStringWithDelay("========================================\r\n");

    SSD1306_DrawString(1,  20, "Testing RL1...", 1, true);
    SSD1306_DrawString(1,  30, "(Heating)", 1, true);
    SSD1306_DrawString(1,  40, "Status: ON", 1, true);
    SSD1306_UpdateScreen();

    UART_SendStringWithDelay("  Status: ACTIVATING RL1...\r\n");
    Board_Gpio_WritePin(RL1_PORT, RL1_PIN, 1);
    Delay(RELAY_TEST_DELAY);

    SSD1306_DrawString(1,  40, "Status: OFF", 1, true);
    SSD1306_UpdateScreen();

    UART_SendStringWithDelay("  Status: DEACTIVATING RL1...\r\n");
    Board_Gpio_WritePin(RL1_PORT, RL1_PIN, 0);
    Delay(RELAY_TEST_DELAY);

    /* Test RL2 (Cooling) */
    UART_SendStringWithDelay("\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("  RELAY TEST - RL2 (COOLING)\r\n");
    UART_SendStringWithDelay("========================================\r\n");

    SSD1306_ClearBuffer();
    SSD1306_DrawString(2,  0,  "Relay Test", 1, true);
    SSD1306_DrawLine(0, 9, 127, 9, true);
    SSD1306_DrawString(1,  20, "Testing RL2...", 1, true);
    SSD1306_DrawString(1,  30, "(Cooling)", 1, true);
    SSD1306_DrawString(1,  40, "Status: ON", 1, true);
    SSD1306_UpdateScreen();

    UART_SendStringWithDelay("  Status: ACTIVATING RL2...\r\n");
    Board_Gpio_WritePin(RL2_PORT, RL2_PIN, 1);
    Delay(RELAY_TEST_DELAY);

    SSD1306_DrawString(1,  40, "Status: OFF", 1, true);
    SSD1306_UpdateScreen();

    UART_SendStringWithDelay("  Status: DEACTIVATING RL2...\r\n");
    Board_Gpio_WritePin(RL2_PORT, RL2_PIN, 0);
    Delay(RELAY_TEST_DELAY);

    /* Test Complete */
    UART_SendStringWithDelay("\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("  RELAY TEST COMPLETE\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("\r\n");

    SSD1306_ClearBuffer();
    SSD1306_DrawString(2,  0,  "Relay Test", 1, true);
    SSD1306_DrawLine(0, 9, 127, 9, true);
    SSD1306_DrawString(1,  20, "Test Complete!", 1, true);
    SSD1306_DrawString(1,  30, "Starting...", 1, true);
    SSD1306_UpdateScreen();
    Delay(1000U);
}

static void UART_SendTemperatureData(float currentTemp, sint16 setpoint, bool heatingState, bool coolingState, bool sensorError)
{
    char tempStr[8];
    char setpointStr[8];
    char lowerThresholdStr[8];
    char upperThresholdStr[8];
    float lowerThreshold = (float)(setpoint - TEMP_HYSTERESIS);
    float upperThreshold = (float)(setpoint + TEMP_HYSTERESIS);

    UART_SendStringWithDelay("\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("  TEMPERATURE MONITORING REPORT\r\n");
    UART_SendStringWithDelay("========================================\r\n");

    /* Sensor Status */
    UART_SendStringWithDelay("  Sensor Status: ");
    if (sensorError)
    {
        UART_SendStringWithDelay("ERROR - PCT2075DP NOT RESPONDING\r\n");
    }
    else
    {
        UART_SendStringWithDelay("OK - PCT2075DP ACTIVE\r\n");
    }

    /* Current Temperature */
    UART_SendStringWithDelay("  Current Temperature: ");
    if (sensorError)
    {
        UART_SendStringWithDelay("N/A\r\n");
    }
    else
    {
        FloatToString(currentTemp, tempStr);
        UART_SendStringWithDelay(tempStr);
        UART_SendStringWithDelay(" deg C\r\n");
    }

    /* Setpoint */
    UART_SendStringWithDelay("  Target Setpoint: ");
    IntToString(setpoint, setpointStr);
    UART_SendStringWithDelay(setpointStr);
    UART_SendStringWithDelay(" deg C\r\n");

    /* Thresholds */
    UART_SendStringWithDelay("  Lower Threshold: ");
    FloatToString(lowerThreshold, lowerThresholdStr);
    UART_SendStringWithDelay(lowerThresholdStr);
    UART_SendStringWithDelay(" deg C (Heating ON)\r\n");

    UART_SendStringWithDelay("  Upper Threshold: ");
    FloatToString(upperThreshold, upperThresholdStr);
    UART_SendStringWithDelay(upperThresholdStr);
    UART_SendStringWithDelay(" deg C (Cooling ON)\r\n");


    UART_SendStringWithDelay("----------------------------------------\r\n");

    /* Relay Status */
    UART_SendStringWithDelay("  Heating Relay (RL1): ");
    if (sensorError)
    {
        UART_SendStringWithDelay("DISABLED (SENSOR ERROR)\r\n");
    }
    else
    {
        UART_SendStringWithDelay(heatingState ? "ACTIVE (ON)\r\n" : "INACTIVE (OFF)\r\n");
    }

    UART_SendStringWithDelay("  Cooling Relay (RL2): ");
    if (sensorError)
    {
        UART_SendStringWithDelay("DISABLED (SENSOR ERROR)\r\n");
    }
    else
    {
        UART_SendStringWithDelay(coolingState ? "ACTIVE (ON)\r\n" : "INACTIVE (OFF)\r\n");
    }

    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("\r\n");
}

static void UpdateRelayControl(float currentTemp, sint16 setpoint, bool* heatingState, bool* coolingState)
{
    float lowerThreshold = (float)(setpoint - TEMP_HYSTERESIS);
    float upperThreshold = (float)(setpoint + TEMP_HYSTERESIS);

    /* Temperature below lower threshold - activate heating */
    if (currentTemp < lowerThreshold)
    {
        if (!(*heatingState))
        {
            *heatingState = true;
            Board_Gpio_WritePin(RL1_PORT, RL1_PIN, 1);
            UART_SendStringWithDelay("\r\n[RELAY CONTROL] Heating ACTIVATED - Temp below threshold\r\n");
        }
        if (*coolingState)
        {
            *coolingState = false;
            Board_Gpio_WritePin(RL2_PORT, RL2_PIN, 0);
            UART_SendStringWithDelay("[RELAY CONTROL] Cooling DEACTIVATED\r\n");
        }
    }
    /* Temperature above upper threshold - activate cooling */
    else if (currentTemp > upperThreshold)
    {
        if (!(*coolingState))
        {
            *coolingState = true;
            Board_Gpio_WritePin(RL2_PORT, RL2_PIN, 1);
            UART_SendStringWithDelay("\r\n[RELAY CONTROL] Cooling ACTIVATED - Temp above threshold\r\n");
        }
        if (*heatingState)
        {
            *heatingState = false;
            Board_Gpio_WritePin(RL1_PORT, RL1_PIN, 0);
            UART_SendStringWithDelay("[RELAY CONTROL] Heating DEACTIVATED\r\n");
        }
    }
    /* Temperature within acceptable range - turn both off */
    else if ((currentTemp >= (float)setpoint - 0.5f) && (currentTemp <= (float)setpoint + 0.5f))
    {
        if (*heatingState)
        {
            *heatingState = false;
            Board_Gpio_WritePin(RL1_PORT, RL1_PIN, 0);
            UART_SendStringWithDelay("\r\n[RELAY CONTROL] Heating DEACTIVATED - Temp in range\r\n");
        }
        if (*coolingState)
        {
            *coolingState = false;
            Board_Gpio_WritePin(RL2_PORT, RL2_PIN, 0);
            UART_SendStringWithDelay("[RELAY CONTROL] Cooling DEACTIVATED - Temp in range\r\n");
        }
    }
}

/*==================================================================================================
*                                          MAIN
==================================================================================================*/

int main(void)
{
    float currentTemp = 0.0f;
    float rawTemp = 0.0f;
    sint16 tempSetpoint = TEMP_SETPOINT_DEFAULT;
    bool heatingState = false;
    bool coolingState = false;
    bool sensorError = false;

    uint32 lastDisplayUpdateTime = 0;
    uint32 lastButtonCheckTime = 0;
    uint32 lastUartSendTime = 0;
    uint32 currentTime = 0;

    bool sw3Pressed = false;
    bool sw4Pressed = false;
    bool displayNeedsUpdate = false;

    /* Initialize system clocks */
    Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);

    /* Initialize OS interface for timing */
    OsIf_Init(NULL_PTR);

    /* Initialize GPIO pins */
    Port_Ci_Port_Ip_Init(NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
                         g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals);

    /* Initialize UART */
    Lpuart_Uart_Ip_Init(UART_INSTANCE, &Lpuart_Uart_Ip_xHwConfigPB_1);

    /* Send startup message */
    Delay(500U);
    UART_SendStringWithDelay("\r\n\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("  FRDM-A-S32K118 THERMOSTAT SYSTEM\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("  Platform: NXP S32K118\r\n");
    UART_SendStringWithDelay("  Sensor: PCT2075DP (I2C)\r\n");
    UART_SendStringWithDelay("  Actuators: 2x Relay Click\r\n");
    UART_SendStringWithDelay("  Display: SSD1306 OLED\r\n");
    UART_SendStringWithDelay("========================================\r\n");
    UART_SendStringWithDelay("\r\n");
    UART_SendStringWithDelay("Initializing peripherals...\r\n");

    /* Initialize PCT2075DP temperature sensor */
    UART_SendStringWithDelay("  - Initializing PCT2075DP sensor...\r\n");
    if (PCT2075DP_Init() != PCT2075DP_OK)
    {
        sensorError = true;
        UART_SendStringWithDelay("  [ERROR] PCT2075DP initialization failed!\r\n");
    }
    else
    {
        UART_SendStringWithDelay("  [OK] PCT2075DP initialized successfully\r\n");
    }

    /* Initialize OLED display */
    UART_SendStringWithDelay("  - Initializing SSD1306 OLED display...\r\n");
    OLED_PeripheralInit();
    UART_SendStringWithDelay("  [OK] OLED display initialized\r\n");
    OLED_ShowStartupScreen();

    /* Test relays at startup */
    UART_SendStringWithDelay("\r\nStarting relay test sequence...\r\n");
    TestRelaysAtStartup();

    /* Initial temperature reading */
    UART_SendStringWithDelay("Reading initial temperature...\r\n");
    if (!sensorError)
    {
        if (PCT2075DP_GetTemp(&rawTemp) == PCT2075DP_OK)
        {
            currentTemp = rawTemp - TEMP_OFFSET;  /* Apply temperature offset correction */
            UART_SendStringWithDelay("  [OK] Initial temperature reading successful\r\n");
            UpdateRelayControl(currentTemp, tempSetpoint, &heatingState, &coolingState);
        }
        else
        {
            sensorError = true;
            UART_SendStringWithDelay("  [ERROR] Initial temperature reading failed\r\n");
        }
    }

    /* Initial display and UART update */
    OLED_UpdateDisplay(currentTemp, tempSetpoint, heatingState, coolingState, sensorError);
    UART_SendTemperatureData(currentTemp, tempSetpoint, heatingState, coolingState, sensorError);

    UART_SendStringWithDelay("System ready. Entering main control loop...\r\n");
    UART_SendStringWithDelay("Temperature reports will be sent every 5 seconds.\r\n");
    UART_SendStringWithDelay("\r\n");

    /* Main control loop */
    for(;;)
    {
        currentTime = OsIf_GetCounter(OSIF_COUNTER_SYSTEM);

        /* Continuous temperature reading and relay control */
        if (PCT2075DP_GetTemp(&rawTemp) == PCT2075DP_OK)
        {
            currentTemp = rawTemp - TEMP_OFFSET;  /* Apply temperature offset correction */

            if (sensorError)
            {
                sensorError = false;
                UART_SendStringWithDelay("\r\n[SENSOR] PCT2075DP connection restored\r\n");
            }

            UpdateRelayControl(currentTemp, tempSetpoint, &heatingState, &coolingState);
            displayNeedsUpdate = true;
        }
        else
        {
            if (!sensorError)
            {
                UART_SendStringWithDelay("\r\n[ERROR] PCT2075DP sensor communication lost!\r\n");
            }

            sensorError = true;

            /* Safety: Turn off both relays on sensor error */
            if (heatingState)
            {
                heatingState = false;
                Board_Gpio_WritePin(RL1_PORT, RL1_PIN, 0);
                UART_SendStringWithDelay("[SAFETY] Heating relay disabled due to sensor error\r\n");
            }
            if (coolingState)
            {
                coolingState = false;
                Board_Gpio_WritePin(RL2_PORT, RL2_PIN, 0);
                UART_SendStringWithDelay("[SAFETY] Cooling relay disabled due to sensor error\r\n");
            }
            displayNeedsUpdate = true;
        }

        /* Button handling for setpoint adjustment */
        if ((currentTime - lastButtonCheckTime) >= OsIf_MicrosToTicks(BUTTON_CHECK_INTERVAL * 1000U, OSIF_COUNTER_SYSTEM))
        {
            lastButtonCheckTime = currentTime;

            /* SW3: Increase setpoint */
            if (Board_Gpio_ReadPin(SW3_PORT, SW3_PIN) == 0U)
            {
                if (!sw3Pressed)
                {
                    sw3Pressed = true;

                    if (tempSetpoint < TEMP_SETPOINT_MAX)
                    {
                        tempSetpoint++;

                        UART_SendStringWithDelay("\r\n[USER INPUT] Setpoint increased to ");
                        char setpointStr[8];
                        IntToString(tempSetpoint, setpointStr);
                        UART_SendStringWithDelay(setpointStr);
                        UART_SendStringWithDelay(" deg C\r\n");


                        if (!sensorError)
                        {
                            UpdateRelayControl(currentTemp, tempSetpoint, &heatingState, &coolingState);
                        }

                        /* Update display immediately after button press */
                        OLED_UpdateDisplay(currentTemp, tempSetpoint, heatingState, coolingState, sensorError);
                        displayNeedsUpdate = false;
                    }
                }
            }
            else
            {
                sw3Pressed = false;
            }

            /* SW4: Decrease setpoint */
            if (Board_Gpio_ReadPin(SW4_PORT, SW4_PIN) == 0U)
            {
                if (!sw4Pressed)
                {
                    sw4Pressed = true;

                    if (tempSetpoint > TEMP_SETPOINT_MIN)
                    {
                        tempSetpoint--;

                        UART_SendStringWithDelay("\r\n[USER INPUT] Setpoint decreased to ");
                        char setpointStr[8];
                        IntToString(tempSetpoint, setpointStr);
                        UART_SendStringWithDelay(setpointStr);
                        UART_SendStringWithDelay(" deg C\r\n");


                        if (!sensorError)
                        {
                            UpdateRelayControl(currentTemp, tempSetpoint, &heatingState, &coolingState);
                        }

                        /* Update display immediately after button press */
                        OLED_UpdateDisplay(currentTemp, tempSetpoint, heatingState, coolingState, sensorError);
                        displayNeedsUpdate = false;
                    }
                }
            }
            else
            {
                sw4Pressed = false;
            }
        }

        /* Periodic display update (every 1 second) OR when flagged for update */
        if (displayNeedsUpdate ||
            ((currentTime - lastDisplayUpdateTime) >= OsIf_MicrosToTicks(DISPLAY_UPDATE_INTERVAL * 1000U, OSIF_COUNTER_SYSTEM)))
        {
            lastDisplayUpdateTime = currentTime;
            OLED_UpdateDisplay(currentTemp, tempSetpoint, heatingState, coolingState, sensorError);
            displayNeedsUpdate = false;
        }

        /* Periodic UART temperature report (every 5 seconds) */
        if ((currentTime - lastUartSendTime) >= OsIf_MicrosToTicks(UART_SEND_INTERVAL * 1000U, OSIF_COUNTER_SYSTEM))
        {
            lastUartSendTime = currentTime;
            UART_SendTemperatureData(currentTemp, tempSetpoint, heatingState, coolingState, sensorError);
        }


        if(exit_code != 0)
        {
            break;
        }
    }
    return exit_code;
}

/** @} */
