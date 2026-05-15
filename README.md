# ESP32 ESP-IDF component for TM1637 led drive (via RMT driver)

## Tested on

1. [ESP32 ESP-IDF v6.0.1](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/index.html)

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Features

1. Support up to 4 led drives on one device (depends on the used ESP32 chip).

## Note

1. Just simple driver without any additives.

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

One 4 segment led drive on device:

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
    for (uint8_t i = 0; i < 6; ++i)              // Clear LCD.
    {
        zh_tm1637_print(&tm1637_handle, i, 0x00);
    };
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    zh_tm1637_print(&tm1637_handle, 0, 0x4F); // 3 on C0H address.
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    zh_tm1637_print(&tm1637_handle, 4, 0x87); // 7. on C4H address.
}
```
