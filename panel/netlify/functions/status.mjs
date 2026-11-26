// Ultimo estado que mando el ESP32, para el panel.
import { almacen, json, panelAutorizado, noAutorizado } from "../lib/comun.mjs";

export default async (req) => {
  if (!panelAutorizado(req)) return noAutorizado();
  const s = almacen();
  const [estado, ordenes] = await Promise.all([
    s.get("estado", { type: "json" }),
    s.get("ordenes", { type: "json" }),
  ]);
  if (!estado) return json({ fuente: "nube", sin_datos: true });

  const { recibido, ult_muestra, ...resto } = estado;
  return json({
    ...resto,
    fuente: "nube",
    edad_s: Math.round((Date.now() - recibido) / 1000),
    pendientes: {
      bomba: ordenes?.bomba?.modo || null,
      ventilador: ordenes?.ventilador?.modo || null,
    },
  });
};

export const config = { path: "/api/status" };
