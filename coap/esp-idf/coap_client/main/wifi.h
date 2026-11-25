#pragma once

/*
 * Conecta el ESP32 como estación (cliente) a la red Wi-Fi configurada en
 * menuconfig > "Wi-Fi" y bloquea hasta obtener una dirección IP por DHCP.
 * Si la conexión se corta, vuelve a conectarse sola.
 *
 * Antes hay que llamar a nvs_flash_init(), esp_netif_init() y
 * esp_event_loop_create_default().
 */
void wifi_conectar(void);
