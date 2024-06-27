/*
*   RadioLib Transmit with Interrupts Example
*
*   This example transmits packets using SX1276/SX1278/SX1262/SX1268/SX1280/LR1121 LoRa radio module.
*   Each packet contains up to 256 bytes of data, in the form of:
*    - Arduino String
*    - null-terminated char array (C-string)
*    - arbitrary binary data (byte array)
*
*   For full API reference, see the GitHub Pages
*   https://jgromes.github.io/RadioLib/
*
*
*/
#include <SD.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "utilities.h"


// The example is adapted to T-ETH-Elite-LoRa-Shield. Uncomment the model you want to use according to the wireless LoRa module you purchased.

// #define USING_SX1262
// #define USING_LR1121
// #define USING_SX1280
// #define USING_SX1276
// #define USING_SX1276_TCXO
// #define USING_SX1280PA


#define DISPLAY_MODEL               U8G2_SSD1306_128X64_NONAME_F_HW_I2C
#define DISPLAY_ADDR                0x3C

#define U8G2_HOR_ALIGN_CENTER(t)    ((u8g2->getDisplayWidth() -  (u8g2->getUTF8Width(t))) / 2)
#define U8G2_HOR_ALIGN_RIGHT(t)     ( u8g2->getDisplayWidth()  -  u8g2->getUTF8Width(t))



#if     defined(USING_SX1276)
#define CONFIG_RADIO_FREQ           868.0
#define CONFIG_RADIO_OUTPUT_POWER   17
#define CONFIG_RADIO_BW             125.0
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);


#elif   defined(USING_SX1276_TCXO)
#define CONFIG_RADIO_FREQ           868.0
#define CONFIG_RADIO_OUTPUT_POWER   17
#define CONFIG_RADIO_BW             125.0
#define TCXO_ENABLE_PIN             38
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);


#elif   defined(USING_SX1278)
#define CONFIG_RADIO_FREQ           433.0
#define CONFIG_RADIO_OUTPUT_POWER   17
#define CONFIG_RADIO_BW             125.0
SX1278 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

#elif   defined(USING_SX1262)
#define CONFIG_RADIO_FREQ           868.0
#define CONFIG_RADIO_OUTPUT_POWER   22
#define CONFIG_RADIO_BW             125.0

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

#elif   defined(USING_SX1280)
#define CONFIG_RADIO_FREQ           2400.0
#define CONFIG_RADIO_OUTPUT_POWER   13
#define CONFIG_RADIO_BW             203.125
SX1280 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

#elif defined(USING_SX1280PA)
#define CONFIG_RADIO_FREQ           2400.0
// PA Version power range : -18 ~ 3dBm
#define CONFIG_RADIO_OUTPUT_POWER  3
#define CONFIG_RADIO_BW             203.125
SX1280 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#define RADIO_RX_PIN                13
#define RADIO_TX_PIN                38

#elif   defined(USING_SX1268)
#define CONFIG_RADIO_FREQ           433.0
#define CONFIG_RADIO_OUTPUT_POWER   22
#define CONFIG_RADIO_BW             125.0
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

#elif   defined(USING_LR1121)
// The maximum power of LR1121 2.4G band can only be set to 13 dBm
#define CONFIG_RADIO_FREQ           2450.0
#define CONFIG_RADIO_OUTPUT_POWER   13
#define CONFIG_RADIO_BW             125.0

// The maximum power of LR1121 Sub 1G band can only be set to 22 dBm
// #define CONFIG_RADIO_FREQ           868.0
// #define CONFIG_RADIO_OUTPUT_POWER   22
// #define CONFIG_RADIO_BW             125.0

