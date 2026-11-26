# Conexiones del monitor NFT — Quingeo, Cuenca

Referencia de hardware del ESP32 WROOM-32 que monitorea el sistema NFT de lechuga.
Los pines de este documento son los mismos que están definidos en `include/config.h`.

---

## 1. Componentes

| # | Componente | Precio | Función |
|---|---|---|---|
| 1 | ESP32 WROOM-32 (esp32dev) | ya lo tenía | Cerebro, WiFi, servidor web |
| 2 | Sonda DS18B20 digital 1 m **con módulo** | $4,55 | Temperatura del agua/nutriente |
| 3 | Sensor DHT22 / AM2302 | $6,00 | Temperatura y humedad del aire |
| 4 | Sensor de nivel tipo flotador | $4,50 | Aviso de tanque bajo |
| 5 | Módulo relé estado sólido 5V (1 canal) | $2,60 | Conmuta la bomba (120 V) |

**Faltan por comprar / conseguir:**

| Item | Para qué | ¿Obligatorio? |
|---|---|---|
| Resistencia **10 kΩ** (1/4 W) | Pull-up del DHT22 | Sí, si el DHT22 es el pelado de 4 pines |
| Termocontraíble **con adhesivo** | Empalmes sumergidos | Sí |
| Cable 2 conductores baja tensión | Extender flotador y DS18B20 | Sí |
| Fuente 5 V / 1 A (cargador de celu) | Alimentar todo | Sí |
| Caja estanca IP65 | Proteger la electrónica | Sí |
| Fusible 1 A + portafusible | Lado 120 V | Sí |
| Tubo PVC ½" (~80 cm) | Varilla para colgar el flotador | Sí |
| Silicona sellante | Sellar entrada de cable en la varilla | Sí |

---

## 2. Tabla de pines — resumen

| Componente | Señal | Pin ESP32 | Alimentación |
|---|---|---|---|
| DS18B20 (módulo) | DATA | **GPIO4** | **3V3** |
| DHT22 / AM2302 | DATA | **GPIO16** | **3V3** |
| Flotador de nivel | contacto | **GPIO27** ↔ **GND** | ninguna |
| Relé mecánico de la bomba | IN | **GPIO26** | **5V (VIN)** ⚠️ |
| Relé del ventilador (G3MB-202P) | IN | **GPIO25** | **5V (VIN)** ⚠️ |
| LED de estado | — | GPIO2 (onboard) | — |

> **Regla de oro:** todo lo que hable con un GPIO va a **3V3**.
> El ESP32 **no tolera 5 V** en sus pines de entrada. El SSR es la única
> excepción y solo en su pin VCC — su pin IN sigue recibiendo 3,3 V del GPIO26.

---

## 3. Detalle por componente

### 3.1 DS18B20 — temperatura del agua

Es la versión **con módulo**, así que la placa ya trae soldada su resistencia
pull-up de 4,7 kΩ. **No agregues ninguna resistencia.**

| Pin del módulo | Va a |
|---|---|
| VCC / + | 3V3 |
| DATA / OUT / S | GPIO4 |
| GND / − | GND |

- La sonda de acero va **sumergida en el tanque**, cerca de donde está la bomba.
- Viene con 1 m de cable. Si no alcanza, hay que extenderlo (ver §6).

### 3.2 DHT22 / AM2302 — aire y humedad

**Si es el pelado de 4 pines**, mirándolo **de frente** (rejilla hacia vos),
de izquierda a derecha:

| Pin | Función | Va a |
|---|---|---|
| 1 | VCC | 3V3 |
| 2 | DATA | GPIO16 |
| 3 | **NC** | **nada — al aire** |
| 4 | GND | GND |

`NC` significa **"Not Connected"** (no conecta), *no* "normalmente cerrado".
Internamente no está unido a nada. Se deja libre.

**Necesita resistencia de pull-up de 10 kΩ entre DATA y VCC:**

```
  3V3 ──┬────────────────── Pin 1 (VCC)
        │
      [10 kΩ]
        │
        └────────────────── Pin 2 (DATA) ──── GPIO16
```

Sin esa resistencia vas a leer `nan` siempre y te va a llegar la alerta
"Sensor sin lectura". **No sirve el pull-up interno del ESP32** (~45 kΩ,
demasiado débil para el protocolo del DHT22).

**Si en cambio te vino en plaquita de 3 pines** (marcados `+`, `OUT`, `−`):
no hay pin NC y **la resistencia ya viene soldada**. Conectás directo, sin nada extra.

