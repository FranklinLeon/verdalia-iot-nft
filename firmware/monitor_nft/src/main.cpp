/* =====================================================================
 *  VERDALIA - Monitor del NFT de lechuga (Cuenca, Ecuador)
 *  ESP32 WROOM-32
 * ---------------------------------------------------------------------
 *  QUE HACE:
 *    1. Mide agua (DS18B20) + aire y humedad (DHT22)
 *    2. Avisa tanque bajo (flotador a media carga)
 *    3. Alarma de helada, de calor y de agua fuera de rango (ntfy)
 *    4. Panel web con graficos de las ultimas 24 h
 *    5. Calculadora de solucion nutritiva A/B/C
 *    6. Riego ciclado dia/noche
 *    7. Ventilador por temperatura del aire, con contador de horas
 *    8. Configuracion editable desde el panel (engranaje), guardada en flash
 *    9. Se reporta al panel en Netlify para verlo desde cualquier lado
 *
 *  PANEL WEB:
 *    El panel es el MISMO archivo que se publica en Netlify
 *    (panel/public/). Al compilar, scripts/embed_web.py lo mete
 *    comprimido en include/web_files.h y el ESP32 lo sirve en su IP.
 *    API local (igual a la de Netlify, asi el panel no distingue):
 *      GET  /api/status   estado actual
 *      GET  /api/history  ultimas 24 h
 *      GET  /api/config   configuracion
 *      POST /api/config   {"cfg":{...}}
 *      POST /api/cmd      {"dispositivo":"bomba|ventilador","modo":"on|off|auto"}
 * ---------------------------------------------------------------------
 *  CABLEADO -> ver CONEXIONES.md
 * ===================================================================== */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <esp_task_wdt.h>
#include <time.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <ArduinoJson.h>

#include "config.h"
#include "web_files.h"   // generado por scripts/embed_web.py al compilar

// ============================== OBJETOS ==============================
OneWire           oneWire(PIN_DS18B20);
DallasTemperature ds(&oneWire);
DHT               dht(PIN_DHT22, DHT22);
WebServer         server(80);
Preferences       prefs;      // contador del ventilador
Preferences       prefsCfg;   // configuracion editable

// ============================== CONFIGURACION ========================
// Todo lo que se cambia desde el engranaje del panel. Arranca con los
// valores de fabrica de config.h y despues vive en la flash (NVS).
struct Config {
  uint8_t  modoDia, modoNoche;           // RIEGO_CONTINUO / RIEGO_PULSADO
  uint16_t diaOnMin, diaOffMin;
  uint16_t nocheOnMin, nocheOffMin;
  uint8_t  horaDia, horaNoche;           // a que hora empieza cada uno
  float    ventOnC, ventOffC;
  uint8_t  ventSinSensorDesde, ventSinSensorHasta;
  float    heladaC, calorC, aguaMinC, aguaMaxC;
  uint16_t sensorTimeoutMin, recordatorioMin, recordatorioNivelMin, manualMin;
  uint16_t tanqueL;
  float    dosisA, dosisB, dosisC;
} cfg;
uint32_t cfgVer = 0;   // version (segundos epoch del ultimo cambio): gana la mas nueva

enum TipoCampo : uint8_t { T_U8, T_U16, T_F };
struct CampoCfg { const char *clave; TipoCampo tipo; void *ptr; float min, max; };

// Nombre en el JSON, tipo, variable y rango permitido. El panel usa los
// mismos nombres; si llega un valor fuera de rango se recorta al limite.
const CampoCfg CAMPOS[] = {
  {"modo_dia",           T_U8,  &cfg.modoDia,              0,   1},
  {"dia_on",             T_U16, &cfg.diaOnMin,             1,   240},
  {"dia_off",            T_U16, &cfg.diaOffMin,            1,   240},
  {"modo_noche",         T_U8,  &cfg.modoNoche,            0,   1},
  {"noche_on",           T_U16, &cfg.nocheOnMin,           1,   240},
  {"noche_off",          T_U16, &cfg.nocheOffMin,          1,   240},
  {"hora_dia",           T_U8,  &cfg.horaDia,              0,   23},
  {"hora_noche",         T_U8,  &cfg.horaNoche,            0,   23},
  {"vent_on",            T_F,   &cfg.ventOnC,              10,  45},
  {"vent_off",           T_F,   &cfg.ventOffC,             5,   44},
  {"vent_ss_desde",      T_U8,  &cfg.ventSinSensorDesde,   0,   23},
  {"vent_ss_hasta",      T_U8,  &cfg.ventSinSensorHasta,   0,   23},
  {"helada",             T_F,   &cfg.heladaC,              -5,  15},
  {"calor",              T_F,   &cfg.calorC,               20,  50},
  {"agua_min",           T_F,   &cfg.aguaMinC,             0,   25},
  {"agua_max",           T_F,   &cfg.aguaMaxC,             15,  40},
  {"sensor_timeout",     T_U16, &cfg.sensorTimeoutMin,     1,   120},
  {"recordatorio",       T_U16, &cfg.recordatorioMin,      5,   720},
  {"recordatorio_nivel", T_U16, &cfg.recordatorioNivelMin, 5,   720},
  {"manual_min",         T_U16, &cfg.manualMin,            1,   720},
  {"tanque_l",           T_U16, &cfg.tanqueL,              10,  5000},
  {"dosis_a",            T_F,   &cfg.dosisA,               0,   50},
  {"dosis_b",            T_F,   &cfg.dosisB,               0,   50},
  {"dosis_c",            T_F,   &cfg.dosisC,               0,   50},
};
const size_t N_CAMPOS = sizeof(CAMPOS) / sizeof(CAMPOS[0]);

