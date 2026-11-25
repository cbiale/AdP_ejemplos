/*
 * Cliente CoAP con ESP-IDF (componente espressif/coap = libcoap 4.3):
 *  1. GET /.well-known/core  -> imprime los recursos del servidor
 *  2. GET /sensores/temp con Observe = 0 -> recibe notificaciones
 *  3. Si la temperatura supera el umbral, PUT /actuadores/led "on"; si baja, "off"
 *
 * El servidor puede ser el TypeScript (npm run servidor en coap/ts) o otro ESP32
 * con el proyecto coap_server. Se configura en menuconfig > "Ejemplo CoAP cliente".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "coap3/coap.h"
#include "wifi.h"

static const char *TAG = "coap_client";

static coap_uri_t uri_base;           // esquema, host y puerto del servidor
static coap_address_t dst;            // dirección resuelta del servidor
static coap_bin_const_t token_obs;    // token de la observación
static uint8_t token_obs_buf[8];
static int led_remoto = -1;           // -1 desconocido, 0 off, 1 on

/* Arma y envía una solicitud CON. Devuelve el token usado en *tok si no es NULL. */
static void enviar(coap_session_t *ses, coap_pdu_code_t metodo, const char *path,
                   const char *payload, int observe, coap_bin_const_t *tok)
{
    // Crea el PDU: tipo CON, método, Message ID nuevo de la sesión y tamaño máximo
    // que admite la sesión (sobre UDP, lo que entra en un datagrama)
    coap_pdu_t *pdu = coap_pdu_init(COAP_MESSAGE_CON, metodo, coap_new_message_id(ses),
                                    coap_session_max_pdu_size(ses));
    if (!pdu) return;

    // Token nuevo para esta solicitud: asocia la respuesta con la pregunta
    uint8_t t[8]; size_t tl;
    // Genera un token que no se usó en la sesión
    coap_session_new_token(ses, &tl, t);
    // Lo copia en el encabezado del PDU (TKL + token)
    coap_add_token(pdu, tl, t);
    if (tok) { memcpy(token_obs_buf, t, tl); tok->s = token_obs_buf; tok->length = tl; }

    // Opciones: Observe (6) va antes que Uri-Path (11); la lista las ordena sola
    coap_optlist_t *opts = NULL;
    uint8_t buf[64];
    if (observe) {
        // Observe = 0 (registro), codificado como entero de largo variable
        coap_insert_optlist(&opts, coap_new_optlist(COAP_OPTION_OBSERVE,
            coap_encode_var_safe(buf, sizeof buf, COAP_OBSERVE_ESTABLISH), buf));
    }
    coap_uri_t uri = uri_base;
    uri.path.s = (const uint8_t *)path; uri.path.length = strlen(path);
    uint8_t scratch[128];
    // Convierte la URI en opciones: una Uri-Path por segmento (y Uri-Host/Port si hacen falta)
    coap_uri_into_options(&uri, &dst, &opts, 0, scratch, sizeof scratch);
    // Codifica todas las opciones en el PDU, en orden y por diferencia (delta)
    coap_add_optlist_pdu(pdu, &opts);
    // Libera la lista (el PDU ya tiene su copia)
    coap_delete_optlist(opts);

    // Payload después del marcador 0xFF
    if (payload) coap_add_data(pdu, strlen(payload), (const uint8_t *)payload);
    // Envía el PDU; al ser CON, libcoap lo retransmite hasta recibir el ACK.
    // libcoap se queda con el PDU: no hay que liberarlo
    coap_send(ses, pdu);
}

