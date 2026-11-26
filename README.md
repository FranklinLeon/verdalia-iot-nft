<p align="center"><img src="docs/logo.png" alt="Verdalia" width="320"></p>

<h1 align="center">Verdalia — Monitoreo y control IoT de un invernadero hidropónico</h1>

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-WROOM--32-E7352C?logo=espressif&logoColor=white" alt="ESP32">
  <img src="https://img.shields.io/badge/C%2B%2B-PlatformIO-FF7F00?logo=platformio&logoColor=white" alt="PlatformIO">
  <img src="https://img.shields.io/badge/Netlify-Functions-00C7B7?logo=netlify&logoColor=white" alt="Netlify">
  <img src="https://img.shields.io/badge/alertas-ntfy-317F6F" alt="ntfy">
</p>

Sistema IoT **en uso real** que vigila y automatiza un invernadero hidropónico NFT de lechuga en
Quingeo (Cuenca, Ecuador). Un ESP32 mide el agua y el aire, controla la bomba de riego y el
ventilador, avisa al celular cuando algo sale de rango y se puede ver y manejar desde cualquier lugar.

## Qué hace

- **Sensores:** temperatura del agua (DS18B20), temperatura y humedad del aire (DHT22) y nivel del tanque (flotador).
- **Riego ciclado** día/noche con un relé de estado sólido (bomba de 120 V AC).
- **Ventilación automática** por temperatura del aire, con histéresis y contador de horas de uso.
- **Alertas al celular** con [ntfy](https://ntfy.sh): helada, calor, agua fuera de rango y tanque bajo, con recordatorios.
- **Latido a healthchecks.io:** si el ESP32 deja de reportar (se colgó o se cortó la luz), llega un aviso.
- **Panel web** con gráficos de las últimas 24 h, control manual (on / off / auto) y una calculadora de solución nutritiva A/B/C.
- **Configuración editable desde el panel**, guardada en la flash del ESP32 (NVS): sobrevive a reinicios sin volver a programarlo.

## Arquitectura

```
                       cada 30 s
 ESP32 (invernadero) ───────────────►  Netlify Functions  /api/device
   │  sensores, bomba, ventilador      guarda estado, historial y config
   │  ◄──────────────────────────────  y devuelve cambios y comandos pendientes
   │                                          ▲
   │  panel local (misma API)                 │  /api/status /api/config /api/cmd
   ▼                                          │
 celular en la casa                     celular desde cualquier lugar (con clave)

 ESP32 ──► ntfy.sh (alertas)      ESP32 ──► healthchecks.io (latido)
```

- **El mismo panel sirve en los dos lados.** Al compilar, `scripts/embed_web.py` comprime `panel/public/` y lo
  incrusta en el firmware; el ESP32 expone la misma API que Netlify, así el panel no distingue dónde está.
- **Netlify no puede hablarle al ESP32** (está detrás del router de la casa), así que el ESP32 consulta la nube
  cada 30 s y en la respuesta recoge la configuración nueva y los botones que se tocaron afuera.
- Watchdog, IP fija y reintentos de WiFi para que funcione semanas sin intervención.

## Estructura

| Carpeta | Contenido |
|---|---|
| `firmware/monitor_nft/` | Firmware del ESP32 (C++, PlatformIO). Cableado completo en `CONEXIONES.md` |
| `panel/public/` | Panel web (HTML + JS, sin frameworks) |
| `panel/netlify/functions/` | API en la nube (Netlify Functions + Blobs) |
| `docs/` | Diagrama del manifold de riego |

## Cómo usarlo

1. Copia `firmware/monitor_nft/include/config.example.h` a `config.h` y completa tu WiFi, tu topic de ntfy,
   la clave del dispositivo y el UUID de healthchecks.io (`config.h` está en `.gitignore`).
2. Abre `firmware/monitor_nft` en VS Code con PlatformIO y sube el firmware al ESP32.
3. (Opcional) Publica `panel/` en Netlify y define las variables `PANEL_CLAVE` y `DEVICE_KEY`
   (esta última igual a `NUBE_CLAVE` de `config.h`).

> ⚠️ El relé conmuta 120 V AC: usa fusible, borneras y caja estanca.

## Autor

**Franklin León** — Ingeniero en Telecomunicaciones (Universidad de Cuenca).
