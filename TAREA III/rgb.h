#pragma once
#include <stdint.h>

/**
 * @brief RGB LED configuration.
 */
typedef struct {
    uint8_t red;        /**< 0-255 */
    uint8_t green;      /**< 0-255 */
    uint8_t blue;       /**< 0-255 */
    uint8_t brightness; /**< 0-100 % */
} rgb_config_t;

/**
 * @brief Initialize LEDC channels for RGB LED (GPIO6=R, GPIO7=G, GPIO10=B).
 */
void rgb_init(void);

/**
 * @brief Apply a color and brightness to the RGB LED.
 */
void rgb_set(const rgb_config_t *cfg);

/**
 * @brief Get current RGB configuration.
 */
void rgb_get(rgb_config_t *cfg);