/* Todas las respuestas (y notificaciones) llegan acá */
static coap_response_t al_recibir(coap_session_t *ses, const coap_pdu_t *sent,
                                  const coap_pdu_t *rcv, const coap_mid_t mid)
{
    // Código de respuesta: clase (3 bits) y detalle (5 bits), p. ej. 2.05
    coap_pdu_code_t code = coap_pdu_get_code(rcv);
    size_t len; const uint8_t *data;
    // Payload de la respuesta (ya reensamblado si vino en bloques)
    if (!coap_get_data(rcv, &len, &data)) len = 0;
    ESP_LOGI(TAG, "respuesta %d.%02d: %.*s", COAP_RESPONSE_CLASS(code), code & 0x1F, (int)len, (const char *)data);

    // ¿Es una notificación de la observación? Se reconoce por el token
    coap_bin_const_t tok = coap_pdu_get_token(rcv);
    if (len && tok.length == token_obs.length && memcmp(tok.s, token_obs.s, tok.length) == 0) {
        char txt[64];
        snprintf(txt, sizeof txt, "%.*s", (int)len, (const char *)data);
        const char *p = strstr(txt, "\"temp\":");
        if (p) {
            float temp = strtof(p + 7, NULL);
            int quiero = temp >= CONFIG_TEMP_UMBRAL;
            if (quiero != led_remoto) {      // solo enviamos cuando cambia
                led_remoto = quiero;
                ESP_LOGW(TAG, "temp %.1f -> PUT led %s", temp, quiero ? "on" : "off");
                enviar(ses, COAP_REQUEST_CODE_PUT, "actuadores/led", quiero ? "on" : "off", 0, NULL);
            }
        }
    }
    // COAP_RESPONSE_OK: libcoap envía el ACK si hace falta (si devolviéramos
    // COAP_RESPONSE_FAIL respondería con RST, y en Observe eso da de baja)
    return COAP_RESPONSE_OK;
}

static void tarea_coap(void *arg)
{
    // Inicializa libcoap y deja en el log solo advertencias y errores
    coap_startup();
    coap_set_log_level(COAP_LOG_WARN);

    // Separa la URI configurada (coap://host:puerto) en esquema, host y puerto
    if (coap_split_uri((const uint8_t *)CONFIG_COAP_SERVER_URI, strlen(CONFIG_COAP_SERVER_URI), &uri_base) < 0) {
        ESP_LOGE(TAG, "URI inválida: %s", CONFIG_COAP_SERVER_URI);
        vTaskDelete(NULL);
    }
    // Resuelve el host (IP o nombre) a una dirección de socket para UDP
    coap_addr_info_t *info = coap_resolve_address_info(&uri_base.host, uri_base.port, uri_base.port,
                                                       uri_base.port, uri_base.port, 0,
                                                       1 << uri_base.scheme, COAP_RESOLVE_TYPE_REMOTE);
    if (!info) { ESP_LOGE(TAG, "no se pudo resolver el servidor"); vTaskDelete(NULL); }
    dst = info->addr;
    // Libera la lista de resultados (ya copiamos la dirección)
    coap_free_address_info(info);

    // Contexto: guarda sesiones y mensajes pendientes de ACK
    coap_context_t *ctx = coap_new_context(NULL);
    // Block-wise a cargo de libcoap: al_recibir recibe el cuerpo completo
    coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);
    // Una sola función para todas las respuestas y notificaciones
    coap_register_response_handler(ctx, al_recibir);
    // Sesión cliente: socket UDP local (puerto efímero) hacia el servidor
    coap_session_t *ses = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_UDP);

    enviar(ses, COAP_REQUEST_CODE_GET, ".well-known/core", NULL, 0, NULL);
    enviar(ses, COAP_REQUEST_CODE_GET, "sensores/temp", NULL, 1, &token_obs);

    while (1) {
        // Espera hasta 1 s por datagramas y atiende respuestas, notificaciones
        // y retransmisiones de los CON sin ACK
        coap_io_process(ctx, 1000);
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

    // CoAP en su propia tarea: coap_io_process bloquea mientras espera datagramas
    xTaskCreate(tarea_coap, "coap", 8 * 1024, NULL, 5, NULL);
}
