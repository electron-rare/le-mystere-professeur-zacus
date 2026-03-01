// Configuration hardware spécifique Freenove Media Kit (RP2040)
#pragma once

#define FREENOVE_LCD_WIDTH 480
#define FREENOVE_LCD_HEIGHT 320
#define FREENOVE_TFT_CS 5
#define FREENOVE_TFT_DC 6
#define FREENOVE_TFT_RST 7
#define FREENOVE_TFT_MOSI 3
#define FREENOVE_TFT_MISO 4
#define FREENOVE_TFT_SCK 2
#define FREENOVE_TOUCH_CS 9
#define FREENOVE_TOUCH_IRQ 15
#define FREENOVE_UART_TX 0
#define FREENOVE_UART_RX 1
#define FREENOVE_LCD_ROTATION 1

// Boutons physiques (exemples typiques du kit)
#define FREENOVE_BTN_A 10
#define FREENOVE_BTN_B 11
#define FREENOVE_BTN_C 12

// LED et buzzer
#define FREENOVE_LED 25
#define FREENOVE_BUZZER 13

// Capteur DHT11 (temp/humidité)
#define FREENOVE_DHT11 14

// I2C (SDA/SCL)
#define FREENOVE_I2C_SDA 8
#define FREENOVE_I2C_SCL 9

// Capteur MPU6050 (accéléro/gyro, I2C)
#define FREENOVE_MPU6050_ADDR 0x68

// Ajoutez ici d'autres capteurs/modules selon le kit (voir datasheet Freenove)
