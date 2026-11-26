/* =====================================================================
 *  config.h - VERDALIA, monitor del NFT de lechuga (Cuenca, Ecuador)
 *
 *  Aca va lo que NO se puede cambiar desde el panel: WiFi, pines, nube,
 *  polaridad de los reles. Los tiempos de riego, umbrales y dosis se
 *  cambian desde el engranaje del panel y quedan guardados en el ESP32;
 *  los valores de las secciones 3b, 4, 5 y 5b son solo los de FABRICA
 *  (los que usa la primera vez).
 * ===================================================================== */
#pragma once

// ---------------------------------------------------------------------
//  1. WiFi
// ---------------------------------------------------------------------
#define WIFI_SSID           "TU_RED_WIFI"
#define WIFI_PASS           "TU_CLAVE_WIFI"
#define HOSTNAME            "verdalia"        // http://verdalia.local

// --- IP fija (opcional pero MUY recomendado) ---------------------------
// Con DHCP el router le puede cambiar la IP en cada reinicio y perdes el
// dashboard sin darte cuenta. Para fijarla:
//   1. Averigua la IP actual del ESP32 (monitor serial o lista del router).
//   2. Averigua la IP del router (gateway). Casi siempre 192.168.1.1
//      o 192.168.0.1 - mira la que ya usa tu celular.
//   3. Elegi una IP del mismo rango pero ALTA, fuera del pool DHCP
//      (ej: si el router es 192.168.1.1, usa 192.168.1.200).
//   4. Descomenta IP_FIJA y completa los numeros separados por COMAS.
//
//   Router de la casa: 192.168.0.1. Su DHCP reparte desde .100, por eso
//   el ESP32 va en .200 (verificado libre el 2026-10-23).
#define IP_FIJA
#ifdef IP_FIJA
  #define IP_LOCAL     192,168,0,200
  #define IP_GATEWAY   192,168,0,1
  #define IP_MASCARA   255,255,255,0
  #define IP_DNS       8,8,8,8
#endif

// ---------------------------------------------------------------------
//  2. Alertas al celular (ntfy.sh - gratis, sin cuenta)
//     Instala la app "ntfy" y suscribite al MISMO topic de abajo.
//     IMPORTANTE: el topic es publico. Usa un nombre largo y raro,
//     tipo "nft-quingeo-x7k92m". Cualquiera que lo adivine ve tus datos.
// ---------------------------------------------------------------------
#define NTFY_TOPIC          "tu-topic-largo-y-raro"
#define NTFY_URL            "http://ntfy.sh/" NTFY_TOPIC
#define ALERTA_RECORDATORIO_MIN  30   // fabrica: reenvia cada 30 min si sigue el problema
#define RECORDATORIO_NIVEL_MIN   10   // fabrica: el tanque bajo insiste cada 10 min

// ---------------------------------------------------------------------
//  2a. Panel en la nube (Netlify)
//
//  El ESP32 le manda su estado al panel cada NUBE_INTERVALO_S y de paso
//  recoge los cambios de configuracion y los botones que tocaste afuera.
//  NUBE_CLAVE tiene que ser IGUAL a la variable DEVICE_KEY del sitio en
//  Netlify. Deja NUBE_URL vacio ("") para no usar la nube.
// ---------------------------------------------------------------------
#define NUBE_URL ""   // ej: ""
#define NUBE_CLAVE          "IGUAL_A_DEVICE_KEY_EN_NETLIFY"
#define NUBE_INTERVALO_S    30

// ---------------------------------------------------------------------
//  2b. Latido a healthchecks.io  (avisa cuando el ESP32 SE MUERE)
//
//  ntfy solo sirve mientras el ESP32 este vivo: un aparato muerto no manda
//  notificaciones. healthchecks.io hace lo contrario: espera un ping cada
//  tanto y te avisa cuando DEJA de llegar. Tapa ese punto ciego.
//
//  Como configurarlo (gratis, 3 minutos):
//    1. Entra a https://healthchecks.io y crea una cuenta.
//    2. "Add Check". Ponele nombre "Monitor NFT Quingeo".
//    3. Period: 20 minutes   |   Grace Time: 20 minutes
//       (asi te avisa recien tras ~40 min de silencio, sin falsas alarmas)
//    4. Copia el UUID de la URL de ping (lo que va despues de /ping/)
//       y pegalo abajo.
//    5. En Integrations activa el email, o Push si usas su app.
//
//  Dejalo vacio ("") para desactivar el latido.
// ---------------------------------------------------------------------
#define HEALTHCHECK_UUID    "TU_UUID_DE_HEALTHCHECKS"
#define LATIDO_MIN          15

