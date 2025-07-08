#include "main.h"

// =================================================================
// === DEFINIÇÃO DE VARIÁVEIS GLOBAIS ===
// =================================================================

// --- Objeto CAN ---
MCP_CAN CAN0(CS);

// --- Variáveis de Comunicação ---
const byte numChars = 64;
char receivedChars[numChars];
boolean newData = false;

// --- Variáveis de Controle dos Pinos DI ---
const int NUM_DI_PINS = 8;
const int diPins[NUM_DI_PINS] = {di1, di2, di3, di4, di5, di6, di7, di8};
bool ultimoEstadoDI[NUM_DI_PINS];

// --- Variáveis para guardar o estado dos sensores ---
int valor_ai1 = 0;
int valor_ai2 = 0;
byte di_states = 0; 

// Recebidos da outra placa
int status_do2 = 0;
int status_do3 = 0;
int status_do4 = 0;
int status_ao1 = 0;
int status_ao2 = 0;

// --- Temporizadores para envio de dados ---
unsigned long ultimoEnvioControle = 0;
unsigned long ultimoEnvioCAN = 0;
const int INTERVALO_CONTROLE_MS = 20; // Envia comandos de controle a cada 20ms (50 Hz)
const int INTERVALO_CAN_MS = 100;     // Envia dados no CAN a cada 100ms (10 Hz)

// Progamação via OTA
const char* ssid = "NOTEBOOK 3028";       
const char* password = "j4]5428J";  

IPAddress local_IP(192, 168, 137, 181); 
IPAddress gateway(192, 168, 137, 1);    
IPAddress subnet(255, 255, 255, 0);   

const char* ota_hostname = "LV";     
const char* ota_password = "4321"; 

// =================================================================
// === SETUP ===
// =================================================================

void setup() {
  Serial.begin(921600);
  Serial2.begin(115200, SERIAL_8N1, RXHV, TXHV);

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Falha ao configurar o IP estático");
  }

  // Inicializando beacon
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  // --- Configuração do OTA ---
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);

  // Funções de feedback (callbacks) para o processo de OTA.
  // Elas são ótimas para depuração no Monitor Serial.
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

/*
  // --- Inicialização do Módulo CAN ---
  Serial.print("Iniciando CAN... ");
  // Tenta inicializar o CAN a 500kbps com um cristal de 8MHz
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("OK!");
  } else {
    Serial.println("Falha na inicialização!");
    while(1); // Trava se não conseguir comunicar com o módulo
  }

  // Coloca o módulo em modo Normal para poder transmitir na rede
  CAN0.setMode(MCP_NORMAL);
  Serial.println("CAN em modo Normal.");
*/

  // Portas digitais ESP-->INV
  for (int i = 0; i < NUM_DI_PINS; i++) {
    pinMode(diPins[i], INPUT_PULLDOWN); // retirar o pulldown no final
    ultimoEstadoDI[i] = digitalRead(diPins[i]); 
  }

  // Portas digitais INV-->ESP
  pinMode(do2, OUTPUT);
  pinMode(do3, OUTPUT);
  pinMode(do4, OUTPUT);

  // TPS's
  pinMode(ai1, INPUT);
  pinMode(ai2, INPUT);

  // Portas analógicas INV-->ESP
  pinMode(ao1, OUTPUT);
  pinMode(ao2, OUTPUT);

  // Frequência de 5kHz, Resolução de 12 bits (valores de 0 a 4095)  
  ledcSetup(0, 5000, 12);
  ledcSetup(1, 5000, 12);

  // Associa os pinos aos canais configurados
  ledcAttachPin(ao1, 0); // Pino 33 no Canal 0
  ledcAttachPin(ao2, 1); // Pino 32 no Canal 1
}

// =================================================================
// === FUNÇÕES DE APOIO ===
// =================================================================

void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    while (Serial2.available() > 0 && newData == false) {
        rc = Serial2.read();
        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        } else {
            receivedChars[ndx] = '\0'; // Termina a string
            ndx = 0;
            newData = true;
        }
    }
}

// =================================================================
// === LOOP PRINCIPAL ===
// =================================================================

