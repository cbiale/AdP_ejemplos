# CoAP

No necesita broker.

## TypeScript (`ts/`)

```bash
cd ts && npm install
```

| Script | Qué muestra |
|---|---|
| `npm run servidor` | servidor con todos los recursos: observable, PUT, POST/DELETE, FETCH, iPATCH, Block2, `/.well-known/core` |
| `npm run descubrir` | 1. GET `/.well-known/core` |
| `npm run get` | 2. GET confirmable y no confirmable; 4.04 |
| `npm run put -- on` | 3. PUT sobre el LED (`on` u `off`): idempotente, 2.04 |
| `npm run post` | 4. POST crea recursos en `/mediciones` (2.01 + Location-Path, no idempotente) y DELETE los borra (2.02). Solo servidor TypeScript |
| `npm run fetch` | 5. FETCH e iPATCH (solo servidor TypeScript) |
| `npm run bloques` | 6. Block2 (solo servidor TypeScript) |
| `npm run observe` | 7. Observe durante 10 s |
| `npm run servidor-multicast -- luminaria1` | nodo que se une al grupo All CoAP Nodes (224.0.1.187); levantar dos o más con nombres distintos |
| `npm run multicast -- on` | 8. NON PUT y GET a `coap://224.0.1.187/luz`: una solicitud, una respuesta unicast por nodo |

Variables de entorno: `COAP_HOST` (por defecto `localhost`).

Multicast: la dirección de CoAP es **224.0.1.187** en IPv4 y **ff0X::fd** en IPv6 (`ff02::fd` enlace local, `ff05::fd` sitio), RFC 7252 §12.8. Los servidores multicast y `npm run servidor` usan el mismo puerto (udp/5683): no levantarlos a la vez en la misma PC. Para que respondan otras PC del aula, la red tiene que dejar pasar multicast (muchas redes Wi-Fi lo filtran).

## ESP-IDF

- **coap_server**: el componente `espressif/coap` se descarga en el primer build. Probar con
  `COAP_HOST=<ip del ESP32> npm run observe` (o `descubrir`, `get`, `put`). POST/DELETE, FETCH, iPATCH y bloques son solo del servidor TypeScript.
- **coap_client**: configurar la URI del servidor y el umbral en "Ejemplo CoAP cliente". Descubre los recursos, observa `/sensores/temp` y hace PUT a `/actuadores/led` cuando la temperatura cruza el umbral. Sirve contra el servidor TypeScript o contra otro ESP32 con `coap_server`.
