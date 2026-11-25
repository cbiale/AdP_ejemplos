// 2. GET confirmable (CON) y no confirmable (NON).
// Con Wireshark (filtro coap) se ve la diferencia: CON recibe un ACK con
// la respuesta adentro (piggyback); NON recibe otra respuesta NON.
import { pedir } from "./comun.js";

const con = await pedir({ pathname: "/sensores/temp" });
console.log(`GET CON -> ${con.code} ${con.payload}`);

const non = await pedir({ pathname: "/sensores/temp", confirmable: false });
console.log(`GET NON -> ${non.code} ${non.payload}`);

const nada = await pedir({ pathname: "/no/existe" });
console.log(`GET /no/existe -> ${nada.code}`); // 4.04: un error es un código, no un RST
