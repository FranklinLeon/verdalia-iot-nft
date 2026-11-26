// El ESP32 llama aca cada 30 s: deja su estado y se lleva la configuracion
// nueva y las ordenes (Forzar ON/OFF/AUTO) que se hayan dado desde el panel.
import {
  almacen, json, dispositivoAutorizado, noAutorizado, numero,
  HIST_INT_MIN, HIST_HORAS, ORDEN_VENCE_MIN,
} from "../lib/comun.mjs";

export default async (req) => {
  if (req.method !== "POST") return json({ error: "metodo" }, 405);
  if (!dispositivoAutorizado(req)) return noAutorizado();

  let cuerpo;
  try {
    cuerpo = await req.json();
  } catch {
    return json({ error: "json" }, 400);
  }
  const estado = cuerpo.estado || {};
  const ahora = Date.now();
  const s = almacen();

  const [anterior, cfgNube, ordenes] = await Promise.all([
    s.get("estado", { type: "json" }),
    s.get("config", { type: "json" }),
    s.get("ordenes", { type: "json" }),
  ]);

  // --- Historial: una muestra cada 5 min, se guardan las ultimas 24 h ---
  let ultMuestra = anterior?.ult_muestra || 0;
  if (ahora - ultMuestra >= HIST_INT_MIN * 60000 - 15000) {
    const hist = (await s.get("historial", { type: "json" })) || [];
    hist.push({
      t: ahora,
      agua: numero(estado.temp_agua),
      aire: numero(estado.temp_aire),
      hum: numero(estado.humedad),
    });
    const corte = ahora - HIST_HORAS * 3600000;
    await s.setJSON("historial", hist.filter((m) => m.t >= corte));
    ultMuestra = ahora;
  }
  await s.setJSON("estado", { ...estado, recibido: ahora, ult_muestra: ultMuestra });

  const respuesta = {};

  // --- Configuracion: gana la version mas nueva ---
  // Si el ESP32 tiene la misma version o una mas nueva, su configuracion es
  // la que vale (ya viene validada y recortada por el firmware).
  const verEsp = Number(cuerpo.cfg_ver) || 0;
  if (!cfgNube || verEsp >= cfgNube.ver) {
    if (cuerpo.cfg && JSON.stringify(cuerpo.cfg) !== JSON.stringify(cfgNube?.cfg)) {
      await s.setJSON("config", { ver: verEsp, cfg: cuerpo.cfg });
    }
  } else {
    respuesta.cfg = cfgNube.cfg;
    respuesta.cfg_ver = cfgNube.ver;
  }

  // --- Ordenes: se borran las que el ESP32 ya confirmo y las viejas ---
  const ack = cuerpo.ack || {};
  const pendientes = { ...(ordenes || {}) };
  let cambio = false;
  for (const disp of ["bomba", "ventilador"]) {
    const o = pendientes[disp];
    if (!o) continue;
    if (o.id <= (Number(ack[disp]) || 0) || ahora - o.t > ORDEN_VENCE_MIN * 60000) {
      delete pendientes[disp];
      cambio = true;
    }
  }
  if (cambio) await s.setJSON("ordenes", pendientes);
  respuesta.cmds = {};
  for (const disp of ["bomba", "ventilador"]) {
    if (pendientes[disp]) respuesta.cmds[disp] = { id: pendientes[disp].id, modo: pendientes[disp].modo };
  }

  return json(respuesta);
};

export const config = { path: "/api/device" };
