// 3. PUT sobre un actuador: npm run put -- on   (o off)
import { pedir } from "./comun.js";

const valor = process.argv[2] ?? "on";
const put = await pedir({ pathname: "/actuadores/led", method: "PUT" }, valor);
console.log(`PUT led=${valor} -> ${put.code}`); // 2.04 Changed o 4.00 Bad Request

const get = await pedir({ pathname: "/actuadores/led" });
console.log(`GET led -> ${get.payload}`);