**Ubicación:** colgado en el invernadero, a la altura de las plantas, a la sombra.
Nunca al sol directo ni donde le caiga agua — mide el aire, no el riego.

### 3.3 Flotador de nivel

**No lleva alimentación y no tiene polaridad.** Es un contacto seco (una boya
con un imán que cierra un reed switch). Los dos cables son intercambiables:

```
  Cable 1 ───────── GPIO27
  Cable 2 ───────── GND
```

El pull-up interno del ESP32 le pone los 3,3 V (activado en el firmware con
`INPUT_PULLUP`). Trabaja con microamperios: **no hay riesgo eléctrico**,
podés manipularlo con las manos mojadas.

**Altura de montaje:** un poco **por debajo de la mitad** del tanque (~90 L
restantes). Es un aviso de "rellená pronto", no una protección de marcha en seco.

### 3.4 Relé mecánico — bomba (riego ciclado)

Ciclo: **de día 15 min ON / 15 OFF** (arranca a las :00 y :30), **de noche
15 ON / 45 OFF** (arranca a las :00). Día = 06:00 a 18:00. Si el ESP32 no tiene
la hora de internet, riega en continuo.

**Lado de control:**

| Pin del módulo | Va a |
|---|---|
| VCC | **VIN (5 V)** |
| GND | GND |
| IN | **GPIO26** (`D26`) |

**Lado de carga (120 V):** se usan **COM y NC** (NO queda vacío). Así, si el
ESP32 se muere o se suelta un cable, la bomba sigue andando en continuo.

```
   ENCHUFE ── cable 1 ──[FUSIBLE 1A]──► COM  RELÉ  NC ──► BOMBA
   ENCHUFE ── cable 2 ──────────────────────────────────► BOMBA  (entero)
                                         ▲
                     interruptor de bypass en paralelo COM–NC (recomendado)
```

Si al tocar **Forzar OFF** la bomba sigue andando y con **Forzar ON** se para,
cambiar `BOMBA_ACTIVO_ALTO` a `true` en `config.h`.

### 3.5 Relé G3MB-202P — ventilador

Prende con el aire a **27 °C** y apaga cuando baja a **25 °C** (`VENT_ON_C` /
`VENT_OFF_C` en `config.h`).

**Lado de control (baja tensión):**

| Pin del módulo | Va a |
|---|---|
| VCC / DC+ | **5V (VIN del ESP32)** |
| GND / DC− | GND |
| IN / CH1 | **GPIO25** |

**Lado de carga (120 V):** igual que la bomba, en serie con **la FASE** del
ventilador y con fusible. El neutro va directo.

```
   120V FASE ──[FUSIBLE 1A]── RELÉ (borne 1)
                              RELÉ (borne 2) ──── VENTILADOR (vivo)
   120V NEUTRO ───────────────────────────────── VENTILADOR (neutro)
```

⚠️ **El G3MB-202P solo conmuta corriente ALTERNA, máximo 2 A.** Con un
ventilador de 12 V DC no sirve: prende y no vuelve a apagar. Para un ventilador
DC hace falta un relé mecánico o un módulo MOSFET.

Si el ventilador arranca solo al energizar el ESP32, el módulo dispara en BAJO:
cambiar `VENT_ACTIVO_ALTO` a `false`.

**Sin sensor de aire** (DHT22 más de 10 min sin lectura) el ventilador queda
prendido solo de 11:00 a 14:00 (`VENT_SIN_SENSOR_DESDE` / `VENT_SIN_SENSOR_HASTA`).

**Contador de uso:** el dashboard muestra cuánto estuvo prendido hoy, ayer, este
mes y el mes anterior. Se graba en la flash cada 10 min, así que un corte de luz
pierde a lo sumo esos minutos.

---

## 4. Alimentación y masa común

```
  Cargador 5V/1A ──► VIN del ESP32 (o por el puerto USB)
                     │
                     ├─► 3V3 (regulador interno) ──► DS18B20, DHT22
                     └─► 5V  ─────────────────────► VCC del módulo SSR
```

**Masa común, obligatorio.** Todos los GND al mismo punto: ESP32, DS18B20,
DHT22, flotador y SSR. Si el SSR queda con masa separada no dispara, o
dispara de forma errática.

Consumo total: DS18B20 ~1,5 mA + DHT22 ~2,5 mA + SSR ~20 mA + ESP32 ~150 mA
con WiFi. **Cualquier cargador de celular de 5 V / 1 A sobra.**

---

## 5. Diagrama general

