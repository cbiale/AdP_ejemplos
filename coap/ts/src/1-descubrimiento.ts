// 1. Descubrimiento: GET /.well-known/core (RFC 6690).
import { pedir } from "./comun.js";

const res = await pedir({ pathname: "/.well-known/core" });
console.log(`código ${res.code}, Content-Format ${res.headers["Content-Format"]}`);
for (const enlace of res.payload.toString().split(",")) console.log("  ", enlace);
