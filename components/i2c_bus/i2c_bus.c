#include "include/i2c_bus.h"
#include "driver/i2c_types.h"
#include "esp_log.h"

#define I2C_PORT       I2C_NUM_0
#define I2C_SDA_PIN    8
#define I2C_SCL_PIN    9

static const char *TAG = "I2C_BUS";
static i2c_master_bus_handle_t s_bus_handle;

void i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = I2C_PORT,
        .scl_io_num                   = I2C_SCL_PIN,
        .sda_io_num                   = I2C_SDA_PIN,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,   // зовнішні 4.7 кОм, як у Модулі 4.2 -- перевірте Крок 0
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_bus_handle));
    ESP_LOGI(TAG, "Shared I2C bus: SDA=GPIO%d SCL=GPIO%d (3 devices)", I2C_SDA_PIN, I2C_SCL_PIN);
}

i2c_master_bus_handle_t i2c_bus_handle(void)
{
    return s_bus_handle;
}