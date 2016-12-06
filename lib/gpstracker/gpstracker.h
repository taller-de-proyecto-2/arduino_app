/*
 * Sistema de seguimiento GPS.
 * 
 * Cátedra de Taller de Proyecto II
 * UNLP - 2016
 * 
 * Archivo cabecera de gps_tracker.ino
 * 
 * Autores:
 *  - Ailán, Julián
 *  - Hourquebie, Lucas
 */

/**************************************************************
 * Bibliotecas propias					      *
 *************************************************************/

#include <piezo_deb.h>

/**************************************************************
 * Bibliotecas externas					      *
 *************************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <FS.h>
#include <Ticker.h>

/**************************************************************
 * Definición de valores constantes.			      *
 *************************************************************/

const char* host = "gpstracker.hol.es";
const int httpPort = 80;

static const int RXPin = 12, TXPin = 13;
static const uint32_t GPSBaud = 9600;

/**************************************************************
 * Definición de enumerativos.				      *
 *************************************************************/

enum FSMStates {
  WIFI_CONFIG,
  WAITING,
  SAMPLING,
  NET_CONNECTING,
  UPLOADING
};

FSMStates state;

/**************************************************************
 * Comportamiento de los estados.			      *
 *************************************************************/

/*
 * finite_state_machine()
 * 
 * Gestiona la transición de estados de la aplicación.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void finite_state_machine(void);

/*
 * buildServer()
 * 
 * Genera un Access Point para la configuración
 * del archivo de redes.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void buildServer(void);

/*
 * saveConfiguration()
 * 
 * Actualiza el archivo de redes con los nuevos
 * valores indicados por el usuario.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static int saveConfiguration(void);

/*
 * gpsSampling()
 * 
 * Almacena en memoria las muestras generadas por
 * el módulo GPS.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void gpsSampling(void);

/*
 * connectionEstablishment()
 * 
 * Gestiona la conexión de la placa de desarrollo
 * a una red WiFi local.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void connectionEstablishment(void);

/*
 * dataUpload()
 * 
 * Envia las muestras almacenadas en memoria al 
 * servidor web para su procesamiento.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void dataUpload(void);

/**************************************************************
 * Utilidades.						      *
 *************************************************************/

/*
 * smartDelay()
 * 
 * Genera una demora de un cierto valor
 * sin realizar un uso poco óptimo de los recursos
 * como lo haría delay().
 * 
 * Entradas:
 *  ms: tiempo durante el cual se demora la ejecución
 *      del programa.
 *      
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void smartDelay(unsigned long ms);

/*
 * Rutinas de atención a interrupciones.
 */

/*
 * numberOfInterrupts()
 * 
 * Levanta un flag que indica la recepción de dos
 * toques del sensor piezoeléctrico.
 *  
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void numberOfInterrupts(void);

/*
 * configOrReady()
 * 
 * Describe el comportamiento del diodo LED rojo.
 *  
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */

static void configOrReady(void);

/*
 * performingTracking()
 * 
 * Describe el comportamiento del diodo LED verde.
 *  
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void performingTracking(void);

/*
 * uploading()
 * 
 * Describe el comportamiento del diodo LED amarillo.
 *  
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void uploading(void);

/**************************************************************
 * Funciones relativas al watchdog.			      *
 *************************************************************/

/*
 * feedWDT()
 * 
 * Simula condiciones de configuración mínima de hardware.
 * 
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
static void feedWDT(void);
