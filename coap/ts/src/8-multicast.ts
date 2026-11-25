// 8. Comunicación en grupo (RFC 7252 §8, RFC 7390): una solicitud NON a la dirección
//    multicast "All CoAP Nodes" (224.0.1.187; en IPv6 ff0X::fd) llega a todos los nodos
//    del grupo, y cada uno responde por unicast, después de un retardo aleatorio.
//    Antes levantar dos o más servidores: npm run servidor-multicast -- luminaria1  (y luminaria2)
//    npm run multicast -- on   (o off)
import coap, { type IncomingMessage } from "coap";

const GRUPO = "224.0.1.187";
const ESPERA_MS = 2000; // cuánto tiempo juntamos respuestas

// Una solicitud multicast puede tener muchas respuestas: las juntamos durante ESPERA_MS.
function aTodos(method: "GET" | "PUT", payload?: string): Promise<IncomingMessage[]> {
  return new Promise((resolve) => {
    const respuestas: IncomingMessage[] = [];
    const req = coap.request({ hostname: GRUPO, pathname: "/luz", method, multicast: true, multicastTimeout: ESPERA_MS });
    req.on("response", (r: IncomingMessage) => respuestas.push(r));
    req.end(payload);
    setTimeout(() => resolve(respuestas), ESPERA_MS);
  });
}

const valor = process.argv[2] ?? "on";
for (const [metodo, cuerpo] of [["PUT", valor], ["GET", undefined]] as const) {
  console.log(`NON ${metodo} coap://${GRUPO}/luz ${cuerpo ?? ""}`);
  const rs = await aTodos(metodo, cuerpo);
  for (const r of rs) console.log(`  ${r.rsinfo.address}:${r.rsinfo.port} -> ${r.code} ${r.payload}`);
  console.log(`  ${rs.length} respuesta(s)`);
}
process.exit(0);
