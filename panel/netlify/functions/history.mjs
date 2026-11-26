// Ultimas 24 h en el mismo formato que da el ESP32: una lista por variable,
// una posicion cada 5 min, null donde no hubo dato (corte de luz o de internet).
import { almacen, json, panelAutorizado, noAutorizado, HIST_INT_MIN, HIST_HORAS } from "../lib/comun.mjs";

export default async (req) => {
  if (!panelAutorizado(req)) return noAutorizado();
  const hist = (await almacen().get("historial", { type: "json" })) || [];

  const paso = HIST_INT_MIN * 60000;
  const n = (HIST_HORAS * 60) / HIST_INT_MIN;
  const fin = Date.now();
  const inicio = fin - n * paso;
  const agua = new Array(n).fill(null);
  const aire = new Array(n).fill(null);
  const hum = new Array(n).fill(null);
  for (const m of hist) {
    const i = Math.floor((m.t - inicio) / paso);
    if (i < 0 || i >= n) continue;
    agua[i] = m.agua;
    aire[i] = m.aire;
    hum[i] = m.hum;
  }
  return json({ int_min: HIST_INT_MIN, agua, aire, hum });
};

export const config = { path: "/api/history" };
