// 4. POST y DELETE sobre una colección. Solo contra el servidor TypeScript.
//    Comparar con 3-put-led.ts:
//      PUT  /actuadores/led  -> el cliente conoce la URI y reemplaza el estado; repetirlo deja
//                               el mismo resultado (idempotente, 2.04 Changed).
//      POST /mediciones      -> el servidor crea un recurso y elige su URI (2.01 Created +
//                               Location-Path); repetirlo crea OTRO recurso (no idempotente).
//    Por eso un CON con POST que se retransmite depende de la detección de duplicados por Message ID.
import type { IncomingMessage } from "coap";
import { pedir } from "./comun.js";

const json = { "Content-Format": "application/json" };
// Location-Path es repetible (una opción por segmento). headers solo guarda la última,
// así que leemos todas las opciones del paquete.
const ubicacion = (r: IncomingMessage) =>
  "/" + (r._packet.options ?? []).filter((o) => o.name === "Location-Path").map((o) => String(o.value)).join("/");

// Dos POST con el mismo cuerpo: dos recursos distintos.
const lectura = JSON.stringify({ temp: 22.4, ts: Date.now() });
const creados: string[] = [];
for (let i = 1; i <= 2; i++) {
  const r = (await pedir({ pathname: "/mediciones", method: "POST", options: json }, lectura)) as IncomingMessage;
  creados.push(ubicacion(r));
  console.log(`POST #${i} /mediciones -> ${r.code} Location-Path: ${ubicacion(r)}`);
}

const lista = await pedir({ pathname: "/mediciones" });
console.log(`GET /mediciones -> ${lista.code} ${lista.payload}`);

const borrar = await pedir({ pathname: creados[0], method: "DELETE" });
console.log(`DELETE ${creados[0]} -> ${borrar.code}`); // 2.02 Deleted

const otra = await pedir({ pathname: creados[0] });
console.log(`GET ${creados[0]} -> ${otra.code}`); // 4.04: ya no existe
