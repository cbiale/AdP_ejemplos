// Utilidad compartida por los clientes CoAP: una solicitud como promesa.
import coap, { type CoapRequestParams, type IncomingMessage, type ObserveReadStream } from "coap";

// Servidor al que van las solicitudes. Por defecto el servidor TypeScript local;
// para el ESP32: COAP_HOST=<ip del ESP32> npm run <ejemplo>
export const HOST = process.env.COAP_HOST ?? "localhost";

/**
 * Envía una solicitud CoAP a HOST y devuelve una promesa con la respuesta.
 *
 * node-coap trabaja con eventos ("response", "error"); pedir los envuelve en una
 * promesa para que cada ejemplo se lea de arriba hacia abajo con await:
 *   const res = await pedir({ pathname: "/sensores/temp" });
 *   console.log(res.code, res.payload.toString());
 *
 * - opts: los parámetros de coap.request. Los más usados en los ejemplos:
 *     pathname     el recurso, p. ej. "/actuadores/led"
 *     method       "GET" (por defecto), "PUT", "POST", "DELETE", "FETCH", "iPATCH"
 *     confirmable  true (por defecto) envía CON; false envía NON
 *     observe      true agrega Observe = 0 (registro de observación)
 *     options      opciones CoAP, p. ej. { "Content-Format": "application/json" }
 *   Si opts trae hostname, reemplaza a HOST.
 * - payload: el cuerpo de la solicitud (PUT, POST, FETCH, iPATCH). Sin él, va vacío.
 *
 * Resuelve con la primera respuesta que llega:
 * - un IncomingMessage con code (p. ej. "2.05"), headers (las opciones) y payload (Buffer);
 * - con observe: true, un ObserveReadStream que sigue emitiendo "data" con cada
 *   notificación, hasta que se llama a close().
 * Un código de error CoAP (4.04, 4.05, 5.xx) también resuelve la promesa, porque es
 * una respuesta válida: hay que mirar res.code. Solo rechaza si node-coap informa un
 * error (por ejemplo, un problema de red).
 *
 * Al tomar solo la primera respuesta, no sirve para multicast, donde responden
 * varios nodos: por eso 8-multicast.ts usa coap.request directamente.
 */
export function pedir(opts: CoapRequestParams, payload?: string): Promise<IncomingMessage | ObserveReadStream> {
  return new Promise((resolve, reject) => {
    const req = coap.request({ hostname: HOST, ...opts });
    req.on("response", resolve);
    req.on("error", reject);
    req.end(payload);
  });
}
