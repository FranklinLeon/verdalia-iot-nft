# Mete el panel web (panel/public/) dentro del firmware.
#
# Corre solo antes de cada compilacion (extra_scripts en platformio.ini).
# Comprime cada archivo con gzip y escribe include/web_files.h; el ESP32
# los sirve tal cual con "Content-Encoding: gzip". Asi el panel del ESP32
# y el de Netlify son EXACTAMENTE el mismo archivo.
Import("env")  # noqa: F821  (lo inyecta PlatformIO)

import gzip
import os

PROYECTO = env.subst("$PROJECT_DIR")  # noqa: F821
WEB = os.path.normpath(os.path.join(PROYECTO, "..", "..", "panel", "public"))
SALIDA = os.path.join(PROYECTO, "include", "web_files.h")

MIME = {
    ".html": "text/html; charset=utf-8",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".js": "application/javascript",
    ".css": "text/css",
}

archivos = sorted(f for f in os.listdir(WEB) if os.path.splitext(f)[1] in MIME)
lineas = [
    "// GENERADO por scripts/embed_web.py a partir de panel/public/. No editar a mano.",
    "#pragma once",
    "#include <Arduino.h>",
    "",
    "struct WebFile { const char *ruta; const char *mime; const uint8_t *datos; size_t len; };",
    "",
]
tabla = []
total = 0
for i, nombre in enumerate(archivos):
    with open(os.path.join(WEB, nombre), "rb") as f:
        gz = gzip.compress(f.read(), 9, mtime=0)
    total += len(gz)
    bytes_txt = ",".join(str(b) for b in gz)
    lineas.append(f"static const uint8_t WEB_{i}[] PROGMEM = {{{bytes_txt}}};")
    mime = MIME[os.path.splitext(nombre)[1]]
    tabla.append(f'  {{"/{nombre}", "{mime}", WEB_{i}, {len(gz)}}},')

lineas += ["", "static const WebFile WEB_FILES[] = {", *tabla, "};",
           f"static const size_t WEB_FILES_N = {len(archivos)};", ""]
nuevo = "\n".join(lineas)

viejo = None
if os.path.exists(SALIDA):
    with open(SALIDA, encoding="utf-8") as f:
        viejo = f.read()
if nuevo != viejo:
    with open(SALIDA, "w", encoding="utf-8") as f:
        f.write(nuevo)
print(f"[embed_web] {len(archivos)} archivos del panel, {total / 1024:.1f} KB comprimidos")
