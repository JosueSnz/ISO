#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <SPI.h>      
#include "mcp_can.h"
#include <BluetoothSerial.h>
// #include <WiFi.h>
// #include <ESPmDNS.h>
// #include <WiFiUdp.h>
// #include <ArduinoOTA.h>

// Comunicação UART LV/HV
#define RXHV 16 
#define TXHV 17

// Entradas digitais do ESP (Sensor-->ESP)
#define di1 35
#define di2 19  //no final o 34
#define di3 13
#define di4 14
#define di5 4 
#define di6 22
#define di7 21
#define di8 12

// Saídas digitais do INV (INV-->ESP)
#define do2 27 
#define do3 25
#define do4 26

// Entradas analógicas do ESP (Sensor-->ESP)
#define ai1 36
#define ai2 39

// Saídas analógicas do INV (INV-->ESP)
#define ao1 33
#define ao2 32

// CAN CS
#define CS 5

#endif