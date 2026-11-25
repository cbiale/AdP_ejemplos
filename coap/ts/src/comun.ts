// Utilidad compartida por los clientes CoAP: una solicitud como promesa.
import coap, { type CoapRequestParams, type IncomingMessage, type ObserveReadStream } from "coap";

export const HOST = process.env.COAP_HOST ?? "localhost";

export function pedir(opts: CoapRequestParams, payload?: string): Promise<IncomingMessage | ObserveReadStream> {
  return new Promise((resolve, reject) => {
    const req = coap.request({ hostname: HOST, ...opts });
    req.on("response", resolve);
    req.on("error", reject);
    req.end(payload);
  });
}
