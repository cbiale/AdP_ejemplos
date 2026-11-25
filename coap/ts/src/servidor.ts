// Servidor CoAP: recurso observable, actuador con PUT, FETCH e iPATCH (RFC 8132),
// payload grande (Block2 automático) y descubrimiento en /.well-known/core (RFC 6690).
import coap, { type IncomingMessage, type OutgoingMessage } from "coap";

let temp = 22.0;
let led = "off";
const valvula = { apertura: 0, modo: "manual" };
const observadores = new Set<OutgoingMessage>();
const mediciones = new Map<number, unknown>(); // colección para POST/DELETE
let proximoId = 1;

// Simula el sensor y notifica a los observadores cuando cambia el valor.
setInterval(() => {
  temp = Number((temp + (Math.random() - 0.5)).toFixed(1));
  for (const res of observadores) res.write(JSON.stringify({ temp }));
}, 2000);

const recursos: Record<string, string> = {
  "/sensores/temp": '</sensores/temp>;rt="temperature";if="sensor";obs;ct=50',
  "/actuadores/led": '</actuadores/led>;rt="light";if="actuator";ct=0',
  "/log": '</log>;rt="log";sz=1900',
  "/sensores": '</sensores>;rt="sensors";ct=50',
  "/actuadores/valvula": '</actuadores/valvula>;rt="valve";if="actuator";ct=50',
  "/mediciones": '</mediciones>;rt="collection";ct=50',
};

const server = coap.createServer((req: IncomingMessage, res: OutgoingMessage) => {
  const url = req.url.split("?")[0];
  console.log(`${req.method} ${url}${req.headers["Observe"] === 0 ? " (observe)" : ""}`);

  if (url === "/.well-known/core" && req.method === "GET") {
    res.setOption("Content-Format", "application/link-format");
    return res.end(Object.values(recursos).join(","));
  }

  if (url === "/sensores/temp" && req.method === "GET") {
    res.setOption("Content-Format", "application/json");
    if (req.headers["Observe"] === 0) {
      // Registro de observación: la respuesta queda abierta y se usa write().
      observadores.add(res);
      res.on("finish", () => observadores.delete(res));
      return res.write(JSON.stringify({ temp }));
    }
    return res.end(JSON.stringify({ temp }));
  }

  if (url === "/actuadores/led") {
    if (req.method === "GET") return res.end(led);
    if (req.method === "PUT") {
      const v = req.payload.toString();
      if (v !== "on" && v !== "off") { res.code = "4.00"; return res.end("usar on|off"); }
      led = v;
      res.code = "2.04"; // Changed
      return res.end();
    }
    res.code = "4.05"; return res.end(); // Method Not Allowed
  }

  // FETCH (RFC 8132): un GET con cuerpo. El cuerpo dice qué campos queremos.
  if (url === "/sensores" && req.method === "FETCH") {
    const todos: Record<string, number> = { temp, hum: 61, suelo: 34, bateria: 3.7 };
    let campos: string[];
    try { campos = JSON.parse(req.payload.toString()).campos; } catch { res.code = "4.00"; return res.end("JSON inválido"); }
    const parcial = Object.fromEntries(campos.filter((c) => c in todos).map((c) => [c, todos[c]]));
    res.setOption("Content-Format", "application/json");
    return res.end(JSON.stringify(parcial));
  }

  // iPATCH: cambio parcial idempotente. Repetirlo deja el mismo estado,
  // así que no importa si un CON se retransmite.
  if (url === "/actuadores/valvula") {
    if (req.method === "GET") { res.setOption("Content-Format", "application/json"); return res.end(JSON.stringify(valvula)); }
    if (req.method === "iPATCH") {
      try { Object.assign(valvula, JSON.parse(req.payload.toString())); } catch { res.code = "4.00"; return res.end("JSON inválido"); }
      res.code = "2.04";
      return res.end();
    }
    res.code = "4.05"; return res.end();
  }

  // Colección: POST crea un recurso nuevo (el servidor elige la URI), DELETE lo borra.
  if (url === "/mediciones") {
    if (req.method === "GET") {
      res.setOption("Content-Format", "application/json");
      return res.end(JSON.stringify(Object.fromEntries(mediciones)));
    }
    if (req.method === "POST") {
      let m: unknown;
      try { m = JSON.parse(req.payload.toString()); } catch { res.code = "4.00"; return res.end("JSON inválido"); }
      const id = proximoId++;
      mediciones.set(id, m);
      res.code = "2.01"; // Created
      // Location-Path es repetible: una opción por segmento -> /mediciones/<id>
      res.setOption("Location-Path", [Buffer.from("mediciones"), Buffer.from(String(id))]);
      return res.end();
    }
    res.code = "4.05"; return res.end();
  }
  const item = url.match(/^\/mediciones\/(\d+)$/);
  if (item) {
    const id = Number(item[1]);
    if (!mediciones.has(id)) { res.code = "4.04"; return res.end(); }
    if (req.method === "GET") {
      res.setOption("Content-Format", "application/json");
      return res.end(JSON.stringify(mediciones.get(id)));
    }
    if (req.method === "DELETE") {
      mediciones.delete(id);
      res.code = "2.02"; // Deleted
      return res.end();
    }
    res.code = "4.05"; return res.end();
  }

  if (url === "/log" && req.method === "GET") {
    // ~1,9 KB (más que un datagrama de 1024 B): el servidor lo parte en bloques (Block2) de forma transparente.
    const lineas = Array.from({ length: 100 }, (_, i) => `muestra ${i}: ${(20 + i / 10).toFixed(1)} C`);
    return res.end(lineas.join("\n"));
  }

  res.code = "4.04"; res.end();
});

server.listen(5683, () => console.log("CoAP escuchando en udp/5683"));
