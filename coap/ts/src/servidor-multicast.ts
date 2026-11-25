// Servidor CoAP que se une al grupo "All CoAP Nodes" (RFC 7252 §12.8):
// 224.0.1.187 en IPv4 (en IPv6: ff0X::fd, p. ej. ff02::fd o ff05::fd).
// Para tener varios nodos en una misma PC, levantar varias instancias con nombres distintos:
//   npm run servidor-multicast -- luminaria1
//   npm run servidor-multicast -- luminaria2
// Todas escuchan en udp/5683 (reuseAddr) y reciben la misma solicitud multicast.
import coap, { type IncomingMessage, type OutgoingMessage } from "coap";

const GRUPO = "224.0.1.187";
const nombre = process.argv[2] ?? `nodo-${process.pid}`;
let luz = "off";

const server = coap.createServer({ multicastAddress: GRUPO }, (req: IncomingMessage, res: OutgoingMessage) => {
  console.log(`${req.method} ${req.url} de ${req.rsinfo.address}:${req.rsinfo.port}`);
  if (req.url !== "/luz") { res.code = "4.04"; return res.end(); }
  if (req.method === "PUT") luz = req.payload.toString();
  else if (req.method !== "GET") { res.code = "4.05"; return res.end(); }

  // RFC 7252 §8.2: ante una solicitud multicast, esperar un tiempo aleatorio antes de
  // responder (Leisure) para que todos los nodos no contesten a la vez.
  const espera = Math.floor(Math.random() * 1000);
  setTimeout(() => {
    if (req.method === "PUT") res.code = "2.04"; // Changed
    res.end(`${nombre}: luz=${luz} (respondió a los ${espera} ms)`);
  }, espera);
});

server.listen(5683, () => console.log(`${nombre} en el grupo ${GRUPO}, udp/5683`));
