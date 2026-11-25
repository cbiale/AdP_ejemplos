/*
 * Conexión Wi-Fi explícita (modo estación), en lugar de example_connect().
 *
 * El driver Wi-Fi de ESP-IDF trabaja con eventos: las funciones esp_wifi_*
 * piden cosas y las respuestas llegan después como eventos al bucle de eventos.
 * La secuencia es:
 *   esp_wifi_start()  -> WIFI_EVENT_STA_START        -> esp_wifi_connect()
 *   asociación al AP  -> WIFI_EVENT_STA_CONNECTED
 *   DHCP              -> IP_EVENT_STA_GOT_IP          -> ya se puede usar la red
 *   si se corta       -> WIFI_EVENT_STA_DISCONNECTED  -> esp_wifi_connect() otra vez
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "wifi.h"

static const char *TAG = "wifi";

// Grupo de eventos de FreeRTOS: el manejador prende el bit y wifi_conectar() lo espera
static EventGroupHandle_t eventos;
#define CON_IP BIT0

static void al_evento(void *arg, esp_event_base_t base, int32_t id, void *datos)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        // El driver arrancó en modo estación: pide asociarse al AP (el resultado llega como evento)
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "asociado al AP; esperando IP por DHCP");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        // No se pudo asociar o se cortó. Motivos comunes: 201 = no encontró la red,
        // 15 o 204 = clave incorrecta. Se reintenta siempre.
        wifi_event_sta_disconnected_t *d = datos;
        ESP_LOGW(TAG, "desconectado (motivo %d); reintentando", d->reason);
        xEventGroupClearBits(eventos, CON_IP);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        // El servidor DHCP del AP nos asignó dirección: recién ahora funciona TCP/UDP
        ip_event_got_ip_t *e = datos;
        ESP_LOGI(TAG, "IP " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(eventos, CON_IP);
    }
}

void wifi_conectar(void)
{
    eventos = xEventGroupCreate();

    // Crea la interfaz de red de estación (netif de lwIP con cliente DHCP) asociada al Wi-Fi
    esp_netif_create_default_wifi_sta();

    // Inicializa el driver Wi-Fi con la configuración por defecto (buffers, tarea del driver)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registra al_evento para todos los eventos del Wi-Fi y para "obtuve IP"
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, al_evento, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, al_evento, NULL, NULL));

    // SSID y clave de menuconfig. Con clave vacía se conecta a una red abierta.
    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, CONFIG_WIFI_SSID, sizeof wc.sta.ssid);
    strlcpy((char *)wc.sta.password, CONFIG_WIFI_PASSWORD, sizeof wc.sta.password);

    // Modo estación: el ESP32 es un cliente de un AP (no crea su propia red)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Carga SSID y clave en la interfaz de estación
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    // Arranca el driver: emite WIFI_EVENT_STA_START y desde ahí sigue la secuencia de eventos
    ESP_ERROR_CHECK(esp_wifi_start());
    // Sin ahorro de energía: la radio queda siempre encendida y los datagramas
    // entrantes no esperan al próximo beacon del AP (más consumo, menos latencia)
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "conectando a \"%s\"...", CONFIG_WIFI_SSID);
    // Bloquea hasta que al_evento avise que hay IP
    xEventGroupWaitBits(eventos, CON_IP, pdFALSE, pdTRUE, portMAX_DELAY);
}