void configFabrica() {
  cfg.modoDia              = MODO_RIEGO_DIA;
  cfg.modoNoche            = MODO_RIEGO_NOCHE;
  cfg.diaOnMin             = DIA_ON_MIN;
  cfg.diaOffMin            = DIA_OFF_MIN;
  cfg.nocheOnMin           = NOCHE_ON_MIN;
  cfg.nocheOffMin          = NOCHE_OFF_MIN;
  cfg.horaDia              = HORA_INICIO_DIA;
  cfg.horaNoche            = HORA_INICIO_NOCHE;
  cfg.ventOnC              = VENT_ON_C;
  cfg.ventOffC             = VENT_OFF_C;
  cfg.ventSinSensorDesde   = VENT_SIN_SENSOR_DESDE;
  cfg.ventSinSensorHasta   = VENT_SIN_SENSOR_HASTA;
  cfg.heladaC              = UMBRAL_HELADA_C;
  cfg.calorC               = AIRE_MAX_C;
  cfg.aguaMinC             = AGUA_MIN_C;
  cfg.aguaMaxC             = AGUA_MAX_C;
  cfg.sensorTimeoutMin     = SENSOR_TIMEOUT_MIN;
  cfg.recordatorioMin      = ALERTA_RECORDATORIO_MIN;
  cfg.recordatorioNivelMin = RECORDATORIO_NIVEL_MIN;
  cfg.manualMin            = MANUAL_MIN;
  cfg.tanqueL              = TANQUE_LITROS;
  cfg.dosisA               = DOSIS_A_ML_L;
  cfg.dosisB               = DOSIS_B_ML_L;
  cfg.dosisC               = DOSIS_C_ML_L;
}

// Reglas entre campos que el rango solo no cubre.
void validarConfig() {
  if (cfg.ventOffC >= cfg.ventOnC) cfg.ventOffC = cfg.ventOnC - 1.0f;
  if (cfg.aguaMinC >= cfg.aguaMaxC) { cfg.aguaMinC = AGUA_MIN_C; cfg.aguaMaxC = AGUA_MAX_C; }
  if (cfg.horaDia == cfg.horaNoche) { cfg.horaDia = HORA_INICIO_DIA; cfg.horaNoche = HORA_INICIO_NOCHE; }
}

void cfgAJson(JsonObject o) {
  for (size_t i = 0; i < N_CAMPOS; i++) {
    const CampoCfg &c = CAMPOS[i];
    switch (c.tipo) {
      case T_U8:  o[c.clave] = *(uint8_t  *)c.ptr; break;
      case T_U16: o[c.clave] = *(uint16_t *)c.ptr; break;
      case T_F:   o[c.clave] = roundf(*(float *)c.ptr * 100.0f) / 100.0f; break;
    }
  }
}

// Aplica los campos que vengan en el JSON (los que falten no se tocan).
// Devuelve true si algo cambio.
bool cfgDesdeJson(JsonVariantConst src) {
  bool cambio = false;
  for (size_t i = 0; i < N_CAMPOS; i++) {
    const CampoCfg &c = CAMPOS[i];
    JsonVariantConst v = src[c.clave];
    if (!v.is<float>()) continue;
    float x = constrain(v.as<float>(), c.min, c.max);
    switch (c.tipo) {
      case T_U8: {
        uint8_t n = (uint8_t)lroundf(x);
        if (n != *(uint8_t *)c.ptr) { *(uint8_t *)c.ptr = n; cambio = true; }
      } break;
      case T_U16: {
        uint16_t n = (uint16_t)lroundf(x);
        if (n != *(uint16_t *)c.ptr) { *(uint16_t *)c.ptr = n; cambio = true; }
      } break;
      case T_F: {
        x = roundf(x * 100.0f) / 100.0f;
        if (fabsf(x - *(float *)c.ptr) > 0.001f) { *(float *)c.ptr = x; cambio = true; }
      } break;
    }
  }
  validarConfig();
  return cambio;
}

void guardarConfig() {
  prefsCfg.putBytes("c", &cfg, sizeof(cfg));
  prefsCfg.putUInt("ver", cfgVer);
}

void cargarConfig() {
  configFabrica();
  prefsCfg.begin("cfg", false);
  if (prefsCfg.getBytesLength("c") == sizeof(cfg)) {
    prefsCfg.getBytes("c", &cfg, sizeof(cfg));
    cfgVer = prefsCfg.getUInt("ver", 0);
  }
  validarConfig();
}

uint32_t ahoraEpoch() {
  time_t now = time(nullptr);
  return (now > 1600000000) ? (uint32_t)now : 0;
}

// ============================== ESTADO ===============================
struct Estado {
  float    tempAgua       = NAN;
  float    tempAire       = NAN;
  float    humAire        = NAN;
  bool     nivelOk        = true;
  bool     bombaOn        = false;
  bool     ventOn         = false;
  bool     bloqueoNivel   = false;
  bool     wifiOk         = false;
  uint32_t arranquesBomba = 0;
} st;

String ultimaIp = "";   // para detectar cuando el router le cambia la IP

// Forzar ON/OFF desde el panel. Vuelve solo a AUTO a los cfg.manualMin.
enum ModoManual : uint8_t { MAN_AUTO = 0, MAN_ON, MAN_OFF };
struct Manual { ModoManual modo = MAN_AUTO; uint32_t desde = 0; };
Manual manBomba, manVent;

bool manualVencido(const Manual &m) {
  return (millis() - m.desde) > (uint32_t)cfg.manualMin * 60000UL;
}

uint32_t manualRestanteMin(const Manual &m) {
  if (m.modo == MAN_AUTO || manualVencido(m)) return 0;
  return ((uint32_t)cfg.manualMin * 60000UL - (millis() - m.desde)) / 60000UL;
}

// Estado del enlace con el panel de Netlify
uint32_t wifiCaidas = 0;   // veces que se cayo el WiFi desde el arranque

struct Nube {
  bool     ok      = false;
  uint32_t tUltOk  = 0;
  int      codigo  = 0;
  uint32_t ackBomba = 0, ackVent = 0;   // ultima orden aplicada de cada uno
} nube;
uint32_t tNube = 0;   // 0 = sincronizar ya