void loop() {

  // Verifica se há atualizações OTA
  ArduinoOTA.handle();

  // --- Tarefa 1: Ler sensores locais (sempre) ---
  valor_ai1 = analogRead(ai1);
  valor_ai2 = analogRead(ai2);

  di_states = 0;
  for(int i=0; i<NUM_DI_PINS; i++){
    if(digitalRead(diPins[i]) == HIGH){
      di_states |= (1 << i);
    }
  }

  // --- Tarefa 2: Enviar comandos de controle via Serial2 ---
  if (millis() - ultimoEnvioControle >= INTERVALO_CONTROLE_MS) {
    char command_buffer[32];

    // 1. Envia o estado dos 2 sensores analógicos
    sprintf(command_buffer, "AI:1,%d\n", valor_ai1);
    Serial2.print(command_buffer);

    sprintf(command_buffer, "AI:2,%d\n", valor_ai2);
    Serial2.print(command_buffer);

    // 2. Envia o estado dos 8 sensores digitais
    // Percorre todos os pinos de entrada digital
    for (int i = 0; i < NUM_DI_PINS; i++) {
        bool estadoAtual = digitalRead(diPins[i]);

        // Se o estado mudou
        if (estadoAtual != ultimoEstadoDI[i]) {
            int pinIndex = i + 1;
            // Envia o novo estado (0 para desligado, 1 para ligado)
            sprintf(command_buffer, "DI:%d,%d\n", pinIndex, estadoAtual);
            Serial2.print(command_buffer);
            Serial.print("Mudança detectada no DI"); Serial.print(pinIndex);
            Serial.print(" para o estado "); Serial.println(estadoAtual);
        }
        // Atualiza a memória para a próxima verificação
        ultimoEstadoDI[i] = estadoAtual;
    }

    Serial2.print("GET_STATUS\n");

    // Atualiza o temporizador
    ultimoEnvioControle = millis();
  }

  // --- Tarefa 3: Receber e processar status da Serial2 ---
  recvWithEndMarker(); // Função que já criamos
  if (newData == true) {
    // Se o comando for "STATUS:d2,d3,d4,a1,a2"
    if (strncmp(receivedChars, "STATUS:", 7) == 0) {
      // Processa a string de status para atualizar as variáveis
      sscanf(receivedChars + 7, "%d,%d,%d,%d,%d", &status_do2, &status_do3, &status_do4, &status_ao1, &status_ao2);
      // Imprime para depuração
      // Serial.print("Recebido STATUS - DO2: "); Serial.println(status_do2);

      // Ativa as saídas digitais locais com base no status recebido
      digitalWrite(do2, status_do2);
      digitalWrite(do3, status_do3);
      digitalWrite(do4, status_do4);

      //retirar dps
      Serial.print("DO2: "); Serial.print(status_do2);
      Serial.print(" | DO3: "); Serial.print(status_do3);
      Serial.print(" | DO4: "); Serial.println(status_do4);

      // Ativa as saídas "analógicas" (PWM) locais com base no status recebido
      ledcWrite(0, status_ao1); // Canal 0 controla a saída ao1
      ledcWrite(1, status_ao2); // Canal 1 controla a saída ao2
    }
    newData = false;
  }
/*
  // --- Tarefa 4: Enviar dados no barramento CAN ---
  if (millis() - ultimoEnvioCAN >= INTERVALO_CAN_MS) {
    byte can_data[8]; // Buffer para os dados do CAN

    // Exemplo: Enviar valor do ai1 no ID 0x100
    // Empacotando um valor de 12 bits (0-4095) em 2 bytes
    can_data[0] = (valor_ai1 >> 8) & 0xFF; // Byte mais significativo
    can_data[1] = valor_ai1 & 0xFF;       // Byte menos significativo
    CAN0.sendMsgBuf(0x100, 0, 2, can_data); // Envia 2 bytes de dados 

    // Exemplo: Enviar os 8 estados digitais no ID 0x101
    can_data[0] = di_states;
    CAN0.sendMsgBuf(0x101, 0, 1, can_data); // Envia 1 byte de dados 

    // Exemplo: Enviar status recebido do outro ESP no ID 0x200
    can_data[0] = status_do2;
    can_data[1] = status_do3;
    can_data[2] = status_do4;
    CAN0.sendMsgBuf(0x200, 0, 3, can_data); // Envia 3 bytes de dados 
    
    Serial.println("-> Pacote CAN enviado");
    ultimoEnvioCAN = millis();
  }
*/
}

