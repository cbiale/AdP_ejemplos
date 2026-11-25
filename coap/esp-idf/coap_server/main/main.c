/*
 * Servidor CoAP con ESP-IDF (componente espressif/coap = libcoap 4.3):
 *  - GET  /sensores/temp   observable (RFC 7641), JSON
 *  - GET  /actuadores/led  y  PUT /actuadores/led  (on|off)
 *  - GET  /.well-known/core lo responde libcoap con los atributos registrados
 *
 * Probar desde la PC con el cliente TypeScript (COAP_HOST=<ip del ESP32>)
 * o con libcoap:  coap-client -m get -s 20 coap://<ip>/sensores/temp
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "coap3/coap.h"
#include "wifi.h"

static const char *TAG = "coap_server";

static float temp = 22.0f;
static int led = 0;

/* GET /sensores/temp. libcoap llama a este manejador con la solicitud (req) y una
 * respuesta (resp) ya creada; si el cliente pidió Observe, libcoap agrega la opción
 * y vuelve a llamarlo en cada notificación. */
static void hnd_temp_get(coap_resource_t *res, coap_session_t *ses,
                         const coap_pdu_t *req, const coap_string_t *query,
                         coap_pdu_t *resp)
{
    char buf[32];
    int len = snprintf(buf, sizeof buf, "{\"temp\":%.1f}", temp);
    // Código de la respuesta: 2.05 Content
    coap_pdu_set_code(resp, COAP_RESPONSE_CODE_CONTENT);
    // Agrega Content-Format, Max-Age y el payload; si no entra en un datagrama,
    // libcoap lo parte en bloques (Block2) por su cuenta
    coap_add_data_large_response(res, ses, req, resp, query,
                                 COAP_MEDIATYPE_APPLICATION_JSON,
                                 10,  /* Max-Age: 10 s */
                                 0,   /* ETag */
                                 len, (const uint8_t *)buf, NULL, NULL);
}

