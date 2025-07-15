#include "main.h"

// =================================================================
// === DEFINIÇÃO DE VARIÁVEIS GLOBAIS ===
// =================================================================

// --- Variáveis de Comunicação ---
const byte numChars = 64;
char receivedChars[numChars];
byte charIndex = 0;

// --- Variáveis de Controle dos Pinos DI ---
const int NUM_DI_PINS = 8;
const int diPins[NUM_DI_PINS] = {di1, di2, di3, di4, di5, di6, di7, di8};
bool pinIsPulsing[NUM_DI_PINS];      // Array de flags. Uma para cada pino DI.
unsigned long pulseStartTimes[NUM_DI_PINS]; // Array de cronômetros. Um para cada pino DI.
const long duracaoPulso = 2000;

// --- Variáveis para o Modo de Calibração e Botão ---
SystemState currentState = NORMAL;
int calibratedMin = 0;
int calibratedMax = 4095;
int calibMinTemp = 4095;
int calibMaxTemp = 0;
const int pinoBotao = offset;
bool ultimoEstadoBotao = HIGH;
unsigned long tempoUltimoDebounce = 0;
const long delayDebounce = 50;

// Bluetooth
BluetoothSerial SerialBT; 

// Progamação via OTA
// const char* ssid = "NOTEBOOK 3028";       
// const char* password = "j4]5428J";  

// IPAddress local_IP(192, 168, 137, 180); 
// IPAddress gateway(192, 168, 137, 1);    
// IPAddress subnet(255, 255, 255, 0);   

// const char* ota_hostname = "HV";     
// const char* ota_password = "4321"; 

// =================================================================
// === SETUP ===
// =================================================================

void setup() {
  Serial.begin(921600);
  Serial2.begin(115200, SERIAL_8N1, RXHV, TXHV); 
  SerialBT.begin("esp32HV");

  // if (!WiFi.config(local_IP, gateway, subnet)) {
  //   Serial.println("Falha ao configurar o IP estático");
  // }

  // // Inicializando beacon
  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //   Serial.println("Connection Failed! Rebooting...");
  //   delay(5000);
  //   ESP.restart();
  // }

  // Serial.println("Ready");
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());


  // // --- Configuração do OTA ---
  // ArduinoOTA.setHostname(ota_hostname);
  // ArduinoOTA.setPassword(ota_password);

  // // Funções de feedback (callbacks) para o processo de OTA.
  // // Elas são ótimas para depuração no Monitor Serial.
  // ArduinoOTA
  //   .onStart([]() {
  //     String type;
  //     if (ArduinoOTA.getCommand() == U_FLASH)
  //       type = "sketch";
  //     else // U_SPIFFS
  //       type = "filesystem";
  //     Serial.println("Start updating " + type);
  //   })
  //   .onEnd([]() {
  //     Serial.println("\nEnd");
  //   })
  //   .onProgress([](unsigned int progress, unsigned int total) {
  //     Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  //   })
  //   .onError([](ota_error_t error) {
  //     Serial.printf("Error[%u]: ", error);
  //     if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  //     else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  //     else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  //     else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  //     else if (error == OTA_END_ERROR) Serial.println("End Failed");
  //   });

  // ArduinoOTA.begin();
  

  // Portas digitais ESP-->INV
  for (int i = 0; i < NUM_DI_PINS; i++) {
    pinMode(diPins[i], OUTPUT);
  }

  // Inicializa o estado de todos os pulsos como 'falso'
  for (int i = 0; i < NUM_DI_PINS; i++) {
    pinIsPulsing[i] = false;
  }
  
  // Portas digitais INV-->ESP 
  pinMode(do2, INPUT);
  pinMode(do3, INPUT);
  pinMode(do4, INPUT);

  // TPS's
  pinMode(ai1, OUTPUT);
  pinMode(ai2, OUTPUT);

  // Portas analógicas INV-->ESP
  pinMode(ao1, INPUT);
  pinMode(ao2, INPUT);

  // Configuração do OFFSET
  pinMode(offset, INPUT);
  pinMode(led, OUTPUT); // LED para indicar o estado de calibração

  // Frequência de 5kHz, Resolução de 12 bits (valores de 0 a 4095)  
  ledcSetup(0, 5000, 12);
  ledcSetup(1, 5000, 12);

  // Associa os pinos aos canais configurados
  ledcAttachPin(ai1, 0); // Pino 33 no Canal 0
  ledcAttachPin(ai2, 1); // Pino 32 no Canal 1
}

