#include <TFT_eSPI.h>
#include "iconos_rover.h.h"
#include "joystick.h.h"
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>
#include <math.h>

// ============================================================
// ROVER V1 - MANDO ESP32
// ESP-NOW + joystick + pantalla + sensibilidad 1...5
// ============================================================

TFT_eSPI tft = TFT_eSPI();
uint16_t VERDE_ELEC = 0;

const char* opcionesMenu[] = {"CONTROL", "AJUSTES"};
const int num_opciones = 2;
int opcionSeleccionada = 0;

const int BotonArriba = 22;
const int BotonAbajo = 21;
const int BotonOK = 23;
int estadoAnteriorArriba = HIGH;
int estadoAnteriorAbajo = HIGH;
int estadoAnteriorOK = HIGH;

unsigned long ultimoCambioBoton = 0;
const unsigned long tiempoAntiRebote = 150;

const int PIN_BUZZER = 5;
bool dentroDeApartado = false;

unsigned long ultimoMov = 0;
bool pantallaApagada = false;

// ============================================================
// ESP-NOW
// ============================================================

typedef struct {
  int16_t ejeX;          // -255 .. 255
  int16_t ejeY;          // -255 .. 255
  uint8_t sensibilidad;  // 1 .. 5
  uint32_t secuencia;
} DatosMando;

DatosMando datos;

// MAC WiFi STA del ESP32 del Rover
uint8_t macRover[] = {0x58, 0x2A, 0xBD, 0x7D, 0x09, 0x28};

bool espNowListo = false;
volatile bool ultimoEnvioOK = false;
volatile unsigned long ultimoACK = 0;
volatile uint32_t enviosOK = 0;
volatile uint32_t enviosTotales = 0;

// Calidad = porcentaje de entregas ESP-NOW de las ultimas muestras.
const int VENTANA_CALIDAD = 30;
bool historialEnvios[VENTANA_CALIDAD] = {false};
int indiceHistorial = 0;
int muestrasHistorial = 0;
int exitosHistorial = 0;
volatile bool hayResultadoEnvio = false;
volatile bool resultadoEnvioPendiente = false;

unsigned long ultimoEnvio = 0;
const unsigned long INTERVALO_ENVIO_MS = 30; // ~33 Hz
uint32_t numeroSecuencia = 0;

// ============================================================
// CONTROL
// ============================================================

int sensibilidadJoystick = 3;  // 1 minimo, 5 maximo
int pwmDerecha = 0;
int pwmIzquierda = 0;

const int DEADZONE_ADC = 120;

// Limite PWM asociado a cada nivel de sensibilidad.
// indice 0 no se usa.
const int PWM_MAX_NIVEL[6] = {0, 95, 135, 175, 215, 255};

// ============================================================
// UI
// ============================================================

unsigned long ultimaActualizacionUI = 0;
const unsigned long INTERVALO_UI_MS = 120;

// ============================================================
// PROTOTIPOS
// ============================================================

void enviarDatos();
void actualizarControl();
void actualizarCalidadDesdeCallback();
int calcularCalidadConexion();
int normalizarEje(int lectura, int centro);
void mezclarMotores(int giro, int avance, int &derecha, int &izquierda);
void dibujarCabecera();
void dibujarEstadoConexion(bool forzar = false);
void dibujarPantallaControl();
void actualizarPantallaControl();
void dibujarPantallaAjustes();
void dibujarBarraSensibilidad();
void abrirOpcionSeleccionada();
void dibujarMenu();
void dibujarOpcion(int indice);
void animarSeleccion(int y);
void actualizarBotones();
void sonidoMover();
void sonidoEntrar();
void pantallaArranque();
void pantallaCalibracionJoystick();
void animacionCalibracion();
void pantallaCalibracionLista();

void redibujarPantalla();


// ============================================================
// CALLBACK DE ENVIO ESP-NOW (ESP32 core 2.x)
// ============================================================

