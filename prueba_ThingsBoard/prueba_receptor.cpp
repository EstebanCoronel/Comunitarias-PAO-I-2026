// ============================================================
// RECEPTOR LoRa - HELTEC WiFi LoRa V3
//
// FORMATO ÚNICO:
//
// NIVEL,TEMPERATURA,EXTRACTOR,CAUDAL,PRESION
//
// Ejemplo:
// 112,36.4,0,42.7,3.25
//
// THINGSPEAK:
//
// Field 1 = Nivel
// Field 2 = Caudal
// Field 3 = Temperatura
// Field 4 = Presión
// Field 5 = Extractor
// ============================================================

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ============================================================
// OLED
// ============================================================

#ifdef WIRELESS_STICK_V3

static SSD1306Wire display(
  0x3c,
  500000,
  SDA_OLED,
  SCL_OLED,
  GEOMETRY_64_32,
  RST_OLED
);

#else

static SSD1306Wire display(
  0x3c,
  500000,
  SDA_OLED,
  SCL_OLED,
  GEOMETRY_128_64,
  RST_OLED
);

#endif

#define BOTON_PIN 0
#define LED_PIN   35

bool oledEncendida = true;

unsigned long ultimaActividadOLED = 0;

const unsigned long TIEMPO_OLED = 30000;

bool ultimoEstadoBoton = HIGH;

unsigned long ultimoCambioPagina = 0;

const unsigned long INTERVALO_PAGINA = 3000;

byte paginaOLED = 0;


// ============================================================
// LoRa
// ============================================================

#define RF_FREQUENCY           915000000
#define LORA_BANDWIDTH        0
#define LORA_SPREADING_FACTOR 11
#define LORA_CODINGRATE       1
#define LORA_PREAMBLE_LENGTH  8
#define LORA_SYMBOL_TIMEOUT   0

#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON  false

#define BUFFER_SIZE 64

char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;

bool lora_idle = true;
bool paqueteRecibido = false;

int16_t rssi;
int16_t rxSize;


// ============================================================
// VARIABLES RECIBIDAS
// ============================================================

int altura = 0;

float temperatura = 0.0;

int estadoExtractor = 0;

float caudal = 0.0;

float presion = 0.0;


// ============================================================
// WIFI
// ============================================================

const char* ssid = "JUNTA DE AGUA";
const char* password = "juntadeagua-2325";

bool estadoWiFi = false;

unsigned long ultimoIntentoWiFi = 0;

const unsigned long INTERVALO_WIFI = 10000;


// ============================================================
// API HTTP REST - VERCEL
// ============================================================

const char* serverUrl = "https://proyecto-comunitarias.vercel.app/api/registro";


// ============================================================
// MQTT THINGSPEAK
// ============================================================

const char* mqttServer = "mqtt3.thingspeak.com";

const int mqttPort = 1883;

// NUEVAS CREDENCIALES THINGSPEAK

const char* mqttClientID =
  "AzknMikDJhQMDBIDNyIJByk";

const char* mqttUsername =
  "AzknMikDJhQMDBIDNyIJByk";

const char* mqttPassword =
  "ngh8jpeiE4g4HoyufCZkmmVl";


// ============================================================
// NUEVO CANAL THINGSPEAK
// ============================================================

#define THINGSPEAK_CHANNEL 3457085

const char* mqttTopicMulti =
  "channels/3457085/publish";


// ============================================================
// MQTT
// ============================================================

WiFiClient espClient;

PubSubClient client(espClient);

bool estadoMQTT = false;


// ============================================================
// MQTT LOCAL - MOSQUITTO
// ============================================================

const char* mqttServerLocal =
  "192.168.1.135";

const int mqttPortLocal =
  1883;

const char* mqttUserLocal =
  "miusuario";

const char* mqttPasswordLocal =
  "brokerraspsinchal2025";

const char* mqttTopicLocal =
  "nivel";

WiFiClient espClientLocal;

PubSubClient clientLocal(
  espClientLocal
);

String valorPendiente = "";


// ============================================================
// TIMEOUT LoRa
// ============================================================

unsigned long ultimoPaqueteMillis = 0;

const unsigned long INTERVALO_TIMEOUT =
  150000;

bool falloLoraReportado = false;


// ============================================================
// ENCENDER OLED
// ============================================================