```
                        ┌─────────────────────────┐
      3V3 ──────────────┤                         │
      GND ──────────────┤     ESP32 WROOM-32      │
                        │                         │
   DS18B20 ── GPIO4 ────┤ (módulo, pull-up propio)│
                        │                         │
    DHT22  ── GPIO16 ───┤ + resistencia 10k a 3V3 │
                        │                         │
  FLOTADOR ── GPIO27 ───┤ (el otro cable a GND)   │
                        │                         │
    SSR IN ── GPIO26 ───┤ (VCC del SSR a 5V/VIN)  │
                        │                         │
       LED ── GPIO2 ────┤ onboard                 │
                        └─────────────────────────┘
                                    │
                          SSR ──► BOMBA 120V 50W
```

**LED de estado (GPIO2):**
- Parpadeo lento (2 s) → todo normal
- Parpadeo rápido (0,2 s) → hay alguna alerta activa

---

## 6. Montaje físico y empalmes

### Flotador colgado en varilla — sin agujerear el tanque

El tanque va enterrado 0,70 m, así que un flotador atornillado al costado
quedaría bajo tierra: inaccesible e irreparable si filtra.

1. Tramo de **PVC ½" de ~80 cm**.
2. Atornillar el flotador en el extremo inferior con su rosca y tuerca.
3. **Colgar la varilla del borde del tanque** con abrazadera o amarras plásticas.
4. Ajustar la altura hasta el punto de disparo deseado.

Ventajas: cero riesgo de fuga, altura regulable, y se saca para limpiar cuando
cambies la solución nutritiva.

Verificá que el brazo del flotador **pivotee libre**, sin tocar la pared del
tanque ni la manguera de la bomba.

### Empalmes sumergidos — el cobre NO puede tocar el agua

No es riesgo eléctrico (3,3 V, microamperios). El problema es otro:

1. **El cobre es tóxico para la lechuga.** Lo necesita en el orden de 0,05 ppm.
   Cobre pelado corroyéndose en 200 L que recirculan → raíces marrones y
   crecimiento frenado.
2. **Se corroe y falla el sensor.** Sales + cobre + potencial DC = electrólisis.
   En semanas tenés lecturas erráticas y falsas alarmas.
3. **Ensucia la lectura de EC**, que es el número con el que manejás la nutrición.

**Solución — usar el tubo de PVC como conducto seco:**

1. Agujero de entrada del cable en el **extremo inferior** del tubo.
2. Meter el cable corto del sensor **hacia adentro**.
3. Hacer el empalme **adentro del tubo, en seco**.
4. El cable de extensión sube por dentro y sale por arriba, fuera del agua.
5. **Sellar con silicona** el agujero de entrada, por fuera.

**Cómo empalmar:**
- **Termocontraíble CON adhesivo** ("con pegamento" / *adhesive lined*). Al
  calentarlo, la goma interna sella. El termocontraíble común **no es estanco**.
- Empalmar los dos cables **desfasados** (uno más arriba que el otro) para que
  las uniones no queden enfrentadas.
- Termocontraíble sobre cada empalme + **uno grande sobre los dos**.
- **Nunca cinta aislante.** Con humedad se despega en semanas.

Aplica igual al **DS18B20**, que también va sumergido.

### Caja estanca

El invernadero es húmedo. ESP32, resistencia, borneras y SSR van dentro de una
**caja IP65**. Los cables entran por prensaestopas, no por agujeros abiertos.

---

## 7. Calibración — los dos flags

Están en `include/config.h`. Se ajustan **después** de armar, con el dashboard
abierto:

| Flag | Síntoma | Qué hacer |
|---|---|---|
| `FLOAT_INVERTIR` | Dice `RELLENAR` con el tanque lleno | Cambiar `false` → `true` |
| `BOMBA_ACTIVO_ALTO` | "Forzar OFF" no para la bomba (funciona al revés) | Cambiar `false` → `true` |
| `VENT_ACTIVO_ALTO` | El ventilador arranca solo al energizar el ESP32 | Cambiar `true` → `false` |

Después de cambiar cualquiera de los dos: `pio run -t upload`.

---

## 8. Checklist de encendido

Hacer **en este orden**. No conectar los 120 V hasta el paso 6.

1. **Solo sensores conectados**, sin el lado AC del SSR. Alimentar por USB.
2. Abrir el monitor serial y anotar la IP. Abrir el dashboard en el celular.
3. **Agarrar la sonda del DS18B20 con la mano** → la temperatura del agua debe
   subir en segundos. Si muestra `--`, revisar el cableado del DS18B20.
4. **Soplar sobre el DHT22** → la humedad debe saltar. Si muestra `--`,
   falta la resistencia de 10 kΩ.
