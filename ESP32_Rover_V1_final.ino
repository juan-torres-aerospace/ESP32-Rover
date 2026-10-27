#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

// ============================================================
// ROVER V1 - ESP32 RECEPTOR
// ============================================================

typedef struct {
  int16_t ejeX;          
  int16_t ejeY;          
  uint8_t sensibilidad;  
  uint32_t secuencia;
} DatosMando;

DatosMando datos;


const int ENA = 5;
const int IN1 = 17;
const int IN2 = 16;
const int IN3 = 4;
const int IN4 = 0;
const int ENB = 2;

const bool INVERTIR_MOTOR_DERECHO = true;
const bool INVERTIR_MOTOR_IZQUIERDO = false;

volatile unsigned long ultimoPaquete = 0;
volatile bool paqueteNuevo = false;
volatile int comandoDerecha = 0;
volatile int comandoIzquierda = 0;

const unsigned long TIMEOUT_SENAL_MS = 350;

void controladorMotor(int velocidad, int pinA, int pinB, int pinPWM);
void motores(int derecha, int izquierda);
void pararMotores();


void recibirDatos(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(DatosMando)) return;

  memcpy(&datos, data, sizeof(datos));

  comandoDerecha = constrain((int)datos.ejeX, -255, 255);
  comandoIzquierda = constrain((int)datos.ejeY, -255, 255);
  ultimoPaquete = millis();
  paqueteNuevo = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  pararMotores();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("MAC WiFi Rover: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(recibirDatos);
  Serial.println("Rover listo. Esperando mando...");
}

void loop() {
  unsigned long ahora = millis();

  if (ultimoPaquete == 0 || ahora - ultimoPaquete > TIMEOUT_SENAL_MS) {
    pararMotores();
    return;
  }

  int derecha;
  int izquierda;

  noInterrupts();
  derecha = comandoDerecha;
  izquierda = comandoIzquierda;
  paqueteNuevo = false;
  interrupts();

  motores(derecha, izquierda);

  static unsigned long ultimoDebug = 0;
  if (ahora - ultimoDebug >= 250) {
    ultimoDebug = ahora;
    Serial.print("DER: ");
    Serial.print(derecha);
    Serial.print(" | IZQ: ");
    Serial.print(izquierda);
    Serial.print(" | Sens: ");
    Serial.println(datos.sensibilidad);
  }
}

void controladorMotor(int velocidad, int pinA, int pinB, int pinPWM) {
  velocidad = constrain(velocidad, -255, 255);

  if (velocidad > 0) {
    digitalWrite(pinA, HIGH);
    digitalWrite(pinB, LOW);
  } else if (velocidad < 0) {
    digitalWrite(pinA, LOW);
    digitalWrite(pinB, HIGH);
  } else {
    digitalWrite(pinA, LOW);
    digitalWrite(pinB, LOW);
  }

  analogWrite(pinPWM, abs(velocidad));
}

void motores(int derecha, int izquierda) {
  if (INVERTIR_MOTOR_DERECHO) derecha = -derecha;
  if (INVERTIR_MOTOR_IZQUIERDO) izquierda = -izquierda;

  controladorMotor(derecha, IN3, IN4, ENB);
  controladorMotor(izquierda, IN1, IN2, ENA);
}

void pararMotores() {
  controladorMotor(0, IN3, IN4, ENB);
  controladorMotor(0, IN1, IN2, ENA);
}
