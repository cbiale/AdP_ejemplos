// 7. Observe (RFC 7641): GET con Observe = 0 y notificaciones durante 10 s.
import { type ObserveReadStream } from "coap";
import { pedir } from "./comun.js";

const obs = (await pedir({ pathname: "/sensores/temp", observe: true })) as ObserveReadStream;
obs.on("data", (d: Buffer) => console.log(`notificación (Observe=${obs.headers["Observe"]}): ${d}`));

setTimeout(() => {
  // Dejamos de escuchar: la próxima notificación recibe un RST y el servidor nos da de baja
  obs.close();
  console.log("observación cancelada");
  setTimeout(() => process.exit(0), 3000);
}, 10_000);
