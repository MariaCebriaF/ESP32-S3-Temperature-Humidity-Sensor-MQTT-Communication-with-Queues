/**
 * @file humiture.h
 * Generic functions for T&RH sensors
 */

#ifndef HUMITURE_H
#define HUMITURE_H

#include <stdbool.h>

void humiture_init(void);
bool humiture_read(float *temperature, float *rhumidity); // devuelve true si ha podido leerlo

#endif