void alEnviar(const uint8_t *mac_addr, esp_now_send_status_t estado) {
  bool ok = (estado == ESP_NOW_SEND_SUCCESS);
  ultimoEnvioOK = ok;
  enviosTotales++;
  if (ok) {
    enviosOK++;
    ultimoACK = millis();
  }


  resultadoEnvioPendiente = ok;
  hayResultadoEnvio = true;
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  iniciarJoystick();

  pinMode(BotonArriba, INPUT_PULLUP);
  pinMode(BotonAbajo, INPUT_PULLUP);
  pinMode(BotonOK, INPUT_PULLUP);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  tft.init();
  tft.setRotation(2);
  VERDE_ELEC = tft.color565(50, 169, 86);

  pantallaArranque();
  dibujarMenu();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
  } else {
    esp_now_register_send_cb(alEnviar);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macRover, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      espNowListo = true;
      Serial.println("ESP-NOW listo. Rover registrado.");
    } else {
      Serial.println("Error anadiendo el Rover como peer");
    }
  }

  
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  actualizarBotones();
  actualizarCalidadDesdeCallback();


  actualizarControl();

  if (espNowListo && millis() - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    ultimoEnvio = millis();
    enviarDatos();
  }

  if (!pantallaApagada && millis() - ultimaActualizacionUI >= INTERVALO_UI_MS) {
    ultimaActualizacionUI = millis();

    dibujarEstadoConexion();

    if (dentroDeApartado && opcionSeleccionada == 0) {
      actualizarPantallaControl();
    }
  }

  
}

// ============================================================
// ESP-NOW: ENVIO
// ============================================================

void enviarDatos() {
  if (!espNowListo) return;

  datos.sensibilidad = sensibilidadJoystick;
  datos.secuencia = numeroSecuencia++;

  esp_err_t resultado = esp_now_send(
    macRover,
    (uint8_t *)&datos,
    sizeof(datos)
  );


  if (resultado != ESP_OK) {
    ultimoEnvioOK = false;
  }
}

void actualizarCalidadDesdeCallback() {
  if (!hayResultadoEnvio) return;

  noInterrupts();
  bool ok = resultadoEnvioPendiente;
  hayResultadoEnvio = false;
  interrupts();

  if (muestrasHistorial < VENTANA_CALIDAD) {
    historialEnvios[indiceHistorial] = ok;
    if (ok) exitosHistorial++;
    muestrasHistorial++;
  } else {
    if (historialEnvios[indiceHistorial]) exitosHistorial--;
    historialEnvios[indiceHistorial] = ok;
    if (ok) exitosHistorial++;
  }

  indiceHistorial = (indiceHistorial + 1) % VENTANA_CALIDAD;
}

int calcularCalidadConexion() {
  
  if (!espNowListo || ultimoACK == 0 || millis() - ultimoACK > 800) {
    return 0;
  }

  if (muestrasHistorial == 0) return 0;
  return (exitosHistorial * 100) / muestrasHistorial;
}

// ============================================================
// JOYSTICK Y MEZCLA DIFERENCIAL
// ============================================================

int normalizarEje(int lectura, int centro) {
  int delta = lectura - centro;

  if (abs(delta) <= DEADZONE_ADC) return 0;

  if (delta > 0) {
    long valor = map(lectura, centro + DEADZONE_ADC, 4095, 0, 255);
    return constrain((int)valor, 0, 255);
  } else {
    long valor = map(lectura, centro - DEADZONE_ADC, 0, 0, -255);
    return constrain((int)valor, -255, 0);
  }
}

void mezclarMotores(int giro, int avance, int &derecha, int &izquierda) {

  izquierda = avance + giro;
  derecha = avance - giro;

  int maximo = max(abs(izquierda), abs(derecha));
  if (maximo > 255) {
    izquierda = (izquierda * 255) / maximo;
    derecha = (derecha * 255) / maximo;
  }

  izquierda = constrain(izquierda, -255, 255);
  derecha = constrain(derecha, -255, 255);
}

