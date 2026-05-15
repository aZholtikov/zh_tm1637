/**
 * @file zh_tm1637.h
 */

#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"

/**
 * @brief TM1637 led drive initial default values.
 */
#define ZH_TM1637_INIT_CONFIG_DEFAULT() \
    {                                   \
        .clk_gpio = GPIO_NUM_MAX,       \
        .dio_gpio = GPIO_NUM_MAX}

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Structure for initial initialization of TM1637 led drive.
     */
    typedef struct
    {
        gpio_num_t clk_gpio; /*!< CLK GPIO. */
        gpio_num_t dio_gpio; /*!< DIO GPIO. */
    } zh_tm1637_init_config_t;

    /**
     * @brief TM1637 led drive handle.
     */
    typedef struct
    {
        bool is_initialized;                /*!< Led drive initialization flag. */
        int8_t brightness;                  /*!< Led drive brightness. */
        rmt_sync_manager_handle_t synchro;  /*!< Unique RMT TX device sync manager handle. */
        rmt_channel_handle_t tx_channel[2]; /*!< Unique RMT TX device handles. */
        rmt_encoder_handle_t copy_encoder;  /*!< Unique copy encoder handle. */
    } zh_tm1637_handle_t;

    /**
     * @brief Structure for error statistics storage.
     */
    typedef struct
    {
        uint32_t rmt_driver_error; /*!< Number of RMT driver error. */
    } zh_tm1637_stats_t;

    /**
     * @brief Initialize TM1637 led drive.
     *
     * @param[in] config Pointer to TM1637 initialized configuration structure. Can point to a temporary variable.
     * @param[out] handle Pointer to unique TM1637 handle.
     *
     * @attention I2C driver must be initialized first.
     *
     * @note Before initialize the expander recommend initialize zh_tm1637_init_config_t structure with default values.
     *
     * @code zh_tm1637_init_config_t config = ZH_TM1637_INIT_CONFIG_DEFAULT() @endcode
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_tm1637_init(const zh_tm1637_init_config_t *config, zh_tm1637_handle_t *handle);

    /**
     * @brief Deinitialize TM1637 led drive.
     *
     * @param[in] handle Pointer to unique TM1637 handle.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_tm1637_deinit(zh_tm1637_handle_t *handle);

    /**
     * @brief Prints a symbol to the LED.
     *
     * @param[in] handle Pointer to unique TM1637 handle.
     * @param[in] address Display address (0-5).
     * @param[in] symbols Symbol to be displayed.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_tm1637_print(zh_tm1637_handle_t *handle, uint8_t address, uint8_t symbol);

    /**
     * @brief Set brightness of the LED.
     *
     * @param[in] handle Pointer to unique TM1637 handle.
     * @param[in] brightness Brightness (0-7). -1 - LED OFF.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_tm1637_set_brightness(zh_tm1637_handle_t *handle, int8_t brightness);

    /**
     * @brief Get error statistics.
     *
     * @return Pointer to the statistics structure.
     */
    const zh_tm1637_stats_t *zh_tm1637_get_stats(void);

    /**
     * @brief Reset error statistics.
     */
    void zh_tm1637_reset_stats(void);

#ifdef __cplusplus
}
#endif