// Tiempo de ventilador encendido, en segundos. Se guarda en flash (NVS) para
// que no se pierda con un corte de luz. "dia" y "mes" son la fecha a la que
// pertenecen hoySeg y mesSeg, para saber cuando hay que pasar de hoja.
struct VentContador {
  uint32_t dia       = 0;   // dias desde 1970, hora local
  uint32_t hoySeg    = 0;
  uint32_t ayerSeg   = 0;
  uint32_t mes       = 0;   // anio * 12 + mes
  uint32_t mesSeg    = 0;
  uint32_t mesAntSeg = 0;
} vent;
bool ventSinGuardar = false;

// ============================== HISTORIAL ============================
// Buffer circular. Valores x10 en int16 (21.4 C -> 214). INT16_MIN = sin dato.
struct Muestra { int16_t agua; int16_t aire; int16_t hum; };
Muestra  hist[HIST_MUESTRAS];
uint16_t histIdx    = 0;   // proxima posicion a escribir
uint16_t histLlenas = 0;   // cuantas muestras validas hay

int16_t aInt16(float v) {
  if (isnan(v)) return INT16_MIN;
  return (int16_t)lroundf(v * 10.0f);
}

void guardarMuestra() {
  hist[histIdx].agua = aInt16(st.tempAgua);
  hist[histIdx].aire = aInt16(st.tempAire);
  hist[histIdx].hum  = aInt16(st.humAire);
  histIdx = (histIdx + 1) % HIST_MUESTRAS;
  if (histLlenas < HIST_MUESTRAS) histLlenas++;
}

// ============================== ALERTAS ==============================
enum AlertaId : uint8_t {
  AL_HELADA = 0, AL_CALOR, AL_NIVEL, AL_AGUA_FRIA, AL_AGUA_CALIENTE, AL_SENSOR, AL_TOTAL
};
struct Alerta { bool activa = false; uint32_t ultimoEnvio = 0; };
Alerta alertas[AL_TOTAL];

// Textos SIN tildes a proposito: las cabeceras HTTP de ntfy no las toleran.
void enviarNtfy(const char *titulo, const String &cuerpo,
                const char *prioridad = "default", const char *tags = "seedling") {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[ntfy] sin WiFi, no se envia: %s\n", titulo);
    return;
  }
  HTTPClient http;
  http.setTimeout(8000);
  http.begin(NTFY_URL);
  http.addHeader("Title", titulo);
  http.addHeader("Priority", prioridad);
  http.addHeader("Tags", tags);
  http.addHeader("Content-Type", "text/plain");
  int code = http.POST(cuerpo);
  Serial.printf("[ntfy] %s -> HTTP %d\n", titulo, code);
  http.end();
}

// Latido a healthchecks.io. Es un GET a pelo, sin TLS: si el ESP32 se cuelga
// o se queda sin luz, el ping deja de llegar y healthchecks te avisa a vos.
void enviarLatido() {
  if (strlen(HEALTHCHECK_UUID) == 0) return;          // desactivado
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(8000);
  http.begin(String("http://hc-ping.com/") + HEALTHCHECK_UUID);
  int code = http.GET();
  Serial.printf("[latido] hc-ping -> HTTP %d\n", code);
  http.end();
}

void gestionarAlerta(AlertaId id, bool condicion,
                     const char *tituloAct, const String &msgAct,
                     const char *tituloRec, const String &msgRec,
                     const char *prio, const char *tags, uint16_t recordatorioMin) {
  Alerta  &a     = alertas[id];
  uint32_t ahora = millis();

  if (condicion) {
    if (!a.activa || (ahora - a.ultimoEnvio) > (uint32_t)recordatorioMin * 60000UL) {
      enviarNtfy(tituloAct, msgAct, prio, tags);
      a.ultimoEnvio = ahora;
    }
    a.activa = true;
  } else if (a.activa) {
    enviarNtfy(tituloRec, msgRec, "low", "white_check_mark");
    a.activa      = false;
    a.ultimoEnvio = ahora;
  }
}

// ============================== SENSORES =============================
enum EstadoDs : uint8_t { DS_LIBRE, DS_ESPERANDO };
EstadoDs estadoDs  = DS_LIBRE;
uint32_t tPedidoDs = 0;

// Momento de la ultima lectura VALIDA de cada sensor. Una lectura fallida
// suelta no borra el valor: se conserva el ultimo bueno hasta que pasan
// cfg.sensorTimeoutMin sin ninguna, y recien ahi el sensor se da por caido.
uint32_t tUltAgua = 0, tUltAire = 0;

bool sensorCaido(uint32_t tUltimaLectura) {
  return (millis() - tUltimaLectura) > (uint32_t)cfg.sensorTimeoutMin * 60000UL;
}

void leerAgua() {
  if (estadoDs == DS_LIBRE) {
    ds.requestTemperatures();          // asincrono, no bloquea
    tPedidoDs = millis();
    estadoDs  = DS_ESPERANDO;
  } else if (millis() - tPedidoDs >= 800) {
    float t = ds.getTempCByIndex(0);
    if (t > -100.0f) { st.tempAgua = t; tUltAgua = millis(); }   // -127 = sin lectura
    else if (sensorCaido(tUltAgua)) st.tempAgua = NAN;
    estadoDs = DS_LIBRE;
  }
}

void leerAire() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h)) st.humAire  = h;
  if (!isnan(t)) { st.tempAire = t; tUltAire = millis(); }
  else if (sensorCaido(tUltAire)) { st.tempAire = NAN; st.humAire = NAN; }
}