void encenderOLED() {

  digitalWrite(
    Vext,
    LOW
  );

  delay(100);

  display.init();

  display.clear();

  display.setTextAlignment(
    TEXT_ALIGN_LEFT
  );

  display.setFont(
    ArialMT_Plain_10
  );

  oledEncendida = true;

  ultimaActividadOLED =
    millis();

  display.drawString(
    0,
    0,
    "SINCHAL RECEPTOR"
  );

  display.drawString(
    0,
    15,
    "OLED ENCENDIDA"
  );

  display.display();

  Serial.println(
    "OLED: ENCENDIDA"
  );
}


// ============================================================
// APAGAR OLED
// ============================================================

void apagarOLED() {

  display.clear();

  display.display();

  display.sleep();

  digitalWrite(
    Vext,
    HIGH
  );

  oledEncendida = false;

  Serial.println(
    "OLED: APAGADA"
  );
}


// ============================================================
// CONTROL DEL BOTÓN OLED
// ============================================================

void controlarBotonOLED() {

  bool estadoBoton =
    digitalRead(BOTON_PIN);

  // Detectar pulsación
  if (
    estadoBoton == LOW &&
    ultimoEstadoBoton == HIGH
  ) {

    if (!oledEncendida) {

      encenderOLED();

    } else {

      // Reiniciar temporizador
      ultimaActividadOLED =
        millis();
    }

    delay(30);
  }

  ultimoEstadoBoton =
    estadoBoton;


  // Apagado automático
  if (
    oledEncendida &&
    millis() -
    ultimaActividadOLED >=
    TIEMPO_OLED
  ) {

    apagarOLED();
  }
}


// ============================================================
// ACTUALIZAR OLED
// ============================================================

void actualizarPantalla(
  bool loraOK
) {

  if (!oledEncendida)
    return;


  // Cambiar página
  if (
    millis() -
    ultimoCambioPagina >=
    INTERVALO_PAGINA
  ) {

    ultimoCambioPagina =
      millis();

    paginaOLED++;

    if (
      paginaOLED > 2
    ) {

      paginaOLED = 0;
    }
  }


  display.clear();


  // ==========================================================
  // SIN SEÑAL
  // ==========================================================

  if (!loraOK) {

    display.drawString(
      0,
      0,
      "SIN SENAL LoRa"
    );

    display.drawString(
      0,
      15,
      "Altura: -21 cm"
    );

    display.drawString(
      0,
      30,
      "WiFi: " +
      String(
        estadoWiFi
        ? "OK"
        : "FAIL"
      )
    );

    display.display();

    return;
  }


  // ==========================================================
  // PÁGINA 0
  // ==========================================================

  if (
    paginaOLED == 0
  ) {

    display.drawString(
      0,
      0,
      "SINCHAL RECEPTOR"
    );

    display.drawString(
      0,
      15,
      "Altura: " +
      String(altura) +
      " cm"
    );

    display.drawString(
      0,
      27,
      "Temp: " +
      String(
        temperatura,
        1
      ) +
      " C"
    );

    display.drawString(
      0,
      39,
      "Ext: " +
      String(
        estadoExtractor
        ? "ON"
        : "OFF"
      )
    );
  }


  // ==========================================================
  // PÁGINA 1
  // ==========================================================

  else if (
    paginaOLED == 1
  ) {

    display.drawString(
      0,
      0,
      "SENSORES"
    );

    display.drawString(
      0,
      15,
      "Caudal: " +
      String(
        caudal,
        1
      ) +
      " L/min"
    );

    display.drawString(
      0,
      30,
      "Presion: " +
      String(
        presion,
        2
      ) +
      " bar"
    );
  }


  // ==========================================================
  // PÁGINA 2
  // ==========================================================

  else {

    display.drawString(
      0,
      0,
      "COMUNICACIONES"
    );

    display.drawString(
      0,
      15,
      "WiFi: " +
      String(
        estadoWiFi
        ? "OK"
        : "FAIL"
      )
    );

    display.drawString(
      0,
      30,
      "MQTT: " +
      String(
        estadoMQTT
        ? "OK"
        : "FAIL"
      )
    );
  }

  display.display();
}


// ============================================================
// RECONEXIÓN WIFI
// ============================================================

void reconnectWiFi() {

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {

    estadoWiFi = true;

    return;
  }

  estadoWiFi = false;


  if (
    millis() -
    ultimoIntentoWiFi >=
    INTERVALO_WIFI
  ) {

    Serial.println(
      "Intentando conectar a WiFi..."
    );

    WiFi.begin(
      ssid,
      password
    );

    ultimoIntentoWiFi =
      millis();
  }
}


