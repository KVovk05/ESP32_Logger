#include "ctrl_adc.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADC_UNIT       ADC_UNIT_1
#define ADC_CHANNEL    ADC_CHANNEL_3      // GPIO4 -- та сама ручка, що й у Модулі 4.6
#define ADC_ATTEN      ADC_ATTEN_DB_12
#define ADC_BIT_WIDTH  ADC_BITWIDTH_12

#define FRAME_SIZE     256
#define POOL_FRAMES    4

#define THRESH_MIN_C   15.0f
#define THRESH_MAX_C   35.0f

static const char *TAG = "CTRL_ADC";
static adc_continuous_handle_t adc_handle;
static TaskHandle_t adc_task_handle;
static volatile float s_threshold_c = 25.0f;   // безпечне значення за замовчуванням до першого блоку

static bool IRAM_ATTR adc_conv_done_cb(adc_continuous_handle_t handle,
                                        const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t must_yield = pdFALSE;
    vTaskNotifyGiveFromISR(adc_task_handle, &must_yield);
    return must_yield == pdTRUE;
}

static void adc_read_task(void *arg)
{
    uint8_t result[FRAME_SIZE];
    uint32_t bytes_read;

    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // спати, поки DMA не наповнить черговий блок

        esp_err_t err = adc_continuous_read(adc_handle, result, FRAME_SIZE, &bytes_read, 0);
        if (err != ESP_OK) continue;

        adc_digi_output_data_t *data = (adc_digi_output_data_t *)result;
        int count = bytes_read / sizeof(adc_digi_output_data_t);
        uint32_t sum = 0;
        for (int i = 0; i < count; i++) sum += data[i].type2.data;
        uint32_t avg_raw = sum / count;                     // усереднення блоку -- згладжує шум ручки

        float frac = avg_raw / 4095.0f;                     // 0.0 .. 1.0
        s_threshold_c = THRESH_MIN_C + frac * (THRESH_MAX_C - THRESH_MIN_C);
    }
}

void ctrl_adc_init(void)
{
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = FRAME_SIZE * POOL_FRAMES,
        .conv_frame_size    = FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    adc_digi_pattern_config_t pattern[1] = {
        { .atten = ADC_ATTEN, .channel = ADC_CHANNEL, .unit = ADC_UNIT, .bit_width = ADC_BIT_WIDTH },
    };
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20000,
        .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
        .format         = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
        .pattern_num    = 1,
        .adc_pattern    = pattern,
    };
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    adc_continuous_evt_cbs_t cbs = { .on_conv_done = adc_conv_done_cb };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));

    xTaskCreate(adc_read_task, "adc_read_task", 4096, NULL, 5, &adc_task_handle);
    ESP_LOGI(TAG, "ADC1 continuous+DMA ready: GPIO4, lim %.0f-%.0f C", THRESH_MIN_C, THRESH_MAX_C);
}

float ctrl_adc_get_threshold_c(void)
{
    return s_threshold_c;   // головний цикл (Крок 9) лише ЧИТАЄ -- жодного опитування АЦП з його боку
}