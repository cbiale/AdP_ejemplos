// 5. FETCH e iPATCH (RFC 8132). Solo contra el servidor TypeScript.
import { pedir } from "./comun.js";

const json = { "Content-Format": "application/json" } as const;

// FETCH: un GET con cuerpo; el cuerpo dice qué campos queremos
const f = await pedir({ pathname: "/sensores", method: "FETCH", options: json }, JSON.stringify({ campos: ["temp", "hum"] }));
console.log(`FETCH /sensores -> ${f.code} ${f.payload}`);

// iPATCH: cambio parcial e idempotente (repetirlo no cambia el resultado)
for (let i = 0; i < 2; i++) {
  const p = await pedir({ pathname: "/actuadores/valvula", method: "iPATCH", options: json }, JSON.stringify({ apertura: 50 }));
  const v = await pedir({ pathname: "/actuadores/valvula" });
  console.log(`iPATCH #${i + 1} -> ${p.code}; estado ${v.payload}`);
}