LR1121 radio = new Module(RADIO_CS_PIN, RADIO_IRQ_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

#else

#error "Please select the LoRa model you need to use !"

#endif

void printResult(bool radio_online);
bool beginDisplay();
void drawMain();

// save transmission state between loops
static int transmissionState = RADIOLIB_ERR_NONE;
// flag to indicate that a packet was sent
static volatile bool transmittedFlag = false;
static uint32_t counter = 0;
static String payload;


DISPLAY_MODEL *u8g2 = NULL;


// this function is called when a complete packet
// is transmitted by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void)
{
    // we sent a packet, set the flag
    transmittedFlag = true;
}

void setup()
{

    Serial.begin(115200);

    // Onboard LED is a multiplexed function
    // SX1276-TCXO: LED IO38 is multiplexed as TCXO enable function
    // SX1280PA: LED IO38 is multiplexed as LoRa (TX) antenna control function enable function
    // When using other model modules, it is only LED function
#if  defined(TCXO_ENABLE_PIN) || defined(USING_SX1280PA)
    pinMode(TCXO_ENABLE_PIN, OUTPUT);
    digitalWrite(TCXO_ENABLE_PIN, HIGH);
#else
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
#endif

    // Initialize SPI bus
    SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

    // Radio and SD card share the bus. During initialization, their respective CS Pin needs to be set to HIGH.
    pinMode(RADIO_CS_PIN, OUTPUT);
    digitalWrite(RADIO_CS_PIN, HIGH);
    // Radio and SD card share the bus. During initialization, their respective CS Pin needs to be set to HIGH.
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Warning: Failed to init SD card");
    } else {
        Serial.println("SD card init succeeded, size : ");
        Serial.print(SD.cardSize() / (1024 * 1024));
        Serial.println("MBytes");
    }

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    beginDisplay();

    // When the power is turned on, a delay is required.
    delay(1500);


    // initialize radio with default settings
    int state = radio.begin();

    printResult(state == RADIOLIB_ERR_NONE);

    Serial.print(F("Radio Initializing ... "));
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    // set the function that will be called
    // when packet transmission is finished
    radio.setPacketSentAction(setFlag);


    /*
    *   Sets carrier frequency.
    *   SX1278/SX1276 : Allowed values range from 137.0 MHz to 525.0 MHz.
    *   SX1268/SX1262 : Allowed values are in range from 150.0 to 960.0 MHz.
    *   SX1280        : Allowed values are in range from 2400.0 to 2500.0 MHz.
    *   LR1121        : Allowed values are in range from 150.0 to 960.0 MHz, 1900 - 2200 MHz and 2400 - 2500 MHz. Will also perform calibrations.
    * * * */

    if (radio.setFrequency(CONFIG_RADIO_FREQ) == RADIOLIB_ERR_INVALID_FREQUENCY) {
        Serial.println(F("Selected frequency is invalid for this module!"));
        while (true);
    }

    /*
    *   Sets LoRa link bandwidth.
    *   SX1278/SX1276 : Allowed values are 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250 and 500 kHz. Only available in %LoRa mode.
    *   SX1268/SX1262 : Allowed values are 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0 and 500.0 kHz.
    *   SX1280        : Allowed values are 203.125, 406.25, 812.5 and 1625.0 kHz.
    *   LR1121        : Allowed values are 62.5, 125.0, 250.0 and 500.0 kHz.
    * * * */
    if (radio.setBandwidth(CONFIG_RADIO_BW) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
        Serial.println(F("Selected bandwidth is invalid for this module!"));
        while (true);
    }


    /*
    * Sets LoRa link spreading factor.
    * SX1278/SX1276 :  Allowed values range from 6 to 12. Only available in LoRa mode.
    * SX1262        :  Allowed values range from 5 to 12.
    * SX1280        :  Allowed values range from 5 to 12.
    * LR1121        :  Allowed values range from 5 to 12.
    * * * */
    if (radio.setSpreadingFactor(12) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
        Serial.println(F("Selected spreading factor is invalid for this module!"));
        while (true);
    }

    /*
    * Sets LoRa coding rate denominator.
    * SX1278/SX1276/SX1268/SX1262 : Allowed values range from 5 to 8. Only available in LoRa mode.
    * SX1280        :  Allowed values range from 5 to 8.
    * LR1121        :  Allowed values range from 5 to 8.
    * * * */
    if (radio.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
        Serial.println(F("Selected coding rate is invalid for this module!"));
        while (true);
    }

    /*
    * Sets LoRa sync word.
    * SX1278/SX1276/SX1268/SX1262/SX1280 : Sets LoRa sync word. Only available in LoRa mode.
    * * */
    if (radio.setSyncWord(0xAB) != RADIOLIB_ERR_NONE) {
        Serial.println(F("Unable to set sync word!"));
        while (true);
    }

    /*
    * Sets transmission output power.
    * SX1278/SX1276 :  Allowed values range from -3 to 15 dBm (RFO pin) or +2 to +17 dBm (PA_BOOST pin). High power +20 dBm operation is also supported, on the PA_BOOST pin. Defaults to PA_BOOST.
    * SX1262        :  Allowed values are in range from -9 to 22 dBm. This method is virtual to allow override from the SX1261 class.
    * SX1268        :  Allowed values are in range from -9 to 22 dBm.
    * SX1280        :  Allowed values are in range from -18 to 13 dBm. PA Version range : -18 ~ 3dBm
    * LR1121        :  Allowed values are in range from -9 to 22 dBm (high-power PA) or -17 to 14 dBm (low-power PA)
    * * * */
    if (radio.setOutputPower(CONFIG_RADIO_OUTPUT_POWER) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        while (true);
    }

#if !defined(USING_SX1280) && !defined(USING_SX1280PA) && !defined(USING_LR1121)
    /*
    * Sets current limit for over current protection at transmitter amplifier.
    * SX1278/SX1276 : Allowed values range from 45 to 120 mA in 5 mA steps and 120 to 240 mA in 10 mA steps.
    * SX1262/SX1268 : Allowed values range from 45 to 120 mA in 2.5 mA steps and 120 to 240 mA in 10 mA steps.
    * NOTE: set value to 0 to disable overcurrent protection
    * * * */
    if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true);
    }
#endif

    /*
    * Sets preamble length for LoRa or FSK modem.
    * SX1278/SX1276 : Allowed values range from 6 to 65535 in %LoRa mode or 0 to 65535 in FSK mode.
    * SX1262/SX1268 : Allowed values range from 1 to 65535.
    * SX1280        : Allowed values range from 1 to 65535. preamble length is multiple of 4
    * LR1121        : Allowed values range from 1 to 65535.
    * * */
    if (radio.setPreambleLength(16) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH) {
        Serial.println(F("Selected preamble length is invalid for this module!"));
        while (true);
    }

    // Enables or disables CRC check of received packets.
    if (radio.setCRC(false) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) {
        Serial.println(F("Selected CRC is invalid for this module!"));
        while (true);
    }

#ifdef USING_DIO2_AS_RF_SWITCH
#ifdef USING_SX1262
    // Some SX126x modules use DIO2 as RF switch. To enable
    // this feature, the following method can be used.
    // NOTE: As long as DIO2 is configured to control RF switch,
    //       it can't be used as interrupt pin!
    if (radio.setDio2AsRfSwitch() != RADIOLIB_ERR_NONE) {
        Serial.println(F("Failed to set DIO2 as RF switch!"));
        while (true);
    }
#endif //USING_SX1262
#endif //USING_DIO2_AS_RF_SWITCH


#if  defined(USING_LR1121)
    // LR1121
    // set RF switch configuration for Wio WM1110
    // Wio WM1110 uses DIO5 and DIO6 for RF switching
    static const uint32_t rfswitch_dio_pins[] = {
        RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
        RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
    };

    static const Module::RfSwitchMode_t rfswitch_table[] = {
        // mode                  DIO5  DIO6
        { LR11x0::MODE_STBY,   { LOW,  LOW  } },
        { LR11x0::MODE_RX,     { HIGH, LOW  } },
        { LR11x0::MODE_TX,     { LOW,  HIGH } },
        { LR11x0::MODE_TX_HP,  { LOW,  HIGH } },
        { LR11x0::MODE_TX_HF,  { LOW,  LOW  } },
        { LR11x0::MODE_GNSS,   { LOW,  LOW  } },
        { LR11x0::MODE_WIFI,   { LOW,  LOW  } },
        END_OF_MODE_TABLE,
    };
    radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

    // LR1121 TCXO Voltage 2.85~3.15V
    radio.setTCXO(3.0);
#endif

#ifdef RADIO_RX_PIN
    // SX1280 PA Version
    radio.setRfSwitchPins(RADIO_RX_PIN, RADIO_TX_PIN);
#endif

    // start transmitting the first packet
    Serial.print(F("Radio Sending first packet ... "));

    // you can transmit C-string or Arduino string up to
    // 256 characters long
    transmissionState = radio.startTransmit(String(counter).c_str());

    // you can also transmit byte array up to 256 bytes long
    /*
      byte byteArr[] = {0x01, 0x23, 0x45, 0x67,
                        0x89, 0xAB, 0xCD, 0xEF};
      state = radio.startTransmit(byteArr, 8);
    */
    delay(1000);

    drawMain();
}