// ---------------------------------------------------------------------
//  3. Pines (ESP32 WROOM-32)
// ---------------------------------------------------------------------
#define PIN_DS18B20         4     // temperatura del agua (tanque)
#define PIN_DHT22           16    // temperatura + humedad del aire
#define PIN_FLOAT           27    // flotador de nivel (usa pull-up interno)
#define PIN_SSR             26    // control del rele de la bomba
#define PIN_VENTILADOR      25    // control del rele del ventilador (G3MB-202P)
#define PIN_LED             2     // LED onboard

// El flotador puede venir NO o NC y se puede montar "cabeza arriba" o
// "cabeza abajo". Si el sistema te dice TANQUE BAJO con el tanque lleno,
// cambia este false por true.
#define FLOAT_INVERTIR      false
#define FLOAT_DEBOUNCE_MS   5000UL    // el agua chapotea: exige 5 s estables

// El flotador esta montado un poco por DEBAJO DE LA MITAD del tanque, o sea
// que es un aviso de "rellena pronto", NO una proteccion de marcha en seco:
// cuando dispara, todavia quedan ~90 L y la bomba sigue bien sumergida.
// Por eso NO corta la bomba, solo avisa (e insiste cada 10 min).
//
// Ponelo en true SOLO si algun dia bajas el flotador cerca del fondo
// (~15 cm), que ahi si seria proteccion real de la bomba.
#define CORTE_BOMBA_POR_NIVEL   false

// Rele mecanico de la bomba (modulo de 1 canal, 10 A).
//
// La bomba va en el contacto NC (normalmente cerrado): con el rele en
// reposo la bomba ANDA, y el ESP32 lo activa para APAGARLA. Asi, si se
// muere el ESP32, el cargador o un cable, la bomba queda prendida en
// continuo en vez de dejar las raices secas.
#define BOMBA_EN_NC         false   // G3MB-202P (SSR): no tiene NC

// Los modulos azules de 1 canal casi siempre se activan con IN en BAJO.
// Prueba: en el dashboard toca "Forzar OFF". Si la bomba en vez de parar
// sigue andando (y con "Forzar ON" se para), cambia a true.
#define BOMBA_ACTIVO_ALTO   true    // el G3MB dispara con IN en ALTO

// Lo mismo para el modulo del ventilador. Si el ventilador arranca solo al
// energizar el ESP32 (modulo de disparo en BAJO), cambia a false.
#define VENT_ACTIVO_ALTO    true

// ---------------------------------------------------------------------
//  3b. Ventilador (rele G3MB-202P en PIN_VENTILADOR) - valores de FABRICA
//      Prende cuando el AIRE llega a VENT_ON_C y recien apaga cuando baja
//      a VENT_OFF_C. Esos 2 C de diferencia evitan que prenda y apague a
//      cada rato cuando la temperatura ronda el umbral.
//
//      Si el DHT22 se queda sin lectura, el ventilador queda prendido solo
//      de VENT_SIN_SENSOR_DESDE a VENT_SIN_SENSOR_HASTA (las horas de mas
//      sol) y apagado el resto del dia.
//
//      OJO: el G3MB-202P solo conmuta corriente ALTERNA (120 V), max 2 A.
//      Con un ventilador de 12 V DC no sirve: prende y no apaga nunca mas.
// ---------------------------------------------------------------------
#define VENT_ON_C           27.0f   // prende al llegar a esto
#define VENT_OFF_C          25.0f   // apaga al bajar a esto
#define VENT_SIN_SENSOR_DESDE  11   // sin sensor: prende a las 11:00
#define VENT_SIN_SENSOR_HASTA  14   // sin sensor: apaga a las 14:00
#define VENT_GUARDAR_MIN   10      // cada cuanto graba el contador de horas en flash