5. **Mover la boya del flotador** y esperar 5 s → debe pasar a `RELLENAR` y
   llegar la notificación. Si está invertido, ajustar `FLOAT_INVERTIR`.
6. Recién ahora **conectar el lado AC del SSR** con el fusible puesto.
7. Probar con los botones **Forzar ON / Forzar OFF** del dashboard.
8. Dejar andando una hora y **tocar el SSR**: si quema, cambiarlo por uno con
   disipador.

---

## 9. Referencia rápida del firmware

### Umbrales de alerta

| Alerta | Dispara | Prioridad | Insiste cada |
|---|---|---|---|
| TANQUE BAJO - rellenar | Flotador bajo 5 s seguidos | urgent | **10 min** |
| RIESGO DE HELADA | Aire ≤ **3 °C** | high | 30 min |
| CALOR EN EL INVERNADERO | Aire ≥ **30 °C** (se levanta a 28,5 °C) | high | 30 min |
| Agua muy fría | Agua < **8 °C** | default | 30 min |
| Agua muy caliente | Agua > **26 °C** | default | 30 min |
| Sensor sin lectura | DS18B20 o DHT22 **más de 10 min** sin datos | default | 30 min |
| El monitor cambio de IP | El router le dio otra IP al reconectar | high | — |
| Monitor NFT encendido | Al arrancar/reiniciar | low | — |

Cada alerta manda también su aviso de recuperación. La de arranque sirve de
chivato: si llega sin que hayas tocado nada, **hubo corte de luz o reinició el
watchdog**.

### Solución nutritiva

Dosis en **ml por litro** del agua que se agrega (total − actual):

| Concentrado | ml/L | Para 200 L |
|---|---|---|
| Solución A | 5,10 | 1020 ml |
| Solución B | 2,70 | 540 ml |
| Solución C | 2,70 | 540 ml |

> Valores deducidos de la app de Aqua Payana. **Confirmar contra la etiqueta
> del concentrado que se compre** — si cambia la marca, cambian las dosis
> (`DOSIS_A_ML_L` y siguientes en `config.h`).
>
> Disolver cada concentrado **por separado** y en ese orden. Nunca mezclar
> A y B puros: el calcio precipita con los sulfatos.

### Endpoints web

| URL | Devuelve |
|---|---|
| `/` | Panel Verdalia (el mismo de Netlify, ver `panel/LEEME.md`) |
| `GET /api/status` | JSON de estado actual — el navegador lo pide cada 5 s |
| `GET /api/history` | JSON con las 288 muestras de 24 h — cada 60 s |
| `GET` / `POST /api/config` | Configuración del engranaje (riego, ventilador, alertas, dosis) |
| `POST /api/cmd` | `{"dispositivo":"bomba"\|"ventilador","modo":"on"\|"off"\|"auto"}` |

El historial vive en RAM: **se pierde al reiniciar el ESP32**.

### Modo de riego

Por defecto **continuo día y noche** (`MODO_RIEGO_DIA` / `MODO_RIEGO_NOCHE`).
Para probar el esquema de Aqua Payana (90 s ON / 420 s OFF) solo de noche:

```c
#define MODO_RIEGO_NOCHE    RIEGO_PULSADO
```

### Latido a healthchecks.io — el aviso de "se murió"

ntfy solo funciona **mientras el ESP32 esté vivo**. Un aparato muerto no manda
notificaciones. healthchecks.io hace lo contrario: espera un ping cada 15 min y
**te avisa cuando deja de llegar**.

Configuración (gratis, 3 minutos):
1. Crear cuenta en <https://healthchecks.io>
2. **Add Check** → nombre "Monitor NFT Quingeo"
3. **Period: 20 minutes** | **Grace Time: 20 minutes** (avisa tras ~40 min de
   silencio, sin falsas alarmas por un WiFi que parpadea)
4. Copiar el UUID de la URL de ping y pegarlo en `HEALTHCHECK_UUID` (`config.h`)
5. En **Integrations**, activar el email

Con `HEALTHCHECK_UUID` vacío el latido queda desactivado y el firmware no
intenta nada.

### Notificaciones en el celular

App **ntfy** (Play Store), suscrita al topic `tu-topic-largo-y-raro`.

Para que lleguen con la app cerrada hay que hacer las cuatro cosas:
1. En ntfy: topic → menú **⋮** → **Instant delivery** activado.
2. Ajustes → Apps → ntfy → Batería → **Sin restricciones**.
3. Inicio automático / Autostart activado (Xiaomi, Huawei, Oppo, Samsung).
4. Notificaciones permitidas con prioridad alta.
