#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Fan (ventilation) operating modes.
 */
typedef enum {
    FAN_MODE_AUTO   = 0,  /**< Proportional control based on temperature */
    FAN_MODE_MANUAL = 1,  /**< User sets speed directly                  */
} fan_mode_t;

/**
 * @brief Fan configuration.
 */
typedef struct {
    fan_mode_t mode;
    uint8_t    manual_percent;   /**< 0-100 % – only used in manual mode  */
    float      desired_temp;     /**< °C – fan OFF below this             */
    float      max_temp;         /**< °C – fan at 100 % at/above this     */
} fan_config_t;

/**
 * @brief Initialize LEDC PWM for fan (GPIO3) and alarm LED (GPIO4).
 */
void fan_init(void);

/**
 * @brief Update configuration (mode, thresholds, manual speed).
 */
void fan_set_config(const fan_config_t *cfg);

/**
 * @brief Get current configuration.
 */
void fan_get_config(fan_config_t *cfg);

/**
 * @brief Call periodically with the latest temperature reading.
 *        In AUTO mode: adjusts fan speed and alarm LED.
 *        In MANUAL mode: no-op (speed already set).
 */
void fan_tick(float temperature_celsius);

/**
 * @brief Returns true if the alarm LED is currently active.
 */
bool fan_alarm_active(void);