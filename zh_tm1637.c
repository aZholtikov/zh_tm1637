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
    for (uint8_t i = 0; i < 6; ++i)
    {
        handle->digit_positions[i] = i;
    }
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

esp_err_t zh_tm1637_print(zh_tm1637_handle_t *handle, zh_tm1637_address_t address, uint8_t symbol) // -V2008
{
    ZH_LOGI("Led drive printing started.");
    ZH_ERROR_CHECK(handle != NULL && address <= 5, ESP_ERR_INVALID_ARG, NULL, "Led drive printing failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive printing failed. Led drive not initialized.");
    uint8_t set_data_command = 0x44;
    ZH_ERROR_CHECK(_zh_tm1637_send_data(handle, &set_data_command, 1) == ESP_OK, ESP_FAIL, NULL, "Led drive printing failed. RMT driver error.");
    uint8_t print_symbols_command[2];
    print_symbols_command[0] = address | 0xC0;
    memcpy(&print_symbols_command[1], &symbol, 1);
    ZH_ERROR_CHECK(_zh_tm1637_send_data(handle, print_symbols_command, 2) == ESP_OK, ESP_FAIL, NULL, "Led drive printing failed. RMT driver error.");
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

esp_err_t zh_tm1637_clear(zh_tm1637_handle_t *handle)
{
    ZH_LOGI("Led drive clear started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Led drive clear failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive clear failed. Led drive not initialized.");
    for (uint8_t i = 0; i < 6; ++i)
    {
        ZH_ERROR_CHECK(zh_tm1637_print(handle, (zh_tm1637_address_t)i, 0x00) == ESP_OK, ESP_FAIL, NULL, "Led drive clear failed. RMT driver error.");
    }
    ZH_LOGI("Led drive clear completed successfully.");
    return ESP_OK;
}

esp_err_t zh_tm1637_set(zh_tm1637_handle_t *handle, zh_tm1637_address_t digit_0, zh_tm1637_address_t digit_1, zh_tm1637_address_t digit_2, zh_tm1637_address_t digit_3, zh_tm1637_address_t digit_4, zh_tm1637_address_t digit_5) // -V2008
{
    ZH_LOGI("Led drive set physical correspondence started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Led drive set physical correspondence failed. Invalid argument.");
    ZH_ERROR_CHECK(digit_0 < ZH_TM1637_MAX && digit_1 < ZH_TM1637_MAX && digit_2 < ZH_TM1637_MAX && digit_3 < ZH_TM1637_MAX && digit_4 < ZH_TM1637_MAX && digit_5 < ZH_TM1637_MAX, ESP_ERR_INVALID_ARG, NULL, "Led drive set physical correspondence failed. Invalid address.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive set physical correspondence failed. Led drive not initialized.");
    uint8_t matrix[] = {digit_0, digit_1, digit_2, digit_3, digit_4, digit_5};
    for (uint8_t i = 0; i < sizeof(matrix); ++i)
    {
        for (uint8_t j = i + 1; j < sizeof(matrix); ++j)
        {
            ZH_ERROR_CHECK(matrix[i] != matrix[j], ESP_ERR_INVALID_ARG, NULL, "Led drive set physical correspondence failed. Addresses are repeated.");
        }
    }
    handle->digit_positions[0] = digit_0;
    handle->digit_positions[1] = digit_1;
    handle->digit_positions[2] = digit_2;
    handle->digit_positions[3] = digit_3;
    handle->digit_positions[4] = digit_4;
    handle->digit_positions[5] = digit_5;
    ZH_LOGI("Led drive set physical correspondence completed successfully.");
    return ESP_OK;
}

esp_err_t zh_tm1637_print_int(zh_tm1637_handle_t *handle, uint8_t position, int value) // -V2008
{
    ZH_LOGI("Led drive print int started.");
    ZH_ERROR_CHECK(handle != NULL && position <= 5, ESP_ERR_INVALID_ARG, NULL, "Led drive print int failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive print int failed. Led drive not initialized.");
    char char_buffer[6] = {0};
    sprintf(char_buffer, "%d", value);
    for (uint8_t i = 0; i < sizeof(char_buffer) - position; ++i)
    {
        uint8_t int_symbol = 0x00;
        switch (char_buffer[i])
        {
        case '-':
            int_symbol = 0x40;
            break;
        case '0':
            int_symbol = 0x3F;
            break;
        case '1':
            int_symbol = 0x06;
            break;
        case '2':
            int_symbol = 0x5B;
            break;
        case '3':
            int_symbol = 0x4F;
            break;
        case '4':
            int_symbol = 0x66;
            break;
        case '5':
            int_symbol = 0x6D;
            break;
        case '6':
            int_symbol = 0x7D;
            break;
        case '7':
            int_symbol = 0x07;
            break;
        case '8':
            int_symbol = 0x7F;
            break;
        case '9':
            int_symbol = 0x6F;
            break;
        default:
            break;
        }
        if (int_symbol != 0x00)
        {
            ZH_ERROR_CHECK(zh_tm1637_print(handle, handle->digit_positions[i + position], int_symbol) == ESP_OK, ESP_FAIL, NULL, "Led drive print int failed. RMT driver error.");
        }
    }
    ZH_LOGI("Led drive print int completed successfully.");
    return ESP_OK;
}

esp_err_t zh_tm1637_print_float(zh_tm1637_handle_t *handle, uint8_t position, float value, uint8_t precision) // -V2008
{
    ZH_LOGI("Led drive print float started.");
    ZH_ERROR_CHECK(handle != NULL && position <= 5, ESP_ERR_INVALID_ARG, NULL, "Led drive print float failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Led drive print float failed. Led drive not initialized.");
    char char_buffer[7] = {0};
    uint8_t dot_offset = 0;
    sprintf(char_buffer, "%.*f", precision, value); // -V512
    for (uint8_t i = 0; i < sizeof(char_buffer) - position; ++i)
    {
        uint8_t int_symbol = 0x00;
        switch (char_buffer[i])
        {
        case '.':
            dot_offset = 1;
            break;
        case '-':
            int_symbol = 0x40;
            break;
        case '0':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xBF : 0x3F;
            break;
        case '1':
            int_symbol = (char_buffer[i + 1] == '.') ? 0x86 : 0x06;
            break;
        case '2':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xDB : 0x5B;
            break;
        case '3':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xCF : 0x4F;
            break;
        case '4':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xE6 : 0x66;
            break;
        case '5':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xED : 0x6D;
            break;
        case '6':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xFD : 0x7D;
            break;
        case '7':
            int_symbol = (char_buffer[i + 1] == '.') ? 0x87 : 0x07;
            break;
        case '8':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xFF : 0x7F;
            break;
        case '9':
            int_symbol = (char_buffer[i + 1] == '.') ? 0xEF : 0x6F;
            break;
        default:
            break;
        }
        if (int_symbol != 0x00 && (i + position - dot_offset) != 6)
        {
            ZH_ERROR_CHECK(zh_tm1637_print(handle, handle->digit_positions[i + position - dot_offset], int_symbol) == ESP_OK, ESP_FAIL, NULL, "Led drive print float failed. RMT driver error.");
        }
    }
    ZH_LOGI("Led drive print float completed successfully.");
    return ESP_OK;
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