void actualizarControl() {
  actualizarJoystick();

  int x = normalizarEje(obtenerJoystickX(), obtenerCentroJoystickX());
  int y = normalizarEje(obtenerJoystickY(), obtenerCentroJoystickY());


  int avance = y;
  int giro = x;

  int maxPWM = PWM_MAX_NIVEL[sensibilidadJoystick];
  avance = map(avance, -255, 255, -maxPWM, maxPWM);
  giro = map(giro, -255, 255, -maxPWM, maxPWM);

  mezclarMotores(giro, avance, pwmDerecha, pwmIzquierda);

  
  datos.ejeX = pwmDerecha;
  datos.ejeY = pwmIzquierda;
}

// ============================================================
// CABECERA / CONEXION
// ============================================================

void dibujarCabecera() {
  tft.fillRect(0, 0, 240, 38, TFT_DARKGREEN);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  tft.drawString("ROVER V1", 12, 19, 2);

  dibujarEstadoConexion(true);
}

void dibujarEstadoConexion(bool forzar) {
  static int calidadAnterior = -1;
  int calidad = calcularCalidadConexion();

  if (!forzar && calidad == calidadAnterior) return;
  calidadAnterior = calidad;

  tft.fillRect(144, 3, 92, 32, TFT_DARKGREEN);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(VERDE_ELEC, TFT_DARKGREEN);

  if (calidad == 0) {
    tft.drawString("SIN SENAL", 228, 19, 2);
  } else {
    String texto = String(calidad) + "%";
    tft.drawString(texto, 228, 19, 2);
  }
}

// ============================================================
// PANTALLA DE ARRANQUE
// ============================================================

void pantallaArranque() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("ROVER V1", 120, 75, 4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SISTEMA DE CONTROL", 120, 115, 2);

  for (int puntos = 0; puntos <= 3; puntos++) {
    tft.fillRect(40, 145, 180, 30, TFT_BLACK);
    String textoCarga = "Cargando";
    for (int i = 0; i < puntos; i++) textoCarga += ".";

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(textoCarga, 120, 160, 2);
    delay(350);
  }
}

// ============================================================
// MENU PRINCIPAL
// ============================================================

void dibujarMenu() {
  tft.fillScreen(TFT_BLACK);
  dibujarCabecera();

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("MENU PRINCIPAL", 120, 58, 2);

  for (int i = 0; i < num_opciones; i++) dibujarOpcion(i);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Usa los botones para navegar", 120, 255, 2);
}

void dibujarOpcion(int indice) {
  int y = 96 + indice * 40;
  bool seleccionada = (indice == opcionSeleccionada);

  tft.fillRoundRect(42, y - 14, 156, 29, 6, TFT_BLACK);

  if (seleccionada) {
    tft.fillRoundRect(42, y - 14, 156, 29, 6, VERDE_ELEC);
    tft.setTextColor(TFT_WHITE, VERDE_ELEC);
  } else {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  }

  tft.setTextDatum(MC_DATUM);
  tft.drawString(opcionesMenu[indice], 120, y, 2);
}

void animarSeleccion(int y) {
  for (int ancho = 0; ancho <= 156; ancho += 16) {
    tft.fillRoundRect(42, y - 14, 156, 29, 6, TFT_BLACK);
    tft.fillRoundRect(42, y - 14, ancho, 29, 6, VERDE_ELEC);
    delay(10);
  }
}

// ============================================================
// BOTONES
// ============================================================

