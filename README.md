# Ejemplos de Arquitectura de Protocolos

Ejemplos de clase de Arquitectura de Protocolos (Universidad de Buenos Aires), en TypeScript (Node.js 20+) y ESP-IDF (5.1+). Hay una carpeta por protocolo y cada proyecto es independiente: tiene sus propias dependencias y se instala y corre por separado.

```
coap/
  ts/                   servidor y clientes paso a paso (node-coap)
  esp-idf/coap_server/  servidor CoAP (libcoap) con recurso observable y actuador
  esp-idf/coap_client/  cliente CoAP (libcoap): descubre, observa la temperatura y prende un LED remoto
```

Por ahora está publicado CoAP; MQTT, AMQP y Bluetooth LE se van a sumar en sus propias carpetas.

Cada carpeta tiene su README con los ejemplos y sugerencias para la clase.

## Proyectos TypeScript

En la carpeta `ts/` de cada protocolo:

```bash
npm install
npm run check            # chequeo de tipos
npm run <ejemplo>        # ver el README del protocolo
```

## Proyectos ESP-IDF

Requiere ESP-IDF 5.1 o superior. En cada proyecto:

```bash
idf.py set-target esp32      # o esp32s3, esp32c3...
idf.py menuconfig            # Wi-Fi en "Example Connection Configuration"
idf.py build flash monitor
```

La temperatura es simulada; reemplazarla por la lectura de un sensor real.
