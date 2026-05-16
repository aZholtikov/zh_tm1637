# ESP32 ESP-IDF component for TM1637 led drive (via RMT driver)

## Tested on

1. [ESP32 ESP-IDF v6.0.1](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/index.html)

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Features

1. Support up to 4 led drives on one device (depends on the used ESP32 chip).
2. Print raw symbols, int and float values.

## Attention

1. Original ESP32, ESP32-S2 and ESP32-C2 not supported.
2. For correct operation, please enable the following settings in the menuconfig:

```text
CONFIG_RMT_ENCODER_FUNC_IN_IRAM=y
CONFIG_RMT_TX_ISR_HANDLER_IN_IRAM=y
CONFIG_RMT_TX_ISR_CACHE_SAFE=y
```

## Using

In an existing project, run the following command to install the components:

```text
cd ../your_project/components
git clone https://github.com/aZholtikov/zh_tm1637
```

In the application, add the component:

```c
#include "zh_tm1637.h"
```

## Examples

One 6 segment led drive on device:

```c
// Numerical symbols without dot (with dot).
// 0x3F (0xBF) - 0 (0.).
// 0x06 (0x86) - 1 (1.).
// 0x5B (0xDB) - 2 (2.).
// 0x4F (0xCF) - 3 (3.).
// 0x66 (0xE6) - 4 (4.).
// 0x6D (0xED) - 5 (5.).
// 0x7D (0xFD) - 6 (6.).
// 0x07 (0x87) - 7 (7.).
// 0x7F (0xFF) - 8 (8.).
// 0x6F (0xEF) - 9 (9.).
// 0x40        - Minus.
// 0x00        - Empty.
// 0x80        - Dot.

#include "zh_tm1637.h"

zh_tm1637_handle_t tm1637_handle = {0};

void app_main(void)
{
    esp_log_level_set("zh_tm1637", ESP_LOG_ERROR);
    zh_tm1637_init_config_t config = ZH_TM1637_INIT_CONFIG_DEFAULT();
    config.clk_gpio = GPIO_NUM_25;
    config.dio_gpio = GPIO_NUM_26;
    zh_tm1637_init(&config, &tm1637_handle);
    zh_tm1637_set_brightness(&tm1637_handle, 7); // Set brightness.
    zh_tm1637_clear(&tm1637_handle);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    zh_tm1637_print(&tm1637_handle, ZH_TM1637_C0H, 0x3F); // 0 on C0H address.
    zh_tm1637_print(&tm1637_handle, ZH_TM1637_C1H, 0x06); // 1 on C1H address.
    zh_tm1637_print(&tm1637_handle, ZH_TM1637_C2H, 0x5B); // 2 on C2H address.
    zh_tm1637_print(&tm1637_handle, ZH_TM1637_C3H, 0x4F); // 3 on C3H address.
    zh_tm1637_print(&tm1637_handle, ZH_TM1637_C4H, 0x66); // 4 on C4H address.
    zh_tm1637_print(&tm1637_handle, ZH_TM1637_C5H, 0x6D); // 5 on C5H address.
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // If the display of numbers on the screen does not correspond to the order, it is necessary to establish physical correspondence.
    // For example you sees 210543. The physical correspondence should be established as follows:
    zh_tm1637_set(&tm1637_handle, ZH_TM1637_C2H, ZH_TM1637_C1H, ZH_TM1637_C0H, ZH_TM1637_C5H, ZH_TM1637_C4H, ZH_TM1637_C3H);
    zh_tm1637_clear(&tm1637_handle);
    zh_tm1637_print_int(&tm1637_handle, 0, 123456);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    zh_tm1637_clear(&tm1637_handle);
    zh_tm1637_print_float(&tm1637_handle, 0, -156.255, 2);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
```
