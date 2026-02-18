#pragma once

// --- Core UI defines ---
#define SPI_FREQUENCY         40000000   // Fréquence SPI écran
#define SPI_READ_FREQUENCY    20000000   // Fréquence SPI lecture
#define SPI_TOUCH_FREQUENCY   2500000    // Fréquence SPI tactile
#define UI_LCD_SPI_HOST       0          // SPI Host LCD
#define UI_SERIAL_BAUD        57600      // Baudrate UI link interne
#define UI_ROTATION           1          // Rotation UI par défaut
#define ILI9488_DRIVER        // Driver écran ILI9488
#define TFT_RGB_ORDER         TFT_BGR    // Ordre RGB écran

// Configuration hardware Freenove Media Kit All-in-One (ESP32-S3)

// --- Display ---
#define FREENOVE_LCD_WIDTH      480  // Largeur écran
#define FREENOVE_LCD_HEIGHT     320  // Hauteur écran
#define FREENOVE_LCD_ROTATION   1    // Rotation écran (0-3)

// --- TFT SPI ---
#define FREENOVE_TFT_SCK        18   // GPIO 18 : SCK (SPI écran)
#define FREENOVE_TFT_MOSI       23   // GPIO 23 : MOSI (SPI écran)
#define FREENOVE_TFT_MISO       19   // GPIO 19 : MISO (optionnel)
#define FREENOVE_TFT_CS         5    // GPIO 5  : CS (partagé BTN4)
#define FREENOVE_TFT_DC         16   // GPIO 16 : DC (commande/données)
#define FREENOVE_TFT_RST        17   // GPIO 17 : RESET écran
#define FREENOVE_TFT_BL         4    // GPIO 4  : BL (partagé BTN3)

// --- Touch SPI (XPT2046) ---
#define FREENOVE_TOUCH_CS       21   // GPIO 21 : CS tactile
#define FREENOVE_TOUCH_IRQ      22   // GPIO 22 : IRQ tactile

// --- Buttons (internal pull-up) ---
#define FREENOVE_BTN_1          2    // GPIO 2  : Bouton 1
#define FREENOVE_BTN_2          3    // GPIO 3  : Bouton 2
#define FREENOVE_BTN_3          4    // GPIO 4  : Bouton 3 (BL shared)
#define FREENOVE_BTN_4          5    // GPIO 5  : Bouton 4 (CS shared)
// Attention : ne pas activer BL et BTN3 ou CS et BTN4 simultanément.

// --- Audio (I2S only) ---
#define FREENOVE_I2S_WS         25   // GPIO 25 : I2S WS (obligatoire)
#define FREENOVE_I2S_BCK        26   // GPIO 26 : I2S BCK
#define FREENOVE_I2S_DOUT       27   // GPIO 27 : I2S DOUT

// --- Misc peripherals ---
#define FREENOVE_LED            13   // GPIO 13 : indicateur LED
#define FREENOVE_BUZZER         12   // GPIO 12 : buzzer (PWM)
#define FREENOVE_DHT11          14   // GPIO 14 : DHT11 (si utilisé)
#define FREENOVE_I2C_SDA        8    // GPIO 8  : SDA I2C (MPU6050, capteurs)
#define FREENOVE_I2C_SCL        9    // GPIO 9  : SCL I2C
#define FREENOVE_MPU6050_ADDR   0x68 // Adresse I2C MPU6050 (optionnel)

// Ce firmware repose sur l'I2S pour l'audio et sur un seul ESP32-S3; UI link externe n'est pas requis.