// ============================================================
// RECONEXIÓN MQTT THINGSPEAK
// ============================================================

void reconnectMQTT() {

  if (!estadoWiFi)
    return;


  if (
    client.connected()
  ) {

    estadoMQTT = true;

    return;
  }


  Serial.println(
    "Intentando conectar a MQTT ThingSpeak..."
  );


  if (
    client.connect(
      mqttClientID,
      mqttUsername,
      mqttPassword
    )
  ) {

    Serial.println(
      "MQTT ThingSpeak conectado."
    );

    estadoMQTT = true;

  } else {

    Serial.printf(
      "MQTT ThingSpeak fallo: %d\n",
      client.state()
    );

    estadoMQTT = false;
  }
}


// ============================================================
// RECONEXIÓN MQTT LOCAL
// ============================================================

void reconnectMQTTLocal() {

  if (
    !estadoWiFi ||
    clientLocal.connected()
  )
    return;


  Serial.println(
    "Intentando conectar a MQTT Local..."
  );


  if (
    clientLocal.connect(
      "HeltecClient",
      mqttUserLocal,
      mqttPasswordLocal
    )
  ) {

    Serial.println(
      "MQTT Local conectado."
    );

  } else {

    Serial.printf(
      "MQTT Local fallo: %d\n",
      clientLocal.state()
    );
  }
}


// ============================================================
// ENVIAR NIVEL
// ============================================================

void enviarDatos(
  int valor
) {

  if (
    !client.connected()
  ) {

    estadoMQTT = false;

    return;
  }


  String payload =
    String(valor);


  client.publish(
    "channels/3457085/publish/fields/field1",
    payload.c_str()
  );


  estadoMQTT = true;


  Serial.println(
    "ThingSpeak nivel: " +
    payload
  );
}


// ============================================================
// ENVIAR TODOS LOS DATOS A THINGSPEAK
//
// FIELD 1 = NIVEL
// FIELD 2 = CAUDAL
// FIELD 3 = TEMPERATURA
// FIELD 4 = PRESION
// FIELD 5 = EXTRACTOR
// ============================================================

void enviarDatosReservorio(
  int nivel,
  float caudalRecibido,
  float temperaturaRecibida,
  float presionRecibida,
  int extractor
) {

  if (
    !client.connected()
  ) {

    estadoMQTT = false;

    Serial.println(
      "Fallo al publicar ThingSpeak."
    );

    return;
  }


  // ==========================================================
  // CONSTRUIR PAQUETE MQTT
  // ==========================================================

  String payload =
    "field1=" +
    String(nivel) +

    "&field2=" +
    String(caudalRecibido, 2) +

    "&field3=" +
    String(temperaturaRecibida, 1) +

    "&field4=" +
    String(presionRecibida, 2) +

    "&field5=" +
    String(extractor);


  // ==========================================================
  // PUBLICAR
  // ==========================================================

  bool enviado =
    client.publish(
      mqttTopicMulti,
      payload.c_str()
    );


  if (enviado) {

    estadoMQTT = true;

    Serial.println();
    Serial.println(
      "===== THINGSPEAK ====="
    );

    Serial.println(
      payload
    );

    Serial.println(
      "======================"
    );

  } else {

    estadoMQTT = false;

    Serial.println(
      "ERROR: No se pudo publicar en ThingSpeak."
    );
  }
}


// ============================================================
// ENVIAR DATOS A API REST (VERCEL)
// ============================================================