// =================================================================
// === FUNÇÕES DE APOIO ===
// =================================================================

void parseAndExecute(char* data) {
    Serial.print("Processando dados: ");
    Serial.println(data);

    char* command;
    char* param1;
    char* param2;

    char tempData[numChars];
    strcpy(tempData, data);

    // Pega o primeiro "pedaço" (o comando) antes do primeiro delimitador ':'
    command = strtok(tempData, ":");

    // Se o comando for nulo, a string estava vazia ou mal formada.
    if (command == NULL) {
        Serial.println("Erro: Comando nulo.");
        return;
    }

    if (strcmp(command, "DI") == 0) {
        // Comando esperado: "DI:pino,estado" (ex: "DI:1,1")

        // Pega o próximo pedaço, usando a vírgula como delimitador
        param1 = strtok(NULL, ","); 
        param2 = strtok(NULL, ",");

        if (param1 == NULL || param2 == NULL) {
            Serial.println("Erro: Parâmetros faltando para o comando DI.");
            return;
        }

        // Converte o texto do parâmetro para um número inteiro
        int pinIndex = atoi(param1); // atoi = ASCII to Integer
        int state = atoi(param2);

        Serial.print("Comando DI. Pino: ");
        Serial.print(pinIndex);
        Serial.print(", Estado: ");
        Serial.println(state);

        // Validação do índice
        if (pinIndex < 1 || pinIndex > NUM_DI_PINS) {
            Serial.println("Erro: Índice de pino DI inválido.");
            return;
        }
        
        int arrayIndex = pinIndex - 1;
        int targetPin = diPins[arrayIndex]; 

        switch (state) {
            case 0: // Desligar
                digitalWrite(targetPin, LOW);
                Serial.print("Pino "); Serial.print(targetPin); Serial.println(" DESLIGADO.");
                break;
            case 1: // Ligar
                if (pinIsPulsing[arrayIndex]) {
                  Serial.print("Aviso: Pino "); Serial.print(targetPin); Serial.println(" já tem um pulso em andamento.");
                  return; 
                }
                SerialBT.print("Iniciando pulso de 2s no pino "); SerialBT.println(targetPin);
                
                // Inicia o pulso PARA ESTE PINO
                digitalWrite(targetPin, HIGH);
                pulseStartTimes[arrayIndex] = millis(); // Usa a posição do array para guardar o tempo
                pinIsPulsing[arrayIndex] = true;        // Usa a posição do array para levantar a flag
                break;
            default:
                Serial.println("Erro: Estado inválido para comando DI (use 0, 1 ou 2).");
                break;
        }
    } else if (strcmp(command, "AI") == 0) { 
        // Comando esperado: "AI:pino,valor" (ex: "AI:1,120")
        param1 = strtok(NULL, ",");
        param2 = strtok(NULL, ",");

        if (param1 == NULL || param2 == NULL) {
            Serial.println("Erro: Parâmetros faltando para o comando AI.");
            return;
        }

        int pinIndex = atoi(param1);
        int rawValue = atoi(param2); 

        if (currentState == CALIBRANDO) {
          // ESTADO DE CALIBRAÇÃO: Apenas registramos os valores min/max
          if (rawValue < calibMinTemp) calibMinTemp = rawValue;
          if (rawValue > calibMaxTemp) calibMaxTemp = rawValue;

          // Imprime feedback para o usuário
          Serial.print("Calibrando... Valor recebido: "); Serial.print(rawValue);
          Serial.print(" | Min Temp: "); Serial.print(calibMinTemp);
          Serial.print(" | Max Temp: "); Serial.println(calibMaxTemp);

        } else { // if (currentState == NORMAL)
          // ESTADO NORMAL: Mapeamos o valor e controlamos a saída
          long mappedValue = map(rawValue, calibratedMin, calibratedMax, 0, 4095);
          mappedValue = constrain(mappedValue, 0, 4095);

          SerialBT.print("Comando AI. Pino: "); SerialBT.print(pinIndex);
          SerialBT.print(", Valor Cru: "); SerialBT.print(rawValue);
          SerialBT.print(", Valor Mapeado: "); SerialBT.println(mappedValue);

          // Seleciona o canal com base no pinIndex e escreve o valor MAPEADO
          if (pinIndex == 1) {
            ledcWrite(0, mappedValue);
          } else if (pinIndex == 2) {
            ledcWrite(1, mappedValue);
          } else {
              Serial.println("Erro: Índice de pino AI inválido.");
              return;
          }
        }
    } else if (strcmp(command, "GET_STATUS") == 0) {
        // Comando esperado: "GET_STATUS"
        Serial.println("Comando GET_STATUS recebido.");

        // Lendo os valores dos sensores
        int val_do2 = digitalRead(do2);
        int val_do3 = digitalRead(do3);
        int val_do4 = digitalRead(do4);
        int val_ao1 = analogRead(ao1);
        int val_ao2 = analogRead(ao2);

        // Montando a string de resposta
        char responseBuffer[64];
        sprintf(responseBuffer, "STATUS:%d,%d,%d,%d,%d\n", val_do2, val_do3, val_do4, val_ao1, val_ao2);

        // Enviando a resposta de volta pela Serial2
        Serial2.print(responseBuffer);
        Serial.print("Enviando resposta: ");
        Serial.print(responseBuffer);

      } else {
        Serial.print("Erro: Comando desconhecido -> ");
        Serial.println(command);
      }
}

