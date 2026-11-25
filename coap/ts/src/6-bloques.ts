// 6. Transferencia por bloques (Block2, RFC 7959). Solo contra el servidor TypeScript.
// node-coap pide los bloques y entrega el payload ya reensamblado.
import { pedir } from "./comun.js";

const res = await pedir({ pathname: "/log" });
const texto = res.payload.toString();
console.log(`${res.code}: ${res.payload.length} bytes, ${texto.split("\n").length} líneas`);
console.log(texto.split("\n").slice(0, 3).join("\n"), "\n...");