/* GET /actuadores/led: devuelve "on" u "off" como texto plano */
static void hnd_led_get(coap_resource_t *res, coap_session_t *ses,
                        const coap_pdu_t *req, const coap_string_t *query,
                        coap_pdu_t *resp)
{
    const char *v = led ? "on" : "off";
    // 2.05 Content con Content-Format text/plain, sin Max-Age ni ETag
    coap_pdu_set_code(resp, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(res, ses, req, resp, query,
                                 COAP_MEDIATYPE_TEXT_PLAIN, 0, 0,
                                 strlen(v), (const uint8_t *)v, NULL, NULL);
}

/* PUT /actuadores/led: el payload "on" u "off" fija el estado (idempotente) */
static void hnd_led_put(coap_resource_t *res, coap_session_t *ses,
                        const coap_pdu_t *req, const coap_string_t *query,
                        coap_pdu_t *resp)
{
    size_t size;
    const uint8_t *data;
    // Obtiene el payload de la solicitud (sin copiarlo); 0 si no trae
    if (!coap_get_data(req, &size, &data)) size = 0;

    if (size == 2 && memcmp(data, "on", 2) == 0) led = 1;
    else if (size == 3 && memcmp(data, "off", 3) == 0) led = 0;
    else {
        coap_pdu_set_code(resp, COAP_RESPONSE_CODE_BAD_REQUEST); // 4.00
        return;
    }
    gpio_set_level(CONFIG_LED_GPIO, led);
    ESP_LOGI(TAG, "LED -> %s", led ? "on" : "off");
    coap_pdu_set_code(resp, COAP_RESPONSE_CODE_CHANGED); // 2.04, sin payload
}

static void tarea_coap(void *arg)
{
    // Inicializa libcoap (una vez por programa)
    coap_startup();
    // Solo advertencias y errores de libcoap en el log (COAP_LOG_DEBUG muestra cada PDU)
    coap_set_log_level(COAP_LOG_WARN);

    // Contexto: guarda endpoints, recursos, sesiones y mensajes pendientes
    coap_context_t *ctx = coap_new_context(NULL);
    // Block-wise (RFC 7959) y reensamblado a cargo de libcoap
    coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

    // Dirección local: cualquier interfaz (INADDR_ANY), puerto 5683
    coap_address_t addr;
    coap_address_init(&addr);
    addr.addr.sin.sin_family = AF_INET;
    addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
    addr.addr.sin.sin_port = htons(COAP_DEFAULT_PORT); // 5683
    // Endpoint: abre el socket UDP en esa dirección y empieza a escuchar
    if (!coap_new_endpoint(ctx, &addr, COAP_PROTO_UDP)) {
        ESP_LOGE(TAG, "no se pudo crear el endpoint UDP");
        vTaskDelete(NULL);
    }

    // /sensores/temp (observable)
    // Crea el recurso con su ruta (sin la barra inicial)
    coap_resource_t *r_temp = coap_resource_init(coap_make_str_const("sensores/temp"), 0);
    // Asocia el manejador al método GET
    coap_register_handler(r_temp, COAP_REQUEST_GET, hnd_temp_get);
    // Acepta registros Observe en GET y lo anuncia como "obs" en /.well-known/core
    coap_resource_set_get_observable(r_temp, 1);
    // Atributos de CoRE Link Format (RFC 6690) que aparecen en /.well-known/core
    coap_add_attr(r_temp, coap_make_str_const("rt"), coap_make_str_const("\"temperature\""), 0);
    coap_add_attr(r_temp, coap_make_str_const("if"), coap_make_str_const("\"sensor\""), 0);
    coap_add_attr(r_temp, coap_make_str_const("ct"), coap_make_str_const("50"), 0);
    // Publica el recurso en el contexto: desde ahora responde
    coap_add_resource(ctx, r_temp);

    // /actuadores/led: un manejador por método
    coap_resource_t *r_led = coap_resource_init(coap_make_str_const("actuadores/led"), 0);
    coap_register_handler(r_led, COAP_REQUEST_GET, hnd_led_get);
    coap_register_handler(r_led, COAP_REQUEST_PUT, hnd_led_put);
    coap_add_attr(r_led, coap_make_str_const("rt"), coap_make_str_const("\"light\""), 0);
    coap_add_attr(r_led, coap_make_str_const("if"), coap_make_str_const("\"actuator\""), 0);
    coap_add_resource(ctx, r_led);

    ESP_LOGI(TAG, "CoAP escuchando en udp/5683");

    int64_t ultimo = esp_timer_get_time();
    while (1) {
        // Espera hasta 500 ms por datagramas y atiende pedidos, ACK,
        // retransmisiones de CON y notificaciones pendientes
        coap_io_process(ctx, 500);

        // Cada 2 s actualiza la temperatura; si cambió, notifica a los observadores
        if (esp_timer_get_time() - ultimo > 2000000) {
            ultimo = esp_timer_get_time();
            float nueva = temp + ((int)(esp_random() % 11) - 5) / 10.0f;
            if (nueva != temp) {
                temp = nueva;
                // Marca el recurso como cambiado: en el próximo coap_io_process libcoap
                // vuelve a llamar a hnd_temp_get y envía una notificación a cada observador
                coap_resource_notify_observers(r_temp, NULL);
            }
        }
    }
}

void app_main(void)
{
    // NVS: el driver Wi-Fi guarda ahí su calibración y configuración
    ESP_ERROR_CHECK(nvs_flash_init());
    // Inicializa la pila TCP/IP (lwIP) y la capa esp_netif
    ESP_ERROR_CHECK(esp_netif_init());
    // Bucle de eventos por defecto: por ahí llegan los eventos de Wi-Fi e IP
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Conecta al Wi-Fi de menuconfig y espera la IP (ver wifi.c)
    wifi_conectar();

    // LED como salida
    gpio_reset_pin(CONFIG_LED_GPIO);
    gpio_set_direction(CONFIG_LED_GPIO, GPIO_MODE_OUTPUT);

    // CoAP en su propia tarea: coap_io_process bloquea mientras espera datagramas
    xTaskCreate(tarea_coap, "coap", 8 * 1024, NULL, 5, NULL);
}
