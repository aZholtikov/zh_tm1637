#include "zh_tm1637.h"

static const char *TAG = "zh_tm1637";

#define ZH_LOGI(msg, ...) ESP_LOGI(TAG, msg, ##__VA_ARGS__)
#define ZH_LOGE(msg, err, ...) ESP_LOGE(TAG, "[%s:%d:%s] " msg, __FILE__, __LINE__, esp_err_to_name(err), ##__VA_ARGS__)

#define ZH_ERROR_CHECK(cond, err, cleanup, msg, ...) \
    if (!(cond))                                     \
    {                                                \
        ZH_LOGE(msg, err, ##__VA_ARGS__);            \
        cleanup;                                     \
        return err;                                  \
    }

static zh_tm1637_stats_t _stats = {0};

static esp_err_t _zh_tm1637_validate_config(const zh_tm1637_init_config_t *config, zh_tm1637_handle_t *handle);
static esp_err_t _zh_tm1637_rmt_init(const zh_tm1637_init_config_t *config, zh_tm1637_handle_t *handle);
static esp_err_t _zh_tm1637_send_data(zh_tm1637_handle_t *handle, uint8_t *symbols, uint8_t size);

esp_err_t zh_tm1637_init(const zh_tm1637_init_config_t *config, zh_tm1637_handle_t *handle) // -V2008
{
    ZH_LOGI("Led drive initialization started.");
#if defined CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C2
    ZH_LOGE("Led drive initialization failed. Unsupported device.", ESP_ERR_NOT_ALLOWED);
    return ESP_ERR_NOT_ALLOWED;
#endif
    ZH_ERROR_CHECK(config != NULL && handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Led drive initialization failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == false, ESP_ERR_INVALID_STATE, NULL, "Led drive initialization failed. Led drive is already initialized.");
    ZH_ERROR_CHECK(_zh_tm1637_validate_config(config, handle) == ESP_OK, ESP_FAIL, NULL, "Led drive initialization failed. Initial configuration check failed.");
    ZH_ERROR_CHECK(_zh_tm1637_rmt_init(config, handle) == ESP_OK, ESP_FAIL, , "Led drive initialization failed. RMT initialization failed.");
    handle->brightness = 7;
    handle->is_initialized = true;
    ZH_LOGI("Led drive initialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_tm1637_deinit(zh_tm1637_handle_t *handle) // -V2008
{
    ZH_LOGI("Led drive deinitialization started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Led drive deinitialization failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive deinitialization failed. Led drive not initialized.");
    ZH_ERROR_CHECK(rmt_disable(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "Led drive deinitialization failed. TX channel disable failed.");
    ZH_ERROR_CHECK(rmt_disable(handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "Led drive deinitialization failed. TX channel disable failed.");
    ZH_ERROR_CHECK(rmt_del_encoder(handle->copy_encoder) == ESP_OK, ESP_FAIL, NULL, "Led drive deinitialization failed. Delete encoder failed.");
    ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "Led drive deinitialization failed. Delete TX channel failed.");
    ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "Led drive deinitialization failed. Delete TX channel failed.");
    ZH_LOGI("Led drive deinitialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_tm1637_print(zh_tm1637_handle_t *handle, uint8_t position, uint8_t *symbols, uint8_t size) // -V2008
{
    ZH_LOGI("Led drive printing started.");
    ZH_ERROR_CHECK(handle != NULL && symbols != NULL && position <= 5 && size > 0, ESP_ERR_INVALID_ARG, NULL, "Led drive printing failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive printing failed. Led drive not initialized.");
    uint8_t set_data_command = 0x40;
    ZH_ERROR_CHECK(_zh_tm1637_send_data(handle, &set_data_command, 1) == ESP_OK, ESP_FAIL, NULL, "Led drive printing failed. RMT driver error.");
    uint8_t print_symbols_command[size + 1];
    print_symbols_command[0] = position | 0xC0;
    memcpy(&print_symbols_command[1], symbols, size);
    ZH_ERROR_CHECK(_zh_tm1637_send_data(handle, print_symbols_command, size + 1) == ESP_OK, ESP_FAIL, NULL, "Led drive printing failed. RMT driver error.");
    uint8_t control_display_command = (handle->brightness == -1) ? 0x80 : handle->brightness | 0x88;
    ZH_ERROR_CHECK(_zh_tm1637_send_data(handle, &control_display_command, 1) == ESP_OK, ESP_FAIL, NULL, "Led drive printing failed. RMT driver error.");
    ZH_LOGI("Led drive printing completed successfully.");
    return ESP_OK;
}

esp_err_t zh_tm1637_set_brightness(zh_tm1637_handle_t *handle, int8_t brightness)
{
    ZH_LOGI("Led drive setup brightness started.");
    ZH_ERROR_CHECK(handle != NULL && brightness <= 7 && brightness >= -1, ESP_ERR_INVALID_ARG, NULL, "Led drive setup brightness failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive setup brightness failed. Led drive not initialized.");
    handle->brightness = brightness;
    ZH_LOGI("Led drive setup brightness completed successfully.");
    return ESP_OK;
}

const zh_tm1637_stats_t *zh_tm1637_get_stats(void)
{
    return &_stats;
}

void zh_tm1637_reset_stats(void)
{
    ZH_LOGI("Error statistic reset started.");
    _stats.rmt_driver_error = 0;
    ZH_LOGI("Error statistic reset successfully.");
}

static esp_err_t _zh_tm1637_validate_config(const zh_tm1637_init_config_t *config, zh_tm1637_handle_t *handle)
{
    ZH_ERROR_CHECK(config->clk_gpio < GPIO_NUM_MAX && config->dio_gpio < GPIO_NUM_MAX, ESP_ERR_INVALID_ARG, NULL, "Invalid GPIO number.")
    ZH_ERROR_CHECK(config->clk_gpio != config->dio_gpio, ESP_ERR_INVALID_ARG, NULL, "CLK GPIO and DIO GPIO is same.")
    return ESP_OK;
}

static esp_err_t _zh_tm1637_rmt_init(const zh_tm1637_init_config_t *config, zh_tm1637_handle_t *handle) // -V2008
{
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .mem_block_symbols = 48,
        .resolution_hz = 1000000,
        .trans_queue_depth = 4,
        .flags.init_level = 1,
    };
    tx_chan_config.gpio_num = config->clk_gpio;
    ZH_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "TX channel creation failed.");
    tx_chan_config.gpio_num = config->dio_gpio;
    ZH_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "TX channel creation failed.");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ZH_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &handle->copy_encoder) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "Delete channel failed.");
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "Delete channel failed."), "Copy encoder creation failed.");
    ZH_ERROR_CHECK(rmt_enable(handle->tx_channel[0]) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(rmt_del_encoder(handle->copy_encoder) == ESP_OK, ESP_FAIL, NULL, "Delete encoder failed.");
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "Delete channel failed.");
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "Delete channel failed."), "Enable TX channel failed.");
    ZH_ERROR_CHECK(rmt_enable(handle->tx_channel[1]) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(rmt_disable(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "TX channel disable failed.");
                   ZH_ERROR_CHECK(rmt_del_encoder(handle->copy_encoder) == ESP_OK, ESP_FAIL, NULL, "Delete encoder failed.");
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "Delete channel failed.");
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "Delete channel failed."), "Enable TX channel failed.");
    rmt_sync_manager_config_t synchro_config = {
        .tx_channel_array = handle->tx_channel,
        .array_size = sizeof(handle->tx_channel) / sizeof(handle->tx_channel[0]),
    };
    ZH_ERROR_CHECK(rmt_new_sync_manager(&synchro_config, &handle->synchro) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(rmt_disable(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "TX channel disable failed.");
                   ZH_ERROR_CHECK(rmt_disable(handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "TX channel disable failed.");
                   ZH_ERROR_CHECK(rmt_del_encoder(handle->copy_encoder) == ESP_OK, ESP_FAIL, NULL, "Delete encoder failed.");
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[0]) == ESP_OK, ESP_FAIL, NULL, "Delete TX channel failed.");
                   ZH_ERROR_CHECK(rmt_del_channel(handle->tx_channel[1]) == ESP_OK, ESP_FAIL, NULL, "Delete TX channel failed."), "Sync manager creation failed.");
    rmt_transmit_config_t tx_transmit_config = {.loop_count = 0, .flags.eot_level = 1};
    rmt_symbol_word_t init_symbols = {.duration0 = 10, .level0 = 1, .duration1 = 10, .level1 = 1};
    rmt_sync_reset(handle->synchro);
    ZH_ERROR_CHECK(rmt_transmit(handle->tx_channel[0], handle->copy_encoder, &init_symbols, sizeof(init_symbols), &tx_transmit_config) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    ZH_ERROR_CHECK(rmt_transmit(handle->tx_channel[1], handle->copy_encoder, &init_symbols, sizeof(init_symbols), &tx_transmit_config) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    ZH_ERROR_CHECK(rmt_tx_wait_all_done(handle->tx_channel[0], 1000 / portTICK_PERIOD_MS) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    ZH_ERROR_CHECK(rmt_tx_wait_all_done(handle->tx_channel[1], 1000 / portTICK_PERIOD_MS) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    return ESP_OK;
}

static esp_err_t _zh_tm1637_send_data(zh_tm1637_handle_t *handle, uint8_t *symbols, uint8_t size)
{
    uint8_t clk_symbols_counter = 0;
    uint8_t dio_symbols_counter = 0;
    uint8_t temp_symbols[size];
    memcpy(&temp_symbols, symbols, size);
    rmt_symbol_word_t clk_symbols[2 + (size * 9)];
    rmt_symbol_word_t dio_symbols[2 + (size * 9)];
    clk_symbols[clk_symbols_counter++] = (rmt_symbol_word_t){.duration0 = 2, .level0 = 1, .duration1 = 2, .level1 = 1}; // Start.
    dio_symbols[dio_symbols_counter++] = (rmt_symbol_word_t){.duration0 = 2, .level0 = 0, .duration1 = 3, .level1 = 0}; // Start.
    for (uint8_t i = 0; i < size; ++i)
    {
        for (uint8_t j = 0; j < 8; ++j)
        {
            clk_symbols[clk_symbols_counter++] = (rmt_symbol_word_t){.duration0 = 2, .level0 = 0, .duration1 = 2, .level1 = 1};                                                         // Data.
            dio_symbols[dio_symbols_counter++] = (rmt_symbol_word_t){.duration0 = 2, .level0 = (temp_symbols[i] >> j) & 0x01, .duration1 = 2, .level1 = (temp_symbols[i] >> j) & 0x01}; // Data.
        }
        clk_symbols[clk_symbols_counter++] = (rmt_symbol_word_t){.duration0 = 2, .level0 = 0, .duration1 = 2, .level1 = 1}; // Ask.
        dio_symbols[dio_symbols_counter++] = (rmt_symbol_word_t){.duration0 = 2, .level0 = 0, .duration1 = 2, .level1 = 0}; // Ask.
    }
    clk_symbols[clk_symbols_counter] = (rmt_symbol_word_t){.duration0 = 2, .level0 = 0, .duration1 = 2, .level1 = 0}; // Stop.
    dio_symbols[dio_symbols_counter] = (rmt_symbol_word_t){.duration0 = 2, .level0 = 1, .duration1 = 2, .level1 = 0}; // Stop.
    rmt_transmit_config_t tx_transmit_config = {.loop_count = 0, .flags.eot_level = 1};
    rmt_sync_reset(handle->synchro);
    ZH_ERROR_CHECK(rmt_transmit(handle->tx_channel[0], handle->copy_encoder, &clk_symbols, sizeof(clk_symbols), &tx_transmit_config) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    ZH_ERROR_CHECK(rmt_transmit(handle->tx_channel[1], handle->copy_encoder, &dio_symbols, sizeof(dio_symbols), &tx_transmit_config) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    ZH_ERROR_CHECK(rmt_tx_wait_all_done(handle->tx_channel[0], 1000 / portTICK_PERIOD_MS) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    ZH_ERROR_CHECK(rmt_tx_wait_all_done(handle->tx_channel[1], 1000 / portTICK_PERIOD_MS) == ESP_OK, ESP_FAIL, ++_stats.rmt_driver_error, "RMT driver error.");
    return ESP_OK;
}