idf_component_register(
    SRCS "main.c" "i2c_bus.c" "sht3x.c" "humiture_sht3x.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_event esp_netif esp_wifi mqtt nvs_flash esp_timer
)
