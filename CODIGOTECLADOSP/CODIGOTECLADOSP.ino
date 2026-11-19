#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

#include "AudioTools.h"
#include "FileSystems.h"
#include "espeak.h"

I2SStream i2s;
ESpeak espeak(i2s);

enum DeviceMode {
  MODE_LETTERS,
  MODE_WORDS,
  MODE_WINDOWS,
  MODE_OFF
};

DeviceMode currentMode = MODE_LETTERS;

bool previousFuncState = HIGH;
unsigned long funcPressTime = 0;
const unsigned long longPressTime = 1000;


const int rowPins[] = { 27, 26, 25 };
const int colPins[] = { 23, 22, 21, 19, 18, 17, 16, 15, 14, 13 };

const int FuncPin = 32;

const int NUM_ROWS = 3;
const int NUM_COLS = 10;

NimBLEHIDDevice* hid;
NimBLECharacteristic* inputKeyboard;

bool connected = false;

const char* keyLabels[NUM_ROWS][NUM_COLS] = {
  { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j" },
  { "k", "l", "m", "n", "ñ", "o", "p", "q", "r", "s" },
  { "t", "u", "v", "w", "x", "y", "z", ",", ".", " " }
};

// ======================================================
//              MAPEO DE TECLAS HID
// ======================================================

const uint8_t keyMap[NUM_ROWS][NUM_COLS] = {

  //       A      B      C      D      E      F      G      H      I      J
  { 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D },

  //       K      L      M      N      Ñ      O      P      Q      R      S
  { 0x0E, 0x0F, 0x10, 0x11, 0x33, 0x12, 0x13, 0x14, 0x15, 0x16 },

  //       T      U      V      W      X      Y      Z      ,      .     " "
  { 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x36, 0x37, 0x2C }
};

// Estado anterior de cada botón
bool previousState[NUM_ROWS][NUM_COLS] = { false };

String currentWord = "";

uint8_t pressedKeys[10] = { 0 };
int numPressedKeys = 0;

// ======================================================
//                  DEBOUNCE
// ======================================================

unsigned long lastDebounceTime[NUM_ROWS][NUM_COLS] = { 0 };

const unsigned long debounceDelay = 30;

// ======================================================
//                HID REPORT DESCRIPTOR
// ======================================================

const uint8_t keyboardReportMap[] = {

  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x06,  // Usage (Keyboard)
  0xA1, 0x01,  // Collection (Application)

  0x85, 0x01,  // Report ID (1)

  // Modifier keys
  0x05, 0x07,  // Usage Page (Keyboard)
  0x19, 0xE0,  // Usage Minimum (Left Control)
  0x29, 0xE7,  // Usage Maximum (Right GUI)
  0x15, 0x00,  // Logical Minimum
  0x25, 0x01,  // Logical Maximum
  0x75, 0x01,  // Report Size
  0x95, 0x08,  // Report Count
  0x81, 0x02,  // Input (Data, Variable, Absolute)

  // Reserved byte
  0x95, 0x01,
  0x75, 0x08,
  0x81, 0x01,

  // Keyboard LEDs
  0x95, 0x05,
  0x75, 0x01,
  0x05, 0x08,
  0x19, 0x01,
  0x29, 0x05,
  0x91, 0x02,

  0x95, 0x01,
  0x75, 0x03,
  0x91, 0x01,

  // Six keyboard keys
  0x95, 0x06,
  0x75, 0x08,
  0x15, 0x00,
  0x25, 0x65,
  0x05, 0x07,
  0x19, 0x00,
  0x29, 0x65,
  0x81, 0x00,

  0xC0  // End Collection
};

class ServerCallbacks : public NimBLEServerCallbacks {

  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {

    connected = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("TECLADO CONECTADO");
    Serial.println("================================");
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    connected = false;

    Serial.print("Teclado desconectado. Razón: ");
    Serial.println(reason);
  }
};

// ======================================================
//          ENVIAR ESTADO ACTUAL DEL TECLADO
// ======================================================

void sendKeyboardReport() {

  if (!connected) return;

  uint8_t report[8] = { 0 };

  for (int i = 0; i < numPressedKeys; i++) {
    report[2 + i] = pressedKeys[i];
  }

  inputKeyboard->setValue(report, sizeof(report));
  inputKeyboard->notify();
}

// CODIGO DE DIAGNOSTICO
// void sendKeyboardReport() {
//   if (!connected) {
//     Serial.println("ERROR: BLE no conectado");
//     return;
//   }

//   uint8_t report[8] = {0};

//   for (int i = 0; i < numPressedKeys; i++) {
//     report[2 + i] = pressedKeys[i];
//   }

//   inputKeyboard->setValue(report, sizeof(report));

//   bool sent = inputKeyboard->notify();

//   Serial.print("Reporte HID enviado. Teclas: ");
//   Serial.print(numPressedKeys);
//   Serial.print(" | Notificacion: ");
//   Serial.println(sent ? "OK" : "FALLO");
// }


// ======================================================
//                 PRESIONAR TECLA
// ======================================================

void keyDown(uint8_t keyCode) {

  if (!connected) return;

  for (int i = 0; i < numPressedKeys; i++) {
    if (pressedKeys[i] == keyCode) {
      return;
    }
  }

  if (numPressedKeys >= 6) {
    return;
  }

  pressedKeys[numPressedKeys] = keyCode;
  numPressedKeys++;

  sendKeyboardReport();
}


// ======================================================
//                  SOLTAR TECLA
// ======================================================

void keyUp(uint8_t keyCode) {

  if (!connected) return;

  for (int i = 0; i < numPressedKeys; i++) {

    if (pressedKeys[i] == keyCode) {

      for (int j = i; j < numPressedKeys - 1; j++) {
        pressedKeys[j] = pressedKeys[j + 1];
      }

      numPressedKeys--;

      break;
    }
  }

  sendKeyboardReport();
}

void processBrailleInput(int r, int c) {
  const char* label = keyLabels[r][c];

  if (currentMode == MODE_LETTERS) {

    const char* spokenLabel = label;

    if (r == 2 && c == 7) {
      spokenLabel = "coma";
    }
    else if (r == 2 && c == 8) {
      spokenLabel = "punto";
    }
    else if (r == 2 && c == 9) {
      spokenLabel = "espacio";
    }
    else if (r == 2 && c == 5) {
      spokenLabel = "y griega";
    }
    else if (r == 2 && c == 3) {
      spokenLabel = "b doble";
    }

    Serial.print("Pronunciando: ");
    Serial.println(spokenLabel);

    if (!espeak.say(spokenLabel)) {
      Serial.println("ERROR: No se pudo sintetizar.");
    }
  } 

  else if (currentMode == MODE_WORDS) {

    const char* spokenLabel = label;

    if (c == 9 && r == 2) {
      // La tecla ESPACIO confirma la palabra
      Serial.print("Palabra: ");
      Serial.println(currentWord);

      if (currentWord.length() > 0) {
        String palabraVoz = currentWord;
        palabraVoz.replace("ñ", "ny");
        palabraVoz.replace("Ñ", "Ny");

        espeak.say(palabraVoz.c_str());
      }

      currentWord = "";
    } else if (c == 8 && r == 2) {
      // La tecla PUNTO se añade a la cadena de la palabra
      currentWord += '.';
      Serial.print("Palabra: ");
      Serial.println(currentWord);
    } else if (c == 7 && r == 2) {
      // La tecla COMA se añade a la palabra
      currentWord += ',';
      currentWord += ' ';
      Serial.print("Palabra: ");
      Serial.println(currentWord);
    } else {
      currentWord += spokenLabel;

      Serial.print("Texto: ");
      Serial.println(currentWord);

      Serial.print("Bytes: ");
      for (size_t i = 0; i < currentWord.length(); i++) {
        Serial.print((uint8_t)currentWord[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
}

// ======================================================
//                    SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  if (!espeak.begin()) {
    Serial.println("ERROR: No se pudo iniciar eSpeak.");
    return;
  }

  Serial.println("eSpeak iniciado correctamente.");

  if (!espeak.setVoice("es")) {
    Serial.println("ERROR: No se pudo seleccionar la voz en español.");
    return;
  }

  Serial.println("Voz en español seleccionada.");
  
  audio_info espeak_info = espeak.audioInfo();

  auto cfg = i2s.defaultConfig();
  cfg.channels = espeak_info.channels;
  cfg.sample_rate = espeak_info.sample_rate;
  cfg.bits_per_sample = espeak_info.bits_per_sample;

  cfg.pin_bck = 33;   // BCLK
  cfg.pin_ws = 4;     // LRC
  cfg.pin_data = 5;   // DIN

  i2s.begin(cfg);

  // Serial.println("Probando síntesis de voz...");

  // if (espeak.say("Hola, esta es una prueba de voz en español.")) {
  //   Serial.println("Solicitud de síntesis aceptada.");
  // } else {
  //   Serial.println("ERROR: Falló la síntesis.");
  // }

  Serial.println();
  Serial.println("================================");
  Serial.println("BRAILLE KEYBOARD");
  Serial.println("ESP32 + BLE HID");
  Serial.println("================================");

  pinMode(FuncPin, INPUT_PULLUP);

  // ----------------------------------------------------
  // Configurar matriz
  // ----------------------------------------------------

  for (int r = 0; r < NUM_ROWS; r++) {

    pinMode(rowPins[r], OUTPUT);

    // Estado inicial HIGH
    digitalWrite(rowPins[r], HIGH);
  }

  for (int c = 0; c < NUM_COLS; c++) {

    pinMode(colPins[c], INPUT_PULLUP);
  }

  Serial.println("Matriz 3x10 configurada.");

  // ----------------------------------------------------
  // BLE
  // ----------------------------------------------------

  NimBLEDevice::init("Braille Keyboard");

  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(
    BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(
    BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  NimBLEServer* server =
    NimBLEDevice::createServer();

  server->setCallbacks(
    new ServerCallbacks());

  // Volver a anunciar después de desconectarse.
  //
  // OJO:
  // Windows puede tener problemas de reconexión
  // con este HID después del primer emparejamiento.
  // Ya comprobamos que el primer emparejamiento funciona.

  //server->advertiseOnDisconnect(true);

  // ----------------------------------------------------
  // HID
  // ----------------------------------------------------

  hid = new NimBLEHIDDevice(server);

  inputKeyboard =
    hid->getInputReport(1);

  hid->setManufacturer("UTEC");

  hid->setPnp(
    0x02,
    0x1234,
    0x5679,  // PID nuevo
    0x0100);

  hid->setHidInfo(
    0x00,  // country
    0x01   // HID version
  );

  hid->setReportMap(
    (uint8_t*)keyboardReportMap,
    sizeof(keyboardReportMap));

  hid->setBatteryLevel(100);

  // ----------------------------------------------------
  // Servicios HID
  // ----------------------------------------------------

  hid->startServices();

  // ----------------------------------------------------
  // Advertising
  // ----------------------------------------------------

  NimBLEAdvertising* advertising =
    NimBLEDevice::getAdvertising();

  advertising->setName("Braille Keyboard");

  advertising->addServiceUUID(
    hid->getHidService()->getUUID());

  // Apariencia BLE: teclado HID
  advertising->setAppearance(0x03C1);

  advertising->enableScanResponse(true);

  NimBLEDevice::startAdvertising();

  Serial.println("BLE HID iniciado.");
  Serial.println("Iniciando prueba de voz...");
  espeak.say("Hola, esta es una prueba de voz.");
  Serial.println("Prueba de voz finalizada.");
  Serial.println("Esperando conexión...");
}

// ======================================================
//                     LOOP
// ======================================================


void loop() {
  // Leer el boton de funcion
  bool funcState = digitalRead(FuncPin);

  // Detectar cuando se presiona
  if (previousFuncState == HIGH && funcState == LOW) {
    funcPressTime = millis();
  }

  // Detectar cuando se suelta
  if (previousFuncState == LOW && funcState == HIGH) {
    unsigned long pressDuration = millis() - funcPressTime;

    if (pressDuration >= longPressTime) {
      currentMode = MODE_OFF;
      Serial.println("Modo: APAGADO LOGICO");
    } else {
      currentMode = (DeviceMode)((currentMode + 1) % 3);
      Serial.print("Modo: ");
      Serial.println(currentMode == MODE_LETTERS ? "LETRAS"
                     : currentMode == MODE_WORDS ? "PALABRAS"
                                                 : "WINDOWS");
    }
  }
  previousFuncState = funcState;
  // Escanear cada fila
  for (int r = 0; r < NUM_ROWS; r++) {

    // Todas las filas HIGH
    for (int rr = 0; rr < NUM_ROWS; rr++) {
      digitalWrite(rowPins[rr], HIGH);
    }

    // Activar la fila actual
    digitalWrite(rowPins[r], LOW);
    delayMicroseconds(50);

    // Leer columnas
    for (int c = 0; c < NUM_COLS; c++) {
      bool pressed = (digitalRead(colPins[c]) == LOW);

      // Solo actuar cuando cambia el estado
      if (pressed != previousState[r][c]) {
        previousState[r][c] = pressed;

        uint8_t keyCode = keyMap[r][c];

        if (pressed) {
          // Serial.print("Tecla DOWN: ");
          // Serial.println(keyLabels[r][c]);

          // Rama A: teclado por Bluetooth
          if (currentMode == MODE_WINDOWS && connected) {
            keyDown(keyCode);
          }

          // Rama B: proyecto Braille
          else if (currentMode == MODE_LETTERS || currentMode == MODE_WORDS) {
            processBrailleInput(r, c);
          }
        } else {
          // Serial.print("Tecla UP: ");
          // Serial.println(keyLabels[r][c]);

          if (currentMode == MODE_WINDOWS && connected) {
            keyUp(keyCode);
          }
        }
      }
    }
  }
  delay(1);
}