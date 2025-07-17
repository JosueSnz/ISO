#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
// #include <WiFi.h>
// #include <ESPmDNS.h>
// #include <WiFiUdp.h>
// #include <ArduinoOTA.h>

// Comunicação UART LV/HV
#define RXHV 16 
#define TXHV 17

// Saídas digitais do ESP (ESP-->INV)
#define di1 21
#define di2 19  
#define di3 18
#define di4 25
#define di5 13
#define di6 12
#define di7 27
#define di8 26

// Entradas digitais para o ESP (INV-->ESP)
#define do2 35 
// #define do3 23 // Anterior
// #define do4 34 // Anterior 
#define do3 34
#define do4 23

// Saídas PWM do ESP (ESP-->INV)
#define ai1 33
#define ai2 32

// Entradas analógicas para o ESP (INV-->ESP)
#define ao1 36
#define ao2 39

// Pino do botão de calibração
#define offset 4 
#define led 22 

// Declaração do nosso tipo de estado. A variável em si será definida no .cpp
enum SystemState { NORMAL, CALIBRANDO };

#endif