void loop()
{
    // check if the previous transmission finished
    if (transmittedFlag) {

        payload = "#" + String(counter++);

        // reset flag
        transmittedFlag = false;

        if (transmissionState == RADIOLIB_ERR_NONE) {
            // packet was successfully sent
            Serial.println(F("transmission finished!"));
            // NOTE: when using interrupt-driven transmit method,
            //       it is not possible to automatically measure
            //       transmission data rate using getDataRate()
        } else {
            Serial.print(F("failed, code "));
            Serial.println(transmissionState);
        }


        drawMain();
        // wait a second before transmitting again
        delay(1000);

        // send another one
        Serial.print(F("Radio Sending another packet ... "));

        // you can transmit C-string or Arduino string up to
        // 256 characters long
        transmissionState = radio.startTransmit(payload);
        // you can also transmit byte array up to 256 bytes long
        /*
          byte byteArr[] = {0x01, 0x23, 0x45, 0x67,
                            0x89, 0xAB, 0xCD, 0xEF};
          int state = radio.startTransmit(byteArr, 8);
        */

    }
}


void drawMain()
{
    if (u8g2) {
        u8g2->clearBuffer();
        u8g2->drawRFrame(0, 0, 128, 64, 5);

        u8g2->setFont(u8g2_font_pxplusibmvga8_mr);
        u8g2->setCursor(22, 25);
        u8g2->print("TX:");
        u8g2->setCursor(22, 40);
        u8g2->print("STATE:");

        u8g2->setFont(u8g2_font_crox1h_tr);
        u8g2->setCursor( U8G2_HOR_ALIGN_RIGHT(payload.c_str()) - 21, 25 );
        u8g2->print(payload);
        String state = transmissionState == RADIOLIB_ERR_NONE ? "NONE" : String(transmissionState);
        u8g2->setCursor( U8G2_HOR_ALIGN_RIGHT(state.c_str()) -  21, 40 );
        u8g2->print(state);
        u8g2->sendBuffer();
    }
}



bool beginDisplay()
{
    Wire.beginTransmission(DISPLAY_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.printf("Find Display model at 0x%X address\n", DISPLAY_ADDR);
        u8g2 = new DISPLAY_MODEL(U8G2_R0, U8X8_PIN_NONE);
        u8g2->begin();
        u8g2->clearBuffer();
        u8g2->setFont(u8g2_font_inb19_mr);
        u8g2->drawStr(0, 30, "LilyGo");
        u8g2->drawHLine(2, 35, 47);
        u8g2->drawHLine(3, 36, 47);
        u8g2->drawVLine(45, 32, 12);
        u8g2->drawVLine(46, 33, 12);
        u8g2->setFont(u8g2_font_inb19_mf);
        u8g2->drawStr(58, 60, "LoRa");
        u8g2->sendBuffer();
        u8g2->setFont(u8g2_font_fur11_tf);
        delay(3000);
        return true;
    }

    Serial.printf("Warning: Failed to find Display at 0x%0X address\n", DISPLAY_ADDR);
    return false;
}



void printResult(bool radio_online)
{
    Serial.print("Radio        : ");
    Serial.println((radio_online) ? "+" : "-");
    Serial.print("PSRAM        : ");
    Serial.println((psramFound()) ? "+" : "-");
    Serial.print("Display      : ");
    Serial.println(( u8g2) ? "+" : "-");
    Serial.print("Sd Card      : ");
    Serial.println((SD.cardSize() != 0) ? "+" : "-");

#ifdef HAS_PMU
    Serial.print("Power        : ");
    Serial.println(( PMU ) ? "+" : "-");
#endif


    if (u8g2) {

        u8g2->clearBuffer();
        u8g2->setFont(u8g2_font_NokiaLargeBold_tf );
        uint16_t str_w =  u8g2->getStrWidth("T-ETH");
        u8g2->drawStr((u8g2->getWidth() - str_w) / 2, 16, "T-ETH");
        u8g2->drawHLine(5, 21, u8g2->getWidth() - 5);
        u8g2->drawStr( 0, 38, "Disp:");     u8g2->drawStr( 45, 38, ( u8g2) ? "+" : "-");
        u8g2->drawStr( 0, 54, "SD :");      u8g2->drawStr( 45, 54, (SD.cardSize() != 0) ? "+" : "-");
        u8g2->drawStr( 62, 38, "Radio:");    u8g2->drawStr( 120, 38, ( radio_online ) ? "+" : "-");

#ifdef HAS_PMU
        u8g2->drawStr( 62, 54, "Power:");    u8g2->drawStr( 120, 54, ( PMU ) ? "+" : "-");
#endif

        u8g2->sendBuffer();

    }
}