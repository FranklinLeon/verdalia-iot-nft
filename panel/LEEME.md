# Panel Verdalia

El mismo panel funciona en dos lugares:

- **En la casa:** lo sirve el ESP32 en http://192.168.0.200 (sin clave).
- **Desde cualquier lado:** publicado en Netlify (pide la clave del panel).

```
ESP32 ──cada 30 s──► Netlify /api/device ──► guarda estado, historial, config
  ▲                         │
  └── config nueva y ◄──────┘    celular ──► Netlify /api/status, /api/config, /api/cmd
      órdenes Forzar
```

Netlify no puede hablarle al ESP32 de la casa (está detrás del router y sin
HTTPS). Por eso el ESP32 es el que llama: deja su estado y se lleva los
cambios. Una orden o un cambio de configuración hecho afuera tarda **menos de
30 s** en aplicarse.

## Archivos

| Archivo | Qué es |
|---|---|
| `public/index.html` | El panel. Al compilar el firmware se mete solo en el ESP32 |
| `public/logo.svg`, `icono*.png` | Copias de `marca/verdalia/` |
| `netlify/functions/` | La API en la nube |
| `netlify.toml` | Configuración del sitio |

Si cambiás el panel: publicarlo en Netlify **y** volver a subir el firmware,
para que los dos queden iguales.

## Variables del sitio en Netlify

| Variable | Valor |
|---|---|
| `DEVICE_KEY` | Igual a `NUBE_CLAVE` de `firmware/monitor_nft/include/config.h` |
| `PANEL_CLAVE` | La clave que escribís en el celular para entrar |

## Publicar

```bash
cd panel
npm install
npx netlify-cli deploy --prod
```

## Costo

Plan gratis de Netlify: 300 créditos al mes. El ESP32 llamando cada 30 s más
el panel abierto un rato al día gastan unos 50–60, más 15 por cada publicación.
Si se acaban los créditos, Netlify pausa el sitio hasta el mes siguiente; el
panel de la casa (192.168.0.200) y las alertas por ntfy siguen andando igual.