void leerFlotador() {
  static bool     ultimaLectura = true;
  static uint32_t tCambio       = 0;
  static bool     inicializado  = false;

  bool cerrado = (digitalRead(PIN_FLOAT) == LOW);
  bool hayAgua = FLOAT_INVERTIR ? !cerrado : cerrado;

  if (!inicializado) {
    ultimaLectura = hayAgua;
    st.nivelOk    = hayAgua;
    tCambio       = millis();
    inicializado  = true;
    return;
  }
  if (hayAgua != ultimaLectura) { ultimaLectura = hayAgua; tCambio = millis(); }
  if (hayAgua != st.nivelOk && (millis() - tCambio) > FLOAT_DEBOUNCE_MS) {
    st.nivelOk = hayAgua;
  }
}

// ============================== HORA =================================
// Hora local sin esperar: devuelve false si todavia no llego la hora de
// internet (el reloj del ESP32 arranca en 1970).
bool horaLocal(struct tm &t) {
  time_t now;
  time(&now);
  localtime_r(&now, &t);
  return t.tm_year >= (2016 - 1900);
}

// Arranca el cliente de hora de internet. Se puede llamar aunque no haya
// WiFi: sigue reintentando solo y se sincroniza cuando vuelve la conexion.
void iniciarHora() {
  configTime(GMT_OFFSET_SEG, DST_OFFSET_SEG, "pool.ntp.org", "time.nist.gov");
}

// true si la hora h cae en la franja [desde, hasta). Sirve aunque la
// franja cruce la medianoche (ej. desde 22 hasta 4).
bool horaEnFranja(uint8_t h, uint8_t desde, uint8_t hasta) {
  if (desde < hasta) return h >= desde && h < hasta;
  return h >= desde || h < hasta;
}

bool esHoraNoche(uint8_t h) { return !horaEnFranja(h, cfg.horaDia, cfg.horaNoche); }

bool esNoche() {
  struct tm t;
  if (!horaLocal(t)) return false;
  return esHoraNoche(t.tm_hour);
}

// ============================== BOMBA ================================
// Escribe el pin del rele. Con la bomba en NC, el rele se ACTIVA para
// apagarla y queda en reposo (bomba andando) cuando tiene que regar.
void escribirReleBomba(bool encender) {
  bool activarRele = BOMBA_EN_NC ? !encender : encender;
  digitalWrite(PIN_SSR, (activarRele == BOMBA_ACTIVO_ALTO) ? HIGH : LOW);
}

void setBomba(bool encender) {
  if (encender == st.bombaOn) return;
  escribirReleBomba(encender);
  st.bombaOn = encender;
  if (encender) st.arranquesBomba++;
  Serial.printf("[bomba] %s\n", encender ? "ON" : "OFF");
}

// Que pide el programa en un minuto dado del dia (0..1439).
bool programaEnMinuto(uint16_t minutoDelDia) {
  bool    noche = esHoraNoche(minutoDelDia / 60);
  uint8_t modo  = noche ? cfg.modoNoche : cfg.modoDia;
  if (modo == RIEGO_CONTINUO) return true;
  uint16_t onMin  = noche ? cfg.nocheOnMin  : cfg.diaOnMin;
  uint16_t offMin = noche ? cfg.nocheOffMin : cfg.diaOffMin;
  return (minutoDelDia % (onMin + offMin)) < onMin;
}

bool bombaSegunPrograma() {
  struct tm t;
  if (!horaLocal(t)) return true;      // sin hora no se sabe si es dia: riega
  return programaEnMinuto(t.tm_hour * 60 + t.tm_min);
}

// Hora ("19:00") en que el programa cambia de prendida a apagada o al reves.
// Vacio si no hay hora o si el riego es continuo.
String horaProximoCambio() {
  struct tm t;
  if (!horaLocal(t)) return "";
  uint16_t ahora  = t.tm_hour * 60 + t.tm_min;
  bool     actual = programaEnMinuto(ahora);
  for (uint16_t k = 1; k <= 24 * 60; k++) {
    uint16_t m = (ahora + k) % (24 * 60);
    if (programaEnMinuto(m) != actual) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%02u:%02u", m / 60, m % 60);
      return String(buf);
    }
  }
  return "";
}

void controlBomba() {
  // El flotador esta por debajo de la mitad del tanque: es AVISO de relleno,
  // no proteccion de marcha en seco. Por eso no corta, salvo que actives
  // CORTE_BOMBA_POR_NIVEL (solo tiene sentido con el flotador cerca del fondo).
#if CORTE_BOMBA_POR_NIVEL
  static uint32_t tNivelOkDesde = 0;
  if (!st.nivelOk) {
    st.bloqueoNivel = true;
    tNivelOkDesde   = 0;
  } else {
    if (tNivelOkDesde == 0) tNivelOkDesde = millis();
    if (st.bloqueoNivel &&
        (millis() - tNivelOkDesde) > ((uint32_t)NIVEL_REARME_SEG * 1000UL)) {
      st.bloqueoNivel = false;
      Serial.println("[bomba] nivel recuperado, rearmando");
    }
  }
#else
  st.bloqueoNivel = false;
#endif

  if (manBomba.modo != MAN_AUTO && manualVencido(manBomba)) {
    manBomba.modo = MAN_AUTO;
    Serial.println("[bomba] modo manual expirado -> AUTO");
  }

  bool quiere;
  if      (st.bloqueoNivel)            quiere = false;
  else if (manBomba.modo == MAN_ON)    quiere = true;
  else if (manBomba.modo == MAN_OFF)   quiere = false;
  else                                 quiere = bombaSegunPrograma();

  setBomba(quiere);
}

// ============================== VENTILADOR ===========================
void guardarVentilador() {
  if (!ventSinGuardar) return;
  prefs.putBytes("cnt", &vent, sizeof(vent));
  ventSinGuardar = false;
}