void actualizarBotones() {
  int estadoBotonArriba = digitalRead(BotonArriba);
  int estadoBotonAbajo = digitalRead(BotonAbajo);
  int estadoBotonOK = digitalRead(BotonOK);

  bool puedeCambiar = millis() - ultimoCambioBoton >= tiempoAntiRebote;

  if (estadoBotonArriba == LOW && estadoAnteriorArriba == HIGH && puedeCambiar) {
    if (pantallaApagada) {
      
    } else {
      

      if (dentroDeApartado && opcionSeleccionada == 1) {
        
        if (sensibilidadJoystick < 5) {
          sensibilidadJoystick++;
          sonidoMover();
          dibujarBarraSensibilidad();
        }
      } else if (!dentroDeApartado && opcionSeleccionada > 0) {
        int anterior = opcionSeleccionada;
        opcionSeleccionada--;
        sonidoMover();
        dibujarOpcion(anterior);
        dibujarOpcion(opcionSeleccionada);
      }
    }
    ultimoCambioBoton = millis();
  }

  if (estadoBotonAbajo == LOW && estadoAnteriorAbajo == HIGH && puedeCambiar) {
    if (pantallaApagada) {
      
    } else {
      

      if (dentroDeApartado && opcionSeleccionada == 1) {
        
        if (sensibilidadJoystick > 1) {
          sensibilidadJoystick--;
          sonidoMover();
          dibujarBarraSensibilidad();
        }
      } else if (!dentroDeApartado && opcionSeleccionada < num_opciones - 1) {
        int anterior = opcionSeleccionada;
        opcionSeleccionada++;
        sonidoMover();
        dibujarOpcion(anterior);
        dibujarOpcion(opcionSeleccionada);
      }
    }
    ultimoCambioBoton = millis();
  }

  if (estadoBotonOK == LOW && estadoAnteriorOK == HIGH && puedeCambiar) {
    if (pantallaApagada) {
      
    } else {
      
      sonidoEntrar();

      if (!dentroDeApartado) {
        dentroDeApartado = true;
        abrirOpcionSeleccionada();
      } else {
        dentroDeApartado = false;
        dibujarMenu();
      }
    }
    ultimoCambioBoton = millis();
  }

  estadoAnteriorArriba = estadoBotonArriba;
  estadoAnteriorAbajo = estadoBotonAbajo;
  estadoAnteriorOK = estadoBotonOK;
}

// ============================================================
// SONIDOS
// ============================================================

void sonidoMover() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(35);
  digitalWrite(PIN_BUZZER, LOW);
}

void sonidoEntrar() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(45);
  digitalWrite(PIN_BUZZER, LOW);
  delay(45);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(45);
  digitalWrite(PIN_BUZZER, LOW);
}

// ============================================================
// CONTROL
// ============================================================

void abrirOpcionSeleccionada() {
  if (opcionSeleccionada == 0) {
    pantallaCalibracionJoystick();
    pantallaCalibracionLista();
    delay(700);
    dibujarPantallaControl();
  } else {
    dibujarPantallaAjustes();
  }
}

void dibujarPantallaControl() {
  tft.fillScreen(TFT_BLACK);
  dibujarCabecera();

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CONTROL", 120, 58, 2);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("PWM MOTORES", 120, 88, 2);

  
  tft.drawRoundRect(14, 105, 101, 82, 7, VERDE_ELEC);
  tft.drawRoundRect(125, 105, 101, 82, 7, VERDE_ELEC);

  tft.setTextColor(VERDE_ELEC, TFT_BLACK);
  tft.drawString("IZQ", 64, 120, 2);
  tft.drawString("DER", 175, 120, 2);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("0-255", 120, 210, 2);
  tft.drawString("OK: volver", 120, 270, 2);

  actualizarPantallaControl();
}

