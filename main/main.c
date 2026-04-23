#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "humiture.h"

#define WIFI_SSID "REPLACE_WITH_WIFI_SSID"
#define WIFI_PASSWORD "REPLACE_WITH_WIFI_PASSWORD"
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"
#define MQTT_TOPIC "upv/humiture"

#define HUMITURE_READ_TASK_INTERVAL_MS 3000
#define HUMITURE_READ_TASK_PRIORITY 10
#define MQTT_PUBLISH_TASK_PRIORITY 9
#define HUMITURE_QUEUE_LENGTH 8

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "HUMITURE_APP";

typedef struct
{
    uint32_t sample_id;
    float temperature;
    float humidity;
    TickType_t tick;
} humiture_sample_t;

static EventGroupHandle_t wifi_event_group;
static QueueHandle_t humiture_queue;
static esp_mqtt_client_handle_t mqtt_client;
static bool mqtt_connected;

static void humiture_read_task(void *pvParameters);
static void mqtt_publish_task(void *pvParameters);
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data);
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data);
static void wifi_init_sta(void);
static void mqtt_start(void);

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_event_group = xEventGroupCreate();
    humiture_queue = xQueueCreate(HUMITURE_QUEUE_LENGTH, sizeof(humiture_sample_t));

    if ((wifi_event_group == NULL) || (humiture_queue == NULL))
    {
        ESP_LOGE(TAG, "No se pudieron crear los recursos RTOS");
        return;
    }

    humiture_init();
    wifi_init_sta();
    mqtt_start();

    xTaskCreate(humiture_read_task,
                "humiture_read_task",
                4096,
                NULL,
                HUMITURE_READ_TASK_PRIORITY,
                NULL);

    xTaskCreate(mqtt_publish_task,
                "mqtt_publish_task",
                4096,
                NULL,
                MQTT_PUBLISH_TASK_PRIORITY,
                NULL);
}

static void wifi_init_sta(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1U);
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1U);

    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta());
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START))
    {
        ESP_LOGI(TAG, "Conectando a Wi-Fi...");
        ESP_ERROR_CHECK(esp_wifi_connect());
        return;
    }

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED))
    {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Wi-Fi desconectado, reintentando");
        ESP_ERROR_CHECK(esp_wifi_connect());
        return;
    }

    if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP))
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;
    (void)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT conectado");
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT desconectado");
        break;

    default:
        break;
    }
}

static void humiture_read_task(void *pvParameters)
{
    (void)pvParameters;
    humiture_sample_t sample = {0};

    for (;;)
    {
        if (humiture_read(&sample.temperature, &sample.humidity) != 0)
        {
            sample.temperature = 0.0f;
            sample.humidity = 0.0f;
        }

        sample.sample_id++;
        sample.tick = xTaskGetTickCount();

        if (xQueueSend(humiture_queue, &sample, 0) != pdPASS)
        {
            ESP_LOGW(TAG, "Cola llena, muestra descartada");
        }
        else
        {
            ESP_LOGI(TAG,
                     "Muestra %" PRIu32 " encolada: T=%.2f C RH=%.2f%%",
                     sample.sample_id,
                     sample.temperature,
                     sample.humidity);
        }

        vTaskDelay(pdMS_TO_TICKS(HUMITURE_READ_TASK_INTERVAL_MS));
    }
}

static void mqtt_publish_task(void *pvParameters)
{
    (void)pvParameters;
    humiture_sample_t sample;
    char payload[128];

    for (;;)
    {
        if (xQueueReceive(humiture_queue, &sample, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        xEventGroupWaitBits(wifi_event_group,
                            WIFI_CONNECTED_BIT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        if (!mqtt_connected)
        {
            ESP_LOGW(TAG, "MQTT no conectado, muestra %" PRIu32 " pendiente", sample.sample_id);
            vTaskDelay(pdMS_TO_TICKS(1000));
            xQueueSendToFront(humiture_queue, &sample, 0);
            continue;
        }

        snprintf(payload,
                 sizeof(payload),
                 "{\"sample\":%" PRIu32 ",\"temperature\":%.2f,\"humidity\":%.2f,\"tick\":%" PRIu32 "}",
                 sample.sample_id,
                 sample.temperature,
                 sample.humidity,
                 (uint32_t)sample.tick);

        if (esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, payload, 0, 1, 0) < 0)
        {
            ESP_LOGE(TAG, "Fallo publicando la muestra %" PRIu32, sample.sample_id);
            xQueueSendToFront(humiture_queue, &sample, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Publicado en %s: %s", MQTT_TOPIC, payload);
    }
}