// Suma el tiempo encendido desde la ultima llamada y pasa de hoja el contador
// cuando cambia el dia o el mes. Se llama en cada ciclo de sensores y justo
// antes de cada cambio de estado del ventilador.
void contarVentilador() {
  static uint32_t tPrev = millis(), restoMs = 0;
  uint32_t ahora = millis();
  if (st.ventOn) restoMs += ahora - tPrev;
  tPrev = ahora;
  if (restoMs >= 1000) {
    vent.hoySeg += restoMs / 1000;
    vent.mesSeg += restoMs / 1000;
    restoMs %= 1000;
    ventSinGuardar = true;
  }

  time_t    now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);
  if (t.tm_year < (2016 - 1900)) return;   // todavia sin hora de internet

  uint32_t dia = (uint32_t)((now + GMT_OFFSET_SEG) / 86400);
  uint32_t mes = (uint32_t)t.tm_year * 12 + t.tm_mon;
  if (dia != vent.dia) {
    vent.ayerSeg = (dia - vent.dia == 1) ? vent.hoySeg : 0;
    vent.hoySeg  = 0;
    vent.dia     = dia;
    if (mes != vent.mes) {
      vent.mesAntSeg = (mes - vent.mes == 1) ? vent.mesSeg : 0;
      vent.mesSeg    = 0;
      vent.mes       = mes;
    }
    ventSinGuardar = true;
    guardarVentilador();
  }
}

void setVentilador(bool encender) {
  if (encender == st.ventOn) return;
  contarVentilador();                  // cierra la cuenta del tramo anterior
  digitalWrite(PIN_VENTILADOR, (encender == VENT_ACTIVO_ALTO) ? HIGH : LOW);
  st.ventOn = encender;
  Serial.printf("[ventilador] %s (aire %.1f C)\n", encender ? "ON" : "OFF", st.tempAire);
}

void controlVentilador() {
  if (manVent.modo != MAN_AUTO && manualVencido(manVent)) {
    manVent.modo = MAN_AUTO;
    Serial.println("[ventilador] modo manual expirado -> AUTO");
  }

  bool quiere = st.ventOn;             // entre apagar y prender se queda como estaba
  if (!isnan(st.tempAire)) {
    if      (st.tempAire >= cfg.ventOnC)  quiere = true;
    else if (st.tempAire <= cfg.ventOffC) quiere = false;
  } else if (sensorCaido(tUltAire)) {
    // sin sensor: prendido solo en las horas de mas sol
    struct tm t;
    quiere = horaLocal(t) &&
             horaEnFranja(t.tm_hour, cfg.ventSinSensorDesde, cfg.ventSinSensorHasta);
  }

  if      (manVent.modo == MAN_ON)  quiere = true;
  else if (manVent.modo == MAN_OFF) quiere = false;

  setVentilador(quiere);
}

// Orden de Forzar ON / OFF / AUTO, venga del panel local o de la nube.
bool aplicarOrden(const char *dispositivo, const char *modo) {
  ModoManual m;
  if      (!strcmp(modo, "on"))   m = MAN_ON;
  else if (!strcmp(modo, "off"))  m = MAN_OFF;
  else if (!strcmp(modo, "auto")) m = MAN_AUTO;
  else return false;

  if (!strcmp(dispositivo, "bomba")) {
    manBomba.modo = m; manBomba.desde = millis();
    controlBomba();
  } else if (!strcmp(dispositivo, "ventilador")) {
    manVent.modo = m; manVent.desde = millis();
    controlVentilador();
  } else return false;
  Serial.printf("[orden] %s -> %s\n", dispositivo, modo);
  return true;
}

// ============================== ALERTAS: EVALUAR =====================
void evaluarAlertas() {
  // --- Tanque bajo: solo avisa e insiste ---
  gestionarAlerta(
      AL_NIVEL, !st.nivelOk,
      "TANQUE BAJO - rellenar",
      "El flotador marca menos de media carga. La bomba sigue andando. "
      "Rellena el tanque y revisa la EC, que sube al bajar el volumen.",
      "Tanque OK", "Nivel recuperado.",
      "urgent", "warning,droplet", cfg.recordatorioNivelMin);

  // --- Helada (con histeresis para no spamear) ---
  static bool heladaActiva = false;
  if (!isnan(st.tempAire)) {
    if (st.tempAire <= cfg.heladaC) heladaActiva = true;
    else if (st.tempAire >= cfg.heladaC + HISTERESIS_HELADA_C) heladaActiva = false;
  }
  gestionarAlerta(
      AL_HELADA, heladaActiva,
      "RIESGO DE HELADA",
      "Aire en " + String(st.tempAire, 1) + " C dentro del invernadero. "
      "Cierra las cortinas laterales.",
      "Temperatura normalizada",
      "Aire de vuelta en " + String(st.tempAire, 1) + " C.",
      "high", "snowflake", cfg.recordatorioMin);

  // --- Calor (dispara en cfg.calorC, se da por superada 1.5 C abajo) ---
  static bool calorActivo = false;
  if (isnan(st.tempAire)) calorActivo = false;
  else if (st.tempAire >= cfg.calorC) calorActivo = true;
  else if (st.tempAire <= cfg.calorC - HISTERESIS_CALOR_C) calorActivo = false;
  gestionarAlerta(
      AL_CALOR, calorActivo,
      "CALOR EN EL INVERNADERO",
      "Aire en " + String(st.tempAire, 1) + " C. Ventilador " +
      (st.ventOn ? "encendido" : "APAGADO - revisalo") +
      ". Abri las cortinas laterales.",
      "Temperatura normalizada",
      isnan(st.tempAire) ? String("Se perdio la lectura del sensor de aire.")
                         : "Aire de vuelta en " + String(st.tempAire, 1) + " C.",
      "high", "hot_face", cfg.recordatorioMin);

  bool aguaFria     = !isnan(st.tempAgua) && st.tempAgua < cfg.aguaMinC;
  bool aguaCaliente = !isnan(st.tempAgua) && st.tempAgua > cfg.aguaMaxC;

  gestionarAlerta(
      AL_AGUA_FRIA, aguaFria,
      "Agua muy fria",
      "Nutriente a " + String(st.tempAgua, 1) + " C. Bajo " +
      String(cfg.aguaMinC, 0) + " C la lechuga casi no absorbe.",
      "Agua OK", "Nutriente en " + String(st.tempAgua, 1) + " C.",
      "default", "cold_face", cfg.recordatorioMin);

  gestionarAlerta(
      AL_AGUA_CALIENTE, aguaCaliente,
      "Agua muy caliente",
      "Nutriente a " + String(st.tempAgua, 1) + " C. Sobre " +
      String(cfg.aguaMaxC, 0) + " C cae el oxigeno disuelto (riesgo de pythium).",
      "Agua OK", "Nutriente en " + String(st.tempAgua, 1) + " C.",
      "default", "fire", cfg.recordatorioMin);

  gestionarAlerta(
      AL_SENSOR, (sensorCaido(tUltAgua) || sensorCaido(tUltAire)),
      "Sensor sin lectura",
      String("Mas de ") + cfg.sensorTimeoutMin + " min sin datos. DS18B20 (agua): " +
      (sensorCaido(tUltAgua) ? "SIN DATO" : "ok") +
      " | DHT22 (aire): " + (sensorCaido(tUltAire) ? "SIN DATO" : "ok") +
      ". Revisa el cableado.",
      "Sensores OK", "Todos los sensores volvieron a leer.",
      "default", "electric_plug", cfg.recordatorioMin);
}