void actualizarPantallaControl() {
  static int izqAnterior = -999;
  static int derAnterior = -999;

  int izq = abs(pwmIzquierda);
  int der = abs(pwmDerecha);

  if (izq != izqAnterior) {
    tft.fillRect(22, 137, 85, 38, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(VERDE_ELEC, TFT_BLACK);
    tft.drawNumber(izq, 64, 155, 4);
    izqAnterior = izq;
  }

  if (der != derAnterior) {
    tft.fillRect(133, 137, 85, 38, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(VERDE_ELEC, TFT_BLACK);
    tft.drawNumber(der, 175, 155, 4);
    derAnterior = der;
  }

  // Barra visual de potencia media
  int media = (izq + der) / 2;
  int ancho = map(media, 0, 255, 0, 190);
  tft.drawRoundRect(25, 225, 190, 18, 4, VERDE_ELEC);
  tft.fillRect(29, 229, 182, 10, TFT_BLACK);
  if (ancho > 8) {
    tft.fillRect(29, 229, min(ancho - 8, 182), 10, VERDE_ELEC);
  }
}

// ============================================================
// AJUSTES / SENSIBILIDAD
// ============================================================

void dibujarPantallaAjustes() {
  tft.fillScreen(TFT_BLACK);
  dibujarCabecera();

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("AJUSTES", 120, 58, 2);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("SENSIBILIDAD JOYSTICK", 120, 94, 2);

  dibujarBarraSensibilidad();

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("ARRIBA: +   ABAJO: -", 120, 220, 2);
  tft.drawString("OK: volver", 120, 255, 2);
}

void dibujarBarraSensibilidad() {
  
  const int x = 20;
  const int y = 125;
  const int w = 200;
  const int h = 58;

  tft.fillRoundRect(x, y, w, h, 8, VERDE_ELEC);
  tft.fillRoundRect(x + 5, y + 5, w - 10, h - 10, 6, TFT_BLACK);

  const int cuadW = 28;
  const int cuadH = 28;
  const int separacion = 7;
  const int totalW = 5 * cuadW + 4 * separacion;
  const int inicioX = 120 - totalW / 2;
  const int cuadY = y + (h - cuadH) / 2;

  for (int i = 0; i < 5; i++) {
    int cx = inicioX + i * (cuadW + separacion);

    if (i < sensibilidadJoystick) {
      tft.fillRoundRect(cx, cuadY, cuadW, cuadH, 4, VERDE_ELEC);
    } else {
      tft.drawRoundRect(cx, cuadY, cuadW, cuadH, 4, VERDE_ELEC);
      tft.fillRect(cx + 3, cuadY + 3, cuadW - 6, cuadH - 6, TFT_BLACK);
    }
  }

  tft.fillRect(80, 190, 80, 22, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(VERDE_ELEC, TFT_BLACK);
  tft.drawString(String(sensibilidadJoystick) + "/5", 120, 200, 2);
}

// ============================================================
// CALIBRACION
// ============================================================

void pantallaCalibracionJoystick() {
  tft.fillScreen(TFT_BLACK);
  dibujarCabecera();

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CONTROL", 120, 58, 2);
  tft.drawString("NO TOQUE", 120, 130, 4);
  tft.drawString("EL JOYSTICK", 120, 170, 4);

  animacionCalibracion();
  calibrarCentroJoystick();
}

void animacionCalibracion() {
  for (int puntos = 1; puntos <= 3; puntos++) {
    String texto = "CALIBRANDO";
    for (int i = 0; i < puntos; i++) texto += ".";

    tft.fillRect(25, 205, 190, 35, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(VERDE_ELEC, TFT_BLACK);
    tft.drawString(texto, 120, 220, 2);
    delay(250);
  }
}

void pantallaCalibracionLista() {
  tft.fillScreen(TFT_BLACK);
  dibujarCabecera();

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CONTROL", 120, 58, 2);

  tft.setTextColor(VERDE_ELEC, TFT_BLACK);
  tft.drawString("CALIBRACION LISTA", 120, 130, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("YA PUEDE USAR", 120, 170, 2);
  tft.drawString("EL JOYSTICK", 120, 205, 2);
}

// ============================================================
// PANTALLA 
// ============================================================


void redibujarPantalla() {
  if (dentroDeApartado) {
    if (opcionSeleccionada == 0) dibujarPantallaControl();
    else dibujarPantallaAjustes();
  } else {
    dibujarMenu();
  }
}


