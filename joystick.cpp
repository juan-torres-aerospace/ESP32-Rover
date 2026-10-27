#include <Arduino.h>
#include "joystick.h.h"

const int pinJoystickX = 34;
const int pinJoystickY = 35;

int valorX = 0;
int valorY = 0;

int centroJoystickX = 1850;
int centroJoystickY = 1850;

void iniciarJoystick() {
  pinMode(pinJoystickX, INPUT);
  pinMode(pinJoystickY, INPUT);
}

void actualizarJoystick() {
  valorX = analogRead(pinJoystickX);
  valorY = analogRead(pinJoystickY);
}

int obtenerJoystickX() {
  return valorX;
}

int obtenerJoystickY() {
  return valorY;
}

void calibrarCentroJoystick() {
  const int num_lecturas = 100;

  // Importante: reiniciar las sumas en cada calibracion.
  long sumaX = 0;
  long sumaY = 0;

  for (int i = 0; i < num_lecturas; i++) {
    sumaX += analogRead(pinJoystickX);
    sumaY += analogRead(pinJoystickY);
    delay(5);
  }

  centroJoystickX = sumaX / num_lecturas;
  centroJoystickY = sumaY / num_lecturas;
}

int obtenerCentroJoystickX() {
  return centroJoystickX;
}

int obtenerCentroJoystickY() {
  return centroJoystickY;
}