// =================================================================
// === LOOP PRINCIPAL ===
// =================================================================

void loop() {

  // Verifica se há atualizações OTA
  // ArduinoOTA.handle();

  // 1. LÓGICA DO BOTÃO PARA MUDAR O ESTADO
  bool estadoAtualBotao = digitalRead(pinoBotao);
  if (estadoAtualBotao != ultimoEstadoBotao) {
    tempoUltimoDebounce = millis();
  }
  if (!((millis() - tempoUltimoDebounce) > delayDebounce)) {
    if (estadoAtualBotao == LOW && ultimoEstadoBotao == HIGH) {
      if (currentState == NORMAL) {
        currentState = CALIBRANDO;
        calibMinTemp = 4095;
        calibMaxTemp = 0;
        SerialBT.println("\n===== MODO DE CALIBRAÇÃO INICIADO =====");
        digitalWrite(led, HIGH); // Liga o LED para indicar que está calibrando
      } else { // Estava em CALIBRANDO
        currentState = NORMAL;
        calibratedMin = calibMinTemp;
        calibratedMax = calibMaxTemp;
        if (calibratedMax <= calibratedMin) {
          SerialBT.println("ERRO DE CALIBRAÇÃO! Usando valores padrão.");
          calibratedMin = 0;
          calibratedMax = 4095;
          digitalWrite(led, HIGH); // Liga o LED para indicar erro
        }
        SerialBT.println("\n===== CALIBRAÇÃO FINALIZADA =====");
        SerialBT.print("Novo range: "); SerialBT.print(calibratedMin); SerialBT.print(" a "); SerialBT.println(calibratedMax);
        digitalWrite(led, LOW); // Desliga o LED ao sair do modo de calibração
      }
    }
  }
  ultimoEstadoBotao = estadoAtualBotao;

  // 2. RECEPÇÃO E PROCESSAMENTO DE DADOS

  while (Serial2.available() > 0) {
    char rc = Serial2.read();

    if (rc == '\n') { 
      receivedChars[charIndex] = '\0'; 
      parseAndExecute(receivedChars);  
      charIndex = 0;                   
    } else {
      if (charIndex < numChars - 1) {
        receivedChars[charIndex] = rc; 
        charIndex++;
      }
    }
  }

  // 3 . LÓGICA DE PULSOS PARA OS PINS DI

  for (int i = 0; i < NUM_DI_PINS; i++) {
    // Verifica se o pino 'i' está atualmente em modo de pulso
    if (pinIsPulsing[i]) {
        // Se estiver, verifica se o tempo dele já esgotou
        if (millis() - pulseStartTimes[i] >= duracaoPulso) {
            // O tempo acabou! Desliga o pino correspondente.
            // O pino físico está em diPins[i]
            digitalWrite(diPins[i], LOW); 
            pinIsPulsing[i] = false;       // Reseta a flag apenas para este pino

            Serial.print("Pulso finalizado no pino "); Serial.println(diPins[i]);
        }
    }
  }
}