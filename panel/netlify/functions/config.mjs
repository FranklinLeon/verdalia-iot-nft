// Configuracion del engranaje. GET la lee; POST la cambia y el ESP32 la
// recoge en su proxima llamada (menos de 30 s).
import { almacen, json, panelAutorizado, noAutorizado, numero } from "../lib/comun.mjs";

export default async (req) => {
  if (!panelAutorizado(req)) return noAutorizado();
  const s = almacen();
  const actual = await s.get("config", { type: "json" });

  if (req.method === "GET") {
    if (!actual) return json({ error: "sin_datos" }, 404);
    return json(actual);
  }
  if (req.method !== "POST") return json({ error: "metodo" }, 405);
  if (!actual) return json({ error: "El invernadero todavia no se conecto nunca" }, 409);

  let cuerpo;
  try {
    cuerpo = await req.json();
  } catch {
    return json({ error: "json" }, 400);
  }

  // Solo se aceptan campos que el ESP32 ya conoce y solo numeros. Los rangos
  // los controla el firmware, que recorta lo que este fuera de limite.
  const cfg = { ...actual.cfg };
  for (const [clave, valor] of Object.entries(cuerpo.cfg || {})) {
    if (clave in cfg && numero(valor) !== null) cfg[clave] = valor;
  }
  const ver = Math.max(Math.floor(Date.now() / 1000), actual.ver + 1);
  await s.setJSON("config", { ver, cfg });
  return json({ ver, cfg });
};

export const config = { path: "/api/config" };
