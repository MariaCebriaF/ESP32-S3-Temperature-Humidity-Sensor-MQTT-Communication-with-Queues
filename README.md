# Humiture Monitor

Proyecto de monitorización de temperatura y humedad con un sensor SHT3x sobre ESP32-S3 usando ESP-IDF, FreeRTOS y MQTT.

## Objetivo

La última parte de la práctica consiste en integrar:

- tareas de FreeRTOS
- comunicación entre tareas mediante colas
- conexión Wi-Fi en modo estación
- publicación de medidas mediante MQTT

## Arquitectura

El comportamiento principal está implementado en [main/main.c](main/main.c).

### Flujo general

1. `app_main()` inicializa NVS, red, Wi-Fi, el cliente MQTT y el sensor.
2. Se crea una cola FreeRTOS para intercambiar muestras entre tareas.
3. La tarea `humiture_read_task` lee periódicamente el sensor SHT3x.
4. Cada muestra se envía a la cola.
5. La tarea `mqtt_publish_task` recibe las muestras desde la cola y las publica en un tópico MQTT en formato JSON.

### Tareas

- `humiture_read_task`
  - Lee temperatura y humedad cada 3 segundos.
  - Empaqueta los datos en una estructura `humiture_sample_t`.
  - Inserta la muestra en la cola.

- `mqtt_publish_task`
  - Espera datos en la cola.
  - Comprueba que haya conectividad Wi-Fi.
  - Publica la muestra en el broker MQTT.

### Comunicación entre tareas

La comunicación se hace con una cola FreeRTOS:

- tipo de dato transmitido: `humiture_sample_t`
- creación: `xQueueCreate(...)`
- envío: `xQueueSend(...)`
- recepción: `xQueueReceive(...)`

Esto desacopla la adquisición del sensor de la publicación por red.

## Configuración actual

En [main/main.c](main/main.c) hay varias macros de configuración rápida:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `MQTT_BROKER_URI`
- `MQTT_TOPIC`

Antes de ejecutar el proyecto hay que sustituir al menos:

- `REPLACE_WITH_WIFI_SSID`
- `REPLACE_WITH_WIFI_PASSWORD`

## Dependencias

En [main/CMakeLists.txt](main/CMakeLists.txt) se han declarado estos componentes de ESP-IDF:

- `driver`
- `esp_event`
- `esp_netif`
- `esp_wifi`
- `mqtt`
- `nvs_flash`
- `esp_timer`

## Estructura principal del proyecto

- [main/main.c](main/main.c): inicialización del sistema, Wi-Fi, MQTT, cola y tareas.
- [main/humiture_sht3x.c](main/humiture_sht3x.c): abstracción del sensor SHT3x.
- [main/humiture.h](main/humiture.h): interfaz genérica del sensor.
- [main/sht3x.c](main/sht3x.c): driver del sensor.
- [main/i2c_bus.c](main/i2c_bus.c): soporte de bus I2C.

## Formato del mensaje MQTT

Las muestras se publican como JSON, por ejemplo:

```json
{"sample":1,"temperature":23.40,"humidity":45.10,"tick":12345}
```

## Compilación y carga

Con el entorno de ESP-IDF correctamente cargado:

```bash
idf.py build
idf.py flash monitor
```

Si partes de un entorno limpio, el proyecto está preparado para `esp32s3` mediante `sdkconfig.defaults`.

## Fuentes utilizadas

La parte de Wi-Fi, eventos, colas y MQTT está basada en la documentación oficial de ESP-IDF y en sus patrones habituales de ejemplo.

### Documentación oficial de Espressif

- Wi-Fi Driver:
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html
- ESP-NETIF:
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_netif.html
- ESP-MQTT:
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html
- FreeRTOS en ESP-IDF:
  - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_idf.html

### Ejemplos oficiales de referencia

- Wi-Fi station example:
  - `examples/wifi/getting_started/station`
- MQTT TCP example:
  - `examples/protocols/mqtt/tcp`

## Observaciones

- El broker configurado por defecto en el código es público: `mqtt://broker.hivemq.com`
