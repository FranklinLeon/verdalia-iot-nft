// Piezas compartidas por las funciones del panel Verdalia.
import { getStore } from "@netlify/blobs";

// Un solo almacen. "strong" para que una orden o un cambio de configuracion
// se vea al instante (sin eso Netlify puede tardar hasta 60 s en propagar).
export const almacen = () => getStore({ name: "verdalia", consistency: "strong" });

export const HIST_INT_MIN = 5;          // una muestra del historial cada 5 min
export const HIST_HORAS = 24;
export const ORDEN_VENCE_MIN = 10;      // una orden que el ESP32 no recogio en 10 min se descarta

export function json(datos, status = 200) {
  return new Response(JSON.stringify(datos), {
    status,
    headers: { "content-type": "application/json", "cache-control": "no-store" },
  });
}

// Clave del panel (la escribis una vez en el celular). Variable PANEL_CLAVE.
export function panelAutorizado(req) {
  const clave = process.env.PANEL_CLAVE;
  return Boolean(clave) && req.headers.get("x-clave") === clave;
}

// Clave del ESP32. Variable DEVICE_KEY = NUBE_CLAVE de config.h.
export function dispositivoAutorizado(req) {
  const clave = process.env.DEVICE_KEY;
  return Boolean(clave) && req.headers.get("x-device-key") === clave;
}

export const noAutorizado = () => json({ error: "clave" }, 401);

export const numero = (v) => (typeof v === "number" && Number.isFinite(v) ? v : null);
