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

/**************************************************************
 * Definición de macros.				      *
 *************************************************************/
#define INTERRUPT_PIN 15
#define DEBOUNCE_DELAY 150

/*
 * increment()
 *
 * Realiza el debouncing a partir de la diferencia de tiempo entre 
 * dos interrupciones sucesivas.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
void increment(void);

/*
 * getTimes()
 *
 * Retorna la cantidad de interrupciones en un dado
 * intervalo de tiempo.
 *
 * Entradas:
 *  No tiene parámetros de entrada. 
 * 
 * Salidas:
 *  times: número de interrupciones generadas por el sensor piezoeléctrico.
 */
int getTimes(void);

/*
 * setTimes()
 *
 * Asigna un valor fijo a la cantidad de interrupciones.
 *
 * Entradas:
 *  t: cantidad de interrupciones a asignar. 
 * 
 * Salidas:
 *  No tiene resultados que retornar.
 */
void setTimes(int);
