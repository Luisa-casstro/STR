#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SERVO_SCHEDULE_MAX 8   /**< Maximum number of timed schedule records */

/**
 * @brief Servo (curtain) operating modes.
 */
typedef enum {
    SERVO_MODE_MANUAL    = 0,
    SERVO_MODE_SCHEDULED = 1,
} servo_mode_t;

/**
 * @brief A single schedule entry: open curtain to `percent` at `hour`.
 */
typedef struct {
    uint8_t hour;    /**< 0-23 */
    uint8_t percent; /**< 0-100 */
    bool    enabled;
} servo_schedule_t;

/**
 * @brief Full servo configuration.
 */
typedef struct {
    servo_mode_t     mode;
    uint8_t          manual_percent;            /**< 0-100 % (manual mode)    */
    servo_schedule_t schedule[SERVO_SCHEDULE_MAX];
} servo_config_t;

/**
 * @brief Initialize LEDC PWM for servo on GPIO8.
 */
void servo_init(void);

/**
 * @brief Move servo to a given curtain opening percentage (0=closed, 100=open).
 */
void servo_set_percent(uint8_t percent);

/**
 * @brief Update configuration and apply immediately (manual) or arm scheduler.
 */
void servo_set_config(const servo_config_t *cfg);

/**
 * @brief Get current configuration.
 */
void servo_get_config(servo_config_t *cfg);

/**
 * @brief Called every minute with the current hour to apply scheduled entries.
 */
void servo_tick(uint8_t current_hour);