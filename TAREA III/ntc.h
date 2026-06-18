#pragma once
#include <stdint.h>

/**
 * @brief Initialize the NTC temperature sensor (ADC on GPIO2).
 */
void ntc_init(void);

/**
 * @brief Read current temperature in Celsius.
 * @return Temperature in °C (float).
 */
float ntc_read_celsius(void);