// ============================== ESTADO EN JSON =======================
String textoModo() {
  struct tm t;
  if (!horaLocal(t)) return "SIN HORA / riego continuo";
  bool    noche = esNoche();
  uint8_t modo  = noche ? cfg.modoNoche : cfg.modoDia;
  char hora[8];
  snprintf(hora, sizeof(hora), "%02d:%02d", t.tm_hour, t.tm_min);
  String s = String(hora) + (noche ? " Noche / " : " Dia / ");
  if (modo == RIEGO_CONTINUO) s += "riego continuo";
  else s += String(noche ? cfg.nocheOnMin : cfg.diaOnMin) + " min ON / " +
            String(noche ? cfg.nocheOffMin : cfg.diaOffMin) + " min OFF";
  return s;
}

String uptime() {
  uint32_t s = millis() / 1000;
  char buf[32];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu", (unsigned long)(s / 86400),
           (unsigned long)((s % 86400) / 3600), (unsigned long)((s % 3600) / 60));
  return String(buf);
}

void ponerNum(JsonObject j, const char *clave, float v, float escala) {
  if (isnan(v)) j[clave] = nullptr;
  else          j[clave] = roundf(v * escala) / escala;
}

// Lo mismo va al panel local (/api/status) y a la nube.
void llenarEstado(JsonObject j) {
  bool alerta = false;
  for (uint8_t i = 0; i < AL_TOTAL; i++) if (alertas[i].activa) alerta = true;

  ponerNum(j, "temp_agua", st.tempAgua, 100);
  ponerNum(j, "temp_aire", st.tempAire, 100);
  ponerNum(j, "humedad",   st.humAire,  10);
  j["nivel_ok"]        = st.nivelOk;
  j["bomba_on"]        = st.bombaOn;
  j["bloqueo_nivel"]   = st.bloqueoNivel;
  j["alerta"]          = alerta;
  j["modo_manual"]     = (int)manBomba.modo;
  j["manual_min"]      = manualRestanteMin(manBomba);
  j["arranques"]       = st.arranquesBomba;
  j["vent_on"]         = st.ventOn;
  j["vent_manual"]     = (int)manVent.modo;
  j["vent_manual_min"] = manualRestanteMin(manVent);
  j["vent_hoy_s"]      = vent.hoySeg;
  j["vent_ayer_s"]     = vent.ayerSeg;
  j["vent_mes_s"]      = vent.mesSeg;
  j["vent_mesant_s"]   = vent.mesAntSeg;
  ponerNum(j, "vent_on_c",  cfg.ventOnC,  10);
  ponerNum(j, "vent_off_c", cfg.ventOffC, 10);
  j["ram"]             = ESP.getFreeHeap();
  j["tanque_l"]        = cfg.tanqueL;
  ponerNum(j, "dosis_a", cfg.dosisA, 100);
  ponerNum(j, "dosis_b", cfg.dosisB, 100);
  ponerNum(j, "dosis_c", cfg.dosisC, 100);
  j["modo"]            = textoModo();
  j["bomba_cambio"]    = horaProximoCambio();
  j["uptime"]          = uptime();
  j["ip"]              = WiFi.localIP().toString();
  j["rssi"]            = WiFi.isConnected() ? WiFi.RSSI() : 0;
  j["wifi_caidas"]     = wifiCaidas;
}

// ============================== HANDLERS =============================
void enviarJson(JsonDocument &doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", out);
}

void responderError(int code, const char *msg) {
  JsonDocument doc;
  doc["error"] = msg;
  enviarJson(doc, code);
}

// Archivos del panel (index.html, logo, iconos) desde web_files.h
bool servirArchivo(String ruta) {
  if (ruta == "/") ruta = "/index.html";
  for (size_t i = 0; i < WEB_FILES_N; i++) {
    if (ruta != WEB_FILES[i].ruta) continue;
    bool html = ruta.endsWith(".html");
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", html ? "no-cache" : "max-age=86400");
    server.send_P(200, WEB_FILES[i].mime, (const char *)WEB_FILES[i].datos, WEB_FILES[i].len);
    return true;
  }
  return false;
}

void handleNoEncontrado() {
  if (!servirArchivo(server.uri())) server.send(404, "text/plain", "no existe");
}

void handleStatus() {
  JsonDocument doc;
  JsonObject j = doc.to<JsonObject>();
  llenarEstado(j);
  j["fuente"] = "local";
  JsonObject n = j["nube"].to<JsonObject>();
  n["activa"] = strlen(NUBE_URL) > 0;
  n["ok"]     = nube.ok;
  n["hace_s"] = nube.tUltOk ? (millis() - nube.tUltOk) / 1000 : -1;
  enviarJson(doc);
}