void enviarAPI(
  float alturaVal,
  float presionVal,
  float caudalVal,
  float temperaturaVal,
  int extractorVal
) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("API Vercel: Error, sin conexion WiFi.");
    return;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure(); // Permite conexion HTTPS directa

  HTTPClient http;
  http.setTimeout(4000); // 4 segundos maximo de espera

  // Iniciar conexion HTTP
  if (http.begin(secureClient, serverUrl)) {
    http.addHeader("Content-Type", "application/json");

    // Construir el JSON
    String jsonPayload = "{";
    jsonPayload += "\"altura\":" + String(alturaVal, 2) + ",";
    jsonPayload += "\"presion\":" + String(presionVal, 2) + ",";
    jsonPayload += "\"caudal\":" + String(caudalVal, 2) + ",";
    jsonPayload += "\"temperatura\":" + String(temperaturaVal, 2) + ",";
    jsonPayload += "\"estadoExtractor\":" + String(extractorVal);
    jsonPayload += "}";

    // Enviar solicitud POST
    int httpResponseCode = http.POST(jsonPayload);

    Serial.println();
    Serial.println("===== API VERCEL POST =====");
    if (httpResponseCode > 0) {
      Serial.println("Codigo de respuesta HTTP: " + String(httpResponseCode));
      Serial.println("Respuesta del Servidor: " + http.getString());
    } else {
      Serial.println("Error al enviar POST: " + String(httpResponseCode));
    }
    Serial.println("===========================");

    http.end(); // Liberar recursos
  } else {
    Serial.println("API Vercel: No se pudo inicializar la conexion HTTP.");
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  delay(1000);

  Serial.begin(
    115200
  );


  // ==========================================================
  // MCU HELTEC
  // ==========================================================

  Mcu.begin(
    HELTEC_BOARD,
    SLOW_CLK_TPYE
  );


  // ==========================================================
  // LED
  // ==========================================================

  pinMode(
    LED_PIN,
    OUTPUT
  );

  digitalWrite(
    LED_PIN,
    LOW
  );


  // ==========================================================
  // BOTÓN
  // ==========================================================

  pinMode(
    BOTON_PIN,
    INPUT_PULLUP
  );


  // ==========================================================
  // OLED
  // ==========================================================

  pinMode(
    Vext,
    OUTPUT
  );

  encenderOLED();


  // ==========================================================
  // LoRa RX
  // ==========================================================

  RadioEvents.RxDone =
    OnRxDone;


  Radio.Init(
    &RadioEvents
  );


  Radio.SetChannel(
    RF_FREQUENCY
  );


  Radio.SetRxConfig(
    MODEM_LORA,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODINGRATE,
    0,
    LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT,
    LORA_FIX_LENGTH_PAYLOAD_ON,
    0,
    true,
    0,
    0,
    LORA_IQ_INVERSION_ON,
    true
  );


  // ==========================================================
  // WIFI
  // ==========================================================

  WiFi.begin(
    ssid,
    password
  );

  ultimoIntentoWiFi =
    millis();


  // ==========================================================
  // MQTT THINGSPEAK
  // ==========================================================

  client.setServer(
    mqttServer,
    mqttPort
  );


  // ==========================================================
  // MQTT LOCAL
  // ==========================================================

  clientLocal.setServer(
    mqttServerLocal,
    mqttPortLocal
  );


  // ==========================================================
  // INICIO
  // ==========================================================

  Serial.println();

  Serial.println(
    "================================"
  );

  Serial.println(
    "RECEPTOR LoRa INICIADO"
  );

  Serial.println(
    "ThingSpeak Channel: 3457085"
  );

  Serial.println(
    "F1 = Nivel"
  );

  Serial.println(
    "F2 = Caudal"
  );

  Serial.println(
    "F3 = Temperatura"
  );

  Serial.println(
    "F4 = Presion"
  );

  Serial.println(
    "F5 = Extractor"
  );

  Serial.println(
    "================================"
  );
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // ==========================================================
  // WIFI / MQTT
  // ==========================================================

  reconnectWiFi();

  reconnectMQTT();

  reconnectMQTTLocal();

  client.loop();

  clientLocal.loop();


  // ==========================================================
  // OLED / BOTÓN
  // ==========================================================

  controlarBotonOLED();


  // ==========================================================
  // REINTENTO MQTT LOCAL
  // ==========================================================

  if (
    !valorPendiente.isEmpty() &&
    clientLocal.connected()
  ) {

    if (
      clientLocal.publish(
        mqttTopicLocal,
        valorPendiente.c_str()
      )
    ) {

      Serial.println(
        "Nivel pendiente enviado: " +
        valorPendiente
      );

      valorPendiente =
        "";
    }
  }


  // ==========================================================
  // RECEPCIÓN LoRa
  // ==========================================================

  if (lora_idle) {

    lora_idle = false;

    Radio.Rx(0);
  }


  // ==========================================================
  // LED
  // ==========================================================

  if (paqueteRecibido) {

    digitalWrite(
      LED_PIN,
      HIGH
    );

    delay(50);

    digitalWrite(
      LED_PIN,
      LOW
    );

    paqueteRecibido =
      false;
  }


  // ==========================================================
  // PROCESAR LoRa
  // ==========================================================

  Radio.IrqProcess();


  // ==========================================================
  // TIMEOUT LoRa
  // ==========================================================

  bool loraOK =
    millis() -
    ultimoPaqueteMillis <=
    INTERVALO_TIMEOUT;


  if (
    !loraOK &&
    !falloLoraReportado
  ) {

    altura =
      -21;

    enviarDatos(
      -21
    );

    Serial.println(
      "Fallo LoRa: enviado -21"
    );

    falloLoraReportado =
      true;
  }


  // ==========================================================
  // OLED
  // ==========================================================

  actualizarPantalla(
    loraOK
  );


  delay(10);
}