// ---------------------------------------------------------------------
//  4. Modo de riego - valores de FABRICA (se cambian desde el panel)
//     RIEGO_CONTINUO = bomba prendida siempre
//     RIEGO_PULSADO  = ciclo ON/OFF en minutos, pegado al reloj:
//       dia   15/15 -> prende a las :00 y a las :30
//       noche 15/45 -> prende a las :00 de cada hora
//     Si el ESP32 todavia no tiene la hora de internet, riega en continuo.
// ---------------------------------------------------------------------
#define RIEGO_CONTINUO      0
#define RIEGO_PULSADO       1

#define MODO_RIEGO_DIA      RIEGO_PULSADO
#define MODO_RIEGO_NOCHE    RIEGO_PULSADO

#define DIA_ON_MIN          15      // de dia: 15 min prendida...
#define DIA_OFF_MIN         15      // ...y 15 apagada (con calor la raiz se seca rapido)
#define NOCHE_ON_MIN        15      // de noche: 15 min prendida...
#define NOCHE_OFF_MIN       45      // ...y 45 apagada (para no enfriar el agua)

#define HORA_INICIO_DIA     6       // 06:00 empieza el "dia"
#define HORA_INICIO_NOCHE   18      // 18:00 empieza la "noche"

// ---------------------------------------------------------------------
//  5. Umbrales de alerta - valores de FABRICA (se cambian desde el panel)
// ---------------------------------------------------------------------
#define UMBRAL_HELADA_C     3.0f    // alerta si el aire baja de esto
#define HISTERESIS_HELADA_C 2.0f    // se considera superada a 3.0 + 2.0 = 5.0 C

#define AIRE_MAX_C          30.0f   // alerta de calor si el aire llega a esto
#define HISTERESIS_CALOR_C  1.5f    // se considera superada a 30.0 - 1.5 = 28.5 C

#define AGUA_MIN_C          8.0f    // raiz muy fria: crecimiento se frena
#define AGUA_MAX_C          26.0f   // raiz muy caliente: poco oxigeno, riesgo pythium

#define NIVEL_REARME_SEG    60      // el nivel debe estar OK 60 s seguidos para rearmar la bomba

// Un sensor que falla UNA lectura (pasa cada tanto, sobre todo el DS18B20)
// no es noticia: se sigue usando el ultimo valor bueno. Recien si pasa este
// tiempo sin ninguna lectura valida se da por caido y llega la alerta.
#define SENSOR_TIMEOUT_MIN  10

// ---------------------------------------------------------------------
//  5b. Solucion nutritiva (calculadora del panel) - valores de FABRICA
//
//  Dosis en ml por LITRO de agua. Estos valores salen de la app que te
//  paso Aqua Payana, donde para 200 L marcaba A=1020, B=540, C=540 ml:
//      A: 1020 / 200 = 5.10 ml/L
//      B:  540 / 200 = 2.70 ml/L
//      C:  540 / 200 = 2.70 ml/L
//
//  OJO: confirmalos contra la etiqueta del concentrado que compres.
//  Si cambias de marca cambian las dosis y hay que tocar estos numeros.
// ---------------------------------------------------------------------
#define TANQUE_LITROS       200
#define DOSIS_A_ML_L        5.10f
#define DOSIS_B_ML_L        2.70f
#define DOSIS_C_ML_L        2.70f

#define MANUAL_MIN          30      // cuanto dura un "Forzar ON/OFF" antes de volver a AUTO

// ---------------------------------------------------------------------
//  5c. Historial para los graficos de 24 h
//      288 muestras x 1 cada 5 min = 24 h exactas. Cuesta 1.7 KB de RAM.
// ---------------------------------------------------------------------
#define HIST_MUESTRAS       288
#define HIST_INTERVALO_MIN  5

// ---------------------------------------------------------------------
//  6. Tiempos internos
// ---------------------------------------------------------------------
#define INTERVALO_SENSORES_MS   5000UL     // lectura cada 5 s
#define INTERVALO_SERIAL_MS     10000UL    // log por serial cada 10 s
#define WDT_TIMEOUT_S           30         // watchdog: reinicia si se cuelga

// ---------------------------------------------------------------------
//  7. Hora local (Ecuador = UTC-5, sin horario de verano)
// ---------------------------------------------------------------------
#define GMT_OFFSET_SEG      (-5 * 3600)
#define DST_OFFSET_SEG      0