// Manda una serie del historial en trozos, para no armar un String gigante.
void enviarSerie(const char *nombre, uint8_t campo, bool coma) {
  String s = "\"" + String(nombre) + "\":[";
  uint16_t inicio = (histLlenas < HIST_MUESTRAS) ? 0 : histIdx;
  for (uint16_t k = 0; k < histLlenas; k++) {
    uint16_t i = (inicio + k) % HIST_MUESTRAS;
    int16_t  v = (campo == 0) ? hist[i].agua : (campo == 1) ? hist[i].aire : hist[i].hum;
    if (k) s += ',';
    if (v == INT16_MIN) s += "null";
    else                s += String(v / 10.0f, 1);
    if (s.length() > 1000) { server.sendContent(s); s = ""; }
  }
  s += coma ? "]," : "]}";
  server.sendContent(s);
}

void handleHistory() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("{\"int_min\":" + String(HIST_INTERVALO_MIN) + ",");
  enviarSerie("agua", 0, true);
  enviarSerie("aire", 1, true);
  enviarSerie("hum",  2, false);
  server.sendContent("");
}

void handleConfig() {
  if (server.method() == HTTP_POST) {
    JsonDocument in;
    if (deserializeJson(in, server.arg("plain"))) { responderError(400, "json"); return; }
    if (cfgDesdeJson(in["cfg"])) {
      cfgVer = max(ahoraEpoch(), cfgVer + 1);
      guardarConfig();
      tNube = 0;                       // avisarle a la nube ya
      Serial.println("[config] cambiada desde el panel local");
    }
  }
  JsonDocument doc;
  cfgAJson(doc["cfg"].to<JsonObject>());
  doc["ver"] = cfgVer;
  enviarJson(doc);
}

void handleCmd() {
  JsonDocument in;
  if (deserializeJson(in, server.arg("plain"))) { responderError(400, "json"); return; }
  const char *d = in["dispositivo"] | "";
  const char *m = in["modo"] | "";
  if (!aplicarOrden(d, m)) { responderError(400, "orden"); return; }
  tNube = 0;
  JsonDocument doc;
  doc["ok"] = true;
  enviarJson(doc);
}

// ============================== NUBE (Netlify) =======================
// Le manda el estado al panel de Netlify y en la respuesta recibe la
// configuracion (si alguien la cambio afuera) y las ordenes pendientes.
// Sin TLS verificado (setInsecure): el certificado de Netlify cambia y un
// ESP32 que no se puede actualizar a mano no deberia quedar sin nube.
void sincronizarNube() {
  if (strlen(NUBE_URL) == 0 || WiFi.status() != WL_CONNECTED) return;

  JsonDocument out;
  llenarEstado(out["estado"].to<JsonObject>());
  cfgAJson(out["cfg"].to<JsonObject>());
  out["cfg_ver"]           = cfgVer;
  out["ack"]["bomba"]      = nube.ackBomba;
  out["ack"]["ventilador"] = nube.ackVent;
  String body;
  serializeJson(out, body);

  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  if (!http.begin(cli, NUBE_URL)) { nube.ok = false; return; }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-key", NUBE_CLAVE);
  int code = http.POST(body);
  nube.codigo = code;
  if (code != 200) {
    Serial.printf("[nube] HTTP %d\n", code);
    nube.ok = false;
    http.end();
    return;
  }
  String resp = http.getString();
  http.end();
  nube.ok     = true;
  nube.tUltOk = millis();

  JsonDocument in;
  if (deserializeJson(in, resp)) return;

  // Configuracion mas nueva hecha desde el panel en la nube
  uint32_t verNube = in["cfg_ver"] | 0;
  if (in["cfg"].is<JsonObject>() && verNube > cfgVer) {
    cfgDesdeJson(in["cfg"]);
    cfgVer = verNube;
    guardarConfig();
    Serial.printf("[nube] configuracion nueva (ver %lu)\n", (unsigned long)cfgVer);
  }

  // Ordenes Forzar ON/OFF/AUTO
  JsonObjectConst cmds = in["cmds"];
  for (JsonPairConst kv : cmds) {
    const char *disp = kv.key().c_str();
    uint32_t    id   = kv.value()["id"] | 0;
    const char *modo = kv.value()["modo"] | "";
    uint32_t   &ack  = !strcmp(disp, "bomba") ? nube.ackBomba : nube.ackVent;
    if (strcmp(disp, "bomba") && strcmp(disp, "ventilador")) continue;
    if (id > ack) {
      aplicarOrden(disp, modo);
      ack = id;
      tNube = millis() - (NUBE_INTERVALO_S - 3) * 1000UL;   // confirmar en ~3 s
    }
  }
}

// ============================== WIFI =================================
void conectarWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  // Sin ahorro de energia: con el WiFi dormido el ESP32 tarda segundos en
  // contestar al panel local y pierde la conexion con mas facilidad.
  WiFi.setSleep(false);
#ifdef IP_FIJA
  {
    IPAddress ip(IP_LOCAL), gw(IP_GATEWAY), mask(IP_MASCARA), dns(IP_DNS);
    if (!WiFi.config(ip, gw, mask, dns))
      Serial.println("[wifi] no se pudo fijar la IP, sigo con DHCP");
    else
      Serial.println("[wifi] IP fija configurada");
  }
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("[wifi] conectando");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(400); Serial.print("."); esp_task_wdt_reset();
  }
  Serial.println();

  st.wifiOk = (WiFi.status() == WL_CONNECTED);
  if (st.wifiOk) {
    ultimaIp = WiFi.localIP().toString();
    Serial.printf("[wifi] OK  IP: %s\n", ultimaIp.c_str());
    if (MDNS.begin(HOSTNAME)) Serial.printf("[wifi] http://%s.local\n", HOSTNAME);
  } else {
    Serial.println("[wifi] SIN CONEXION - sigue midiendo local (sin alertas)");
  }
}