// ============================================================
// CALLBACK LoRa
//
// FORMATO ÚNICO:
//
// NIVEL,TEMP,EXTRACTOR,CAUDAL,PRESION
//
// Ejemplo:
//
// 112,36.4,0,42.7,3.25
// ============================================================

void OnRxDone(
  uint8_t *payload,
  uint16_t size,
  int16_t rssiVal,
  int8_t snr
) {

  rssi =
    rssiVal;

  rxSize =
    size;


  // ==========================================================
  // COPIAR PAQUETE
  // ==========================================================

  uint16_t len =
    min(
      size,
      (uint16_t)(BUFFER_SIZE - 1)
    );


  memcpy(
    rxpacket,
    payload,
    len
  );


  rxpacket[len] =
    '\0';


  paqueteRecibido =
    true;


  // ==========================================================
  // MOSTRAR PAQUETE
  // ==========================================================

  Serial.printf(
    "\nRX: \"%s\" | RSSI=%d | SNR=%d\n",
    rxpacket,
    rssi,
    snr
  );


  // ==========================================================
  // VARIABLES TEMPORALES
  // ==========================================================

  int nivelRecibido;

  float tempRecibida;

  int extractorRecibido;

  float caudalRecibido;

  float presionRecibida;


  // ==========================================================
  // ÚNICO FORMATO VÁLIDO
  // ==========================================================

  if (
    sscanf(
      rxpacket,
      "%d,%f,%d,%f,%f",
      &nivelRecibido,
      &tempRecibida,
      &extractorRecibido,
      &caudalRecibido,
      &presionRecibida
    ) != 5
  ) {

    Serial.println(
      "ERROR: Paquete LoRa no reconocido."
    );

    Serial.printf(
      "Paquete recibido: %s\n",
      rxpacket
    );

    lora_idle =
      true;

    return;
  }


  // ==========================================================
  // GUARDAR DATOS
  // ==========================================================

  altura =
    nivelRecibido;

  temperatura =
    tempRecibida;

  estadoExtractor =
    extractorRecibido;

  caudal =
    caudalRecibido;

  presion =
    presionRecibida;


  // ==========================================================
  // SERIAL
  // ==========================================================

  Serial.println();

  Serial.println(
    "========== DATOS LoRa =========="
  );

  Serial.printf(
    "Altura     : %d cm\n",
    altura
  );

  Serial.printf(
    "Temperatura: %.1f C\n",
    temperatura
  );

  Serial.printf(
    "Extractor  : %s\n",
    estadoExtractor
    ? "ON"
    : "OFF"
  );

  Serial.printf(
    "Caudal     : %.2f L/min\n",
    caudal
  );

  Serial.printf(
    "Presion    : %.2f bar\n",
    presion
  );

  Serial.println(
    "================================"
  );


  // ==========================================================
  // THINGSPEAK
  //
  // F1 = NIVEL
  // F2 = CAUDAL
  // F3 = TEMPERATURA
  // F4 = PRESION
  // F5 = EXTRACTOR
  // =========================================================

  enviarDatosReservorio(
    altura,
    caudal,
    temperatura,
    presion,
    estadoExtractor
  );


  // ==========================================================
  // MQTT LOCAL
  //
  // SOLO NIVEL
  // ==========================================================

  String nivelString =
    String(altura);


  if (
    clientLocal.connected()
  ) {

    clientLocal.publish(
      mqttTopicLocal,
      nivelString.c_str()
    );

  } else {

    valorPendiente =
      nivelString;
  }


  // ==========================================================
  // API REST (VERCEL)
  // ==========================================================

  enviarAPI(
    (float)altura,
    presion,
    caudal,
    temperatura,
    estadoExtractor
  );


  // ==========================================================
  // ACTUALIZAR ESTADO LoRa
  // ==========================================================

  ultimoPaqueteMillis =
    millis();

  falloLoraReportado =
    false;

  lora_idle =
    true;
}

// ============================================================
// FIN DEL PROGRAMA
// ============================================================