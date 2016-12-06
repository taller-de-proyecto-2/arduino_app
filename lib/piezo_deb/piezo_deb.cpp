/*
 * Sistema de seguimiento GPS.
 * 
 * Cátedra de Taller de Proyecto II
 * UNLP - 2016
 * 
 * Archivo cabecera de piezo_deb.cpp
 * 
 * Autores:
 *  - Ailán, Julián
 *  - Hourquebie, Lucas
 */

#include "Arduino.h"
#include "piezo_deb.h"

int times = 0;
unsigned long lastDebounceTime = 0, reading = 0;

void increment() {
  reading = millis();
  if((reading - lastDebounceTime) > DEBOUNCE_DELAY) {
    lastDebounceTime = reading;
    times++;
  }
}

int getTimes(void) {
  return times;
}

void setTimes(int t) {
  times = t;
}