// ============================== SETUP / LOOP =========================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n=== VERDALIA - monitor NFT ===");

  // La bomba arranca PRENDIDA: con el rele en NC eso es dejarlo en reposo,
  // y despues controlBomba() aplica el ciclo apenas haya hora.
  pinMode(PIN_SSR, OUTPUT);
  escribirReleBomba(true);
  st.bombaOn = true;
  pinMode(PIN_VENTILADOR, OUTPUT);
  digitalWrite(PIN_VENTILADOR, VENT_ACTIVO_ALTO ? LOW : HIGH);   // arranca APAGADO
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_FLOAT, INPUT_PULLUP);

  cargarConfig();

  // Contador de horas del ventilador guardado antes del ultimo apagon
  prefs.begin("vent", false);
  if (prefs.getBytesLength("cnt") == sizeof(vent))
    prefs.getBytes("cnt", &vent, sizeof(vent));

  for (uint16_t i = 0; i < HIST_MUESTRAS; i++)
    hist[i] = { INT16_MIN, INT16_MIN, INT16_MIN };

  ds.begin();
  ds.setResolution(12);
  ds.setWaitForConversion(false);
  dht.begin();

#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdtCfg = { .timeout_ms = WDT_TIMEOUT_S * 1000,
                                   .idle_core_mask = 0, .trigger_panic = true };
  esp_task_wdt_reconfigure(&wdtCfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);

  conectarWifi();
  iniciarHora();   // siempre, aunque el WiFi no haya levantado todavia

  server.on("/api/status",  HTTP_GET, handleStatus);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/config",  handleConfig);
  server.on("/api/cmd",     HTTP_POST, handleCmd);
  server.onNotFound(handleNoEncontrado);
  server.begin();
  Serial.println("[web] servidor arriba en el puerto 80");

  leerFlotador();
  leerAire();

  enviarNtfy("Verdalia: monitor encendido",
             "El ESP32 arranco y esta midiendo. Panel local: http://" +
             WiFi.localIP().toString(),
             "low", "seedling");
  enviarLatido();   // primer latido apenas arranca, para que el check quede verde
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  leerAgua();

  static uint32_t tSensores = 0, tSerial = 0, tWifi = 0, tLed = 0, tHist = 0, tLatido = 0,
                  tVentGuardar = 0;
  uint32_t ahora = millis();

  if (ahora - tSensores >= INTERVALO_SENSORES_MS) {
    tSensores = ahora;
    leerAire();
    leerFlotador();
    controlBomba();
    contarVentilador();
    controlVentilador();
    evaluarAlertas();
  }

  if (ahora - tVentGuardar >= (uint32_t)VENT_GUARDAR_MIN * 60000UL) {
    tVentGuardar = ahora;
    guardarVentilador();
  }

  // Historial: la primera muestra a los 10 s, despues cada HIST_INTERVALO_MIN
  if (tHist == 0 ? (ahora > 10000)
                 : (ahora - tHist >= (uint32_t)HIST_INTERVALO_MIN * 60000UL)) {
    tHist = ahora;
    guardarMuestra();
  }

  // Nube: la primera a los 5 s del arranque, despues cada NUBE_INTERVALO_S
  if (tNube == 0 ? (ahora > 5000)
                 : (ahora - tNube >= (uint32_t)NUBE_INTERVALO_S * 1000UL)) {
    tNube = ahora;
    sincronizarNube();
  }

  if (ahora - tSerial >= INTERVALO_SERIAL_MS) {
    tSerial = ahora;
    Serial.printf("agua=%.1fC  aire=%.1fC  hum=%.0f%%  nivel=%s  bomba=%s  vent=%s  nube=%s\n",
                  st.tempAgua, st.tempAire, st.humAire,
                  st.nivelOk ? "OK" : "BAJO", st.bombaOn ? "ON" : "OFF",
                  st.ventOn ? "ON" : "OFF", nube.ok ? "ok" : "--");
  }

  // Latido cada LATIDO_MIN minutos
  if (ahora - tLatido >= (uint32_t)LATIDO_MIN * 60000UL) {
    tLatido = ahora;
    enviarLatido();
  }

  if (ahora - tWifi >= 30000) {
    tWifi = ahora;
    static uint8_t fallos = 0;
    bool ok = (WiFi.status() == WL_CONNECTED);
    if (!ok) {
      if (st.wifiOk) wifiCaidas++;
      fallos++;
      Serial.printf("[wifi] caido, reintentando (%u)\n", fallos);
      if (fallos % 4 == 0) {
        // reconnect() a veces queda trabado; cada 2 min se arranca de cero
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
      } else {
        WiFi.reconnect();
      }
    } else if (!st.wifiOk) {
      fallos = 0;
      // Acaba de volver el WiFi. Hay que relevantar mDNS (se cae con la
      // conexion) y avisar si el router le dio una IP distinta, porque si no
      // el panel local queda inalcanzable sin que te enteres.
      Serial.println("[wifi] reconectado");
      MDNS.end();
      MDNS.begin(HOSTNAME);
      struct tm t;
      if (!horaLocal(t)) iniciarHora();   // por si arranco sin internet
      String ipNueva = WiFi.localIP().toString();
      if (ipNueva != ultimaIp) {
        Serial.printf("[wifi] IP cambio: %s -> %s\n", ultimaIp.c_str(), ipNueva.c_str());
        enviarNtfy("Verdalia: el monitor cambio de IP",
                   "Antes " + ultimaIp + ", ahora http://" + ipNueva +
                   " . Si usas la IP directa, actualiza el favorito.",
                   "high", "globe_with_meridians");
        ultimaIp = ipNueva;
      }
    }
    st.wifiOk = ok;
  }

  bool hayAlerta = !st.nivelOk || alertas[AL_HELADA].activa || alertas[AL_CALOR].activa ||
                   alertas[AL_SENSOR].activa;
  if (ahora - tLed >= (uint32_t)(hayAlerta ? 200 : 2000)) {
    tLed = ahora;
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
  }
}
