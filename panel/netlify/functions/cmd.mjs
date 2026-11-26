// Forzar ON / OFF / AUTO desde el panel. Queda pendiente hasta que el
// ESP32 la recoge y la confirma.
import { almacen, json, panelAutorizado, noAutorizado } from "../lib/comun.mjs";

const DISPOSITIVOS = ["bomba", "ventilador"];
const MODOS = ["on", "off", "auto"];

export default async (req) => {
  if (!panelAutorizado(req)) return noAutorizado();
  if (req.method !== "POST") return json({ error: "metodo" }, 405);

  let cuerpo;
  try {
    cuerpo = await req.json();
  } catch {
    return json({ error: "json" }, 400);
  }
  const { dispositivo, modo } = cuerpo;
  if (!DISPOSITIVOS.includes(dispositivo) || !MODOS.includes(modo)) {
    return json({ error: "orden" }, 400);
  }

  const s = almacen();
  const ordenes = (await s.get("ordenes", { type: "json" })) || {};
  // id siempre creciente: el ESP32 aplica una orden solo si su id es mayor
  // que la ultima que ya aplico.
  const id = Math.max(Math.floor(Date.now() / 1000), (ordenes.ultimo_id || 0) + 1);
  ordenes[dispositivo] = { modo, id, t: Date.now() };
  ordenes.ultimo_id = id;
  await s.setJSON("ordenes", ordenes);
  return json({ ok: true, pendiente: true });
};

export const config = { path: "/api/cmd" };
