# SmartMod

[![PlatformIO Registry](https://badges.registry.platformio.org/packages/pujolbello/library/SmartMod.svg)](https://registry.platformio.org/libraries/pujolbello/SmartMod)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

Cliente modular para dispositivos **ESP32** de la plataforma [SmartMod](https://smartmod.app). Resuelve de fábrica la conectividad WiFi + MQTT, el provisioning mediante portal cautivo, la persistencia en NVS y un modelo de **capabilities** componibles para añadir telemetría y control sin reescribir el ciclo de vida del dispositivo.

## Estado

Fase **F1 + F2 + F3** del plan de evolución:

- Extracción del sketch original a clases reutilizables.
- Núcleo no bloqueante (sin `while` ni `delay()` en la conexión).
- Backoff exponencial WiFi/MQTT.
- Buffer MQTT configurable (por defecto 8 KB, suficiente para payloads de medición).
- Last Will Testament + estado online retenido en `.../output/status/json`.
- Router por `(category, format)` y capabilities desacopladas.
- Persistencia NVS + portal cautivo de provisioning con logo embebido.

## Tabla de contenidos

- [Instalación](#instalación)
- [Quick start](#quick-start)
- [Arquitectura](#arquitectura)
- [Configuración (`Config`)](#configuración-config)
- [Provisioning y persistencia](#provisioning-y-persistencia)
- [Contrato de tópicos MQTT](#contrato-de-tópicos-mqtt)
- [API de la fachada `SmartMod`](#api-de-la-fachada-smartmod)
- [Capabilities incluidas](#capabilities-incluidas)
  - [`GpioCapability`](#gpiocapability)
  - [`MeasurementCapability`](#measurementcapability)
- [Crear tus propias capabilities](#crear-tus-propias-capabilities)
- [LWT y estado online](#lwt-y-estado-online)
- [Operación: factory reset y forzar portal](#operación-factory-reset-y-forzar-portal)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [Licencia](#licencia)

---

## Instalación

### PlatformIO (recomendado)

`platformio.ini`:

```ini
[env:esp32doit-devkit-v1]
platform   = espressif32
board      = esp32doit-devkit-v1
framework  = arduino
monitor_speed = 115200

build_unflags = -std=gnu++11
build_flags   = -std=gnu++17

lib_deps =
  pujolbello/SmartMod@^0.1.0
```

> **Importante**: la librería usa `std::initializer_list` y constructores agregados → necesita C++17. Si tu proyecto ya está en C++17 puedes omitir los `build_*flags`.

Dependencias resueltas automáticamente: `PubSubClient ^2.8`, `ArduinoJson ^7.2.1`.

### Vía Git (sin registro)

```ini
lib_deps = https://github.com/pujolbello/smartmod-device-client.git#0.1.0
```

---

## Quick start

```cpp
#include <Arduino.h>
#include <SmartMod.h>
#include <capabilities/GpioCapability.h>
#include <capabilities/MeasurementCapability.h>

using namespace smartmod;

SmartMod mod;
GpioCapability gpio;
MeasurementCapability meas;

void setup() {
  Serial.begin(115200);

  // Defaults: solo se aplican si NVS está vacío. El portal los sobreescribe.
  auto& cfg = mod.config();
  cfg.mqttHost     = "api.smartmod.app";
  cfg.organization = "tu-org";
  cfg.user         = "tu-usuario";

  mod.use(gpio);
  mod.use(meas);

  gpio.configure({
      PinConfig{4,  PinDir::InputPullup},
      PinConfig{2,  PinDir::Output, 1},
  });

  meas.setIntervalMs(1000);
  meas.setProducer([](JsonArray fields, JsonArray) {
    JsonObject f = fields.add<JsonObject>();
    f["temp"] = random(170, 260) / 10.0f;
  });

  mod.begin();
}

void loop() { mod.loop(); }
```

En el primer arranque (NVS vacío) el dispositivo levanta el AP `SmartMod-XXXXXX` y sirve el formulario de provisioning en `http://192.168.4.1`.

Ejemplo completo en [`examples/Basic/Basic.ino`](examples/Basic/Basic.ino).

---

## Arquitectura

```
┌──────────────┐
│   Sketch     │  define defaults + registra capabilities
└──────┬───────┘
       │ usa
┌──────▼───────┐    ┌───────────┐    ┌────────────┐
│   SmartMod   ├────►   Topics   ◄────┤   Config   │
│  (facade)    │    └───────────┘    └────────────┘
│              │
│  use(cap)    │    ┌────────────────┐
│  begin()     ├────►   Transport    │  WiFi + PubSubClient
│  loop()      │    │ (no bloqueante)│  LWT + backoff
│              │    └───────┬────────┘
│              │            │ rx
│              │    ┌───────▼────────┐
│              ├────►    Router      │  on(category,format)
│              │    └───────┬────────┘
│              │            │
│              │    ┌───────▼────────┐
│              ├────►  Capabilities  │  GPIO, Measurement, …
│              │    └────────────────┘
│              │
│              │    ┌────────────────┐
│              ├────►     Store      │  NVS / Preferences
│              │    └────────────────┘
│              │    ┌────────────────┐
│              └────►    Portal      │  AP + DNS + WebServer
└──────────────┘    └────────────────┘
```

Ficheros:

```
src/
├── SmartMod.h / .cpp            Fachada y orquestador
├── SmartModConfig.h             POD con la configuración
├── SmartModTopics.h / .cpp      Builder/parser de tópicos versionados
├── SmartModTransport.h / .cpp   WiFi + PubSubClient no bloqueante
├── SmartModRouter.h / .cpp      Dispatch por (category, format)
├── SmartModStore.h / .cpp       Persistencia NVS (Preferences)
├── SmartModPortal.h / .cpp      Portal cautivo de provisioning
├── assets/
│   └── logo_svg.h               Logo SVG en PROGMEM
└── capabilities/
    ├── ICapability.h
    ├── GpioCapability.h / .cpp
    └── MeasurementCapability.h / .cpp
```

---

## Configuración (`Config`)

`mod.config()` devuelve una referencia mutable a este POD:

| Campo               | Tipo       | Default                | Notas |
|---------------------|------------|------------------------|-------|
| `wifiSsid`          | `String`   | `""`                   | Vacío ⇒ se abre el portal |
| `wifiPassword`      | `String`   | `""`                   |  |
| `mqttHost`          | `String`   | `""`                   | Requerido |
| `mqttPort`          | `uint16_t` | `1883`                 |  |
| `mqttUser`          | `String`   | `""`                   | Opcional |
| `mqttPassword`      | `String`   | `""`                   | Opcional |
| `workspace`         | `String`   | `"workspace"`          | Raíz del namespace |
| `organization`      | `String`   | `""`                   | Requerido |
| `user`              | `String`   | `""`                   | Requerido |
| `protocolVersion`   | `uint8_t`  | `1`                    | `v1`, `v2`, … en los tópicos |
| `deviceId`          | `String`   | autogenerado MAC       | `"ESP32-AABBCC"` si vacío |
| `mqttBufferSize`    | `uint16_t` | `8192`                 | Subir si tus payloads son grandes |
| `keepAliveSeconds`  | `uint16_t` | `30`                   |  |
| `reconnectMinMs`    | `uint32_t` | `1000`                 | Backoff inicial |
| `reconnectMaxMs`    | `uint32_t` | `30000`                | Backoff máximo |

**Orden de precedencia** al arrancar:

1. Defaults compilados (lo que pones en `mod.config()`).
2. NVS (sobrescribe lo cargado si existe configuración guardada).
3. Portal cautivo (si NVS está vacío o `requestProvisioning()` fue invocado).

---

## Provisioning y persistencia

`SmartMod::begin()` aplica esta lógica:

1. Carga NVS con `Store::load(config)`.
2. Si `forceProvisioning_` está activo **o** falta configuración mínima (`wifiSsid`, `mqttHost`, `organization`, `user`), levanta el portal cautivo bloqueante.
3. Tras guardar el formulario, persiste en NVS y reinicia.
4. En arranque normal procede directo a conectarse.

**Datos del AP**:
- SSID: `SmartMod-XXXXXX` (últimos 6 hex de la MAC).
- IP: `192.168.4.1`.
- DNS captive: cualquier dominio se redirige al portal.
- Rutas: `/` (form), `/save` (POST), `/logo.svg` (estático), endpoints de detección de captive de Android / iOS / macOS.

---

## Contrato de tópicos MQTT

```
workspace/{org}/{user}/{deviceId}/v{N}/{direction}/{category}/{format}
```

- `direction` ∈ `input` (al dispositivo) | `output` (del dispositivo).
- El dispositivo se **suscribe** a `.../v{N}/input/#`.
- Bumpeando `protocolVersion` puedes evolucionar el contrato sin romper dispositivos anteriores.

Ejemplos (con `org=tecnoconciencia`, `user=pujolbello`, `deviceId=ESP32-AABBCC`, `v1`):

| Dirección | Categoría     | Formato | Tópico |
|-----------|---------------|---------|--------|
| output    | `status`      | `json`  | `workspace/tecnoconciencia/pujolbello/ESP32-AABBCC/v1/output/status/json` |
| output    | `gpio`        | `json`  | `…/v1/output/gpio/json` |
| output    | `measurement` | `json`  | `…/v1/output/measurement/json` |
| input     | `gpio`        | `json`  | `…/v1/input/gpio/json` |

---

## API de la fachada `SmartMod`

```cpp
Config&     config();
Topics&     topics();
Router&     router();
Transport&  transport();

void use(ICapability& cap);     // registra y se ataca en begin()
void begin();                   // arranca todo (puede ejecutar portal)
void loop();                    // llamar en cada iteración de loop()
bool connected() const;

bool publishOutput(const String& category,
                   const String& format,
                   const String& payload,
                   bool retained = false);

void requestProvisioning();     // fuerza portal en el próximo begin
void factoryReset();            // borra NVS y reinicia

static String defaultDeviceId(); // "ESP32-AABBCC" desde MAC
```

### Publicar desde tu sketch

```cpp
mod.publishOutput("event", "json", "{\"type\":\"button\",\"id\":1}");
```

### Registrar handlers ad-hoc

```cpp
mod.router().on("custom", "json",
  [](const String& topic, const uint8_t* payload, size_t len) {
    Serial.printf("[custom] %u bytes en %s\n", (unsigned)len, topic.c_str());
  });
```

---

## Capabilities incluidas

### `GpioCapability`

Publica cambios de pines de entrada (poll cada `setPollIntervalMs`, default 100 ms) y acepta comandos para escribir salidas.

**Configuración**:
```cpp
gpio.configure({
    PinConfig{4,  PinDir::InputPullup},
    PinConfig{23, PinDir::Input},
    PinConfig{2,  PinDir::Output, 1},  // initialValue = 1
});
gpio.onChange([](uint8_t pin, int value) { /* hook local */ });
gpio.setPollIntervalMs(50);
```

**Publica** en `output/gpio/json`:
```json
[{"pin":4,"_value":1,"mode":"input","type":"digital"}]
```

**Acepta** en `input/gpio/json`:
```json
[{"pin":2,"_value":0,"mode":"output","type":"digital"}]
```

(Soporta también la variante donde el backend envuelve el array como string escapado.)

### `MeasurementCapability`

Telemetría periódica. Tú aportas un *productor* que rellena `fields` y `tags`; la capability se encarga del temporizado y la serialización.

```cpp
meas.setIntervalMs(1000);
meas.setProducer([](JsonArray fields, JsonArray tags) {
  JsonObject t = fields.add<JsonObject>();
  t["temp"] = 21.4f;

  JsonObject tag = tags.add<JsonObject>();
  tag["room"] = "lab";
});

// Publicación inmediata fuera del temporizador:
meas.publishNow();
```

Publica en `output/measurement/json`:
```json
{ "fields": [ {"temp": 21.4} ], "tags": [ {"room": "lab"} ] }
```

---

## Crear tus propias capabilities

Implementa la interfaz `ICapability`:

```cpp
#include <capabilities/ICapability.h>
#include <SmartMod.h>

class HeartbeatCapability : public smartmod::ICapability {
 public:
  const char* name() const override { return "heartbeat"; }

  void attach(smartmod::SmartMod& core) override {
    core_ = &core;
    // Suscribir handlers de entrada:
    core.router().on("heartbeat", "json",
      [this](const String&, const uint8_t* p, size_t n) {
        Serial.printf("ping %.*s\n", (int)n, p);
      });
  }

  void loop() override {
    if (millis() - last_ < 5000) return;
    last_ = millis();
    core_->publishOutput("heartbeat", "json", "{\"ts\":1}");
  }

 private:
  smartmod::SmartMod* core_ = nullptr;
  uint32_t last_ = 0;
};
```

Registro en el sketch:
```cpp
HeartbeatCapability hb;
mod.use(hb);
```

---

## LWT y estado online

Al conectar publica retenido en `output/status/json`:
```json
{"online":true}
```

Configurado como Last Will Testament:
```json
{"online":false}
```

Esto permite a la plataforma detectar desconexiones bruscas sin esperar al timeout MQTT.

---

## Operación: factory reset y forzar portal

```cpp
// Botón mantenido al arrancar -> abrir portal aunque haya config válida
pinMode(0, INPUT_PULLUP);
delay(50);
if (digitalRead(0) == LOW) mod.requestProvisioning();

// Botón mantenido durante operación normal -> wipe + reinicio
if (digitalRead(0) == LOW && millis() > 30000) mod.factoryReset();
```

---

## Troubleshooting

| Síntoma | Causa probable | Solución |
|--------|---------------|----------|
| `WiFi error 0x3007 sta is connecting` | Llamadas repetidas a `WiFi.begin()` | Ya mitigado; comprueba que estás en la última versión |
| MQTT no publica payloads grandes | Buffer demasiado pequeño | Sube `cfg.mqttBufferSize` |
| El portal se abre siempre | NVS no persiste algún campo requerido | Verifica que el formulario incluye `wifiSsid`, `mqttHost`, `organization`, `user` |
| Compila pero error de `initializer_list` | Falta C++17 | Añade `build_unflags=-std=gnu++11` y `build_flags=-std=gnu++17` |
| Mensaje no llega a la capability | `category` / `format` no registrados | Verifica con `mod.router().on(...)` o el nombre que publica tu capability |

---

## Roadmap

- **F4**: PWM / ADC en `GpioCapability`, batching y compresión de mediciones.
- **F5**: Router declarativo con versiones coexistentes (`v1`, `v2`).
- **F6**: OTA por MQTT (descarga HTTPS topic-driven), Shadow (desired/reported retenido), provisioning HTTPS contra el backend SmartMod.

---

## Licencia

[Apache-2.0](LICENSE) © Tecnoconciencia
