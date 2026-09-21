#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Reemplaza solo las credenciales de tu red WiFi
const char* WIFI_SSID = "NETLIFE-CORONEL";
const char* WIFI_PASS = "Papitoangel";

// Configuración del servidor y Token
const char* TB_SERVER = "thingsboard.cloud";
const int   TB_PORT   = 1883;
const char* TB_TOKEN = "efi2e62c8eubz4ytk3yt";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a la red: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado con éxito");
  Serial.print("Dirección IP local: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Estableciendo conexión MQTT con ThingsBoard Cloud...");
    
    String clientId = "ESP32WROOM-";
    clientId += String(random(0xffff), HEX);

    // En ThingsBoard, el Access Token se envía en el campo Username (Password = NULL)
    if (client.connect(clientId.c_str(), TB_TOKEN, NULL)) {
      Serial.println(" ¡Conectado exitosamente!");
    } else {
      Serial.print(" Error de conexión, código estado=");
      Serial.print(client.state());
      Serial.println(". Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(TB_SERVER, TB_PORT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Transmisión de datos cada 5 segundos
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    // Valores simulados de prueba
    float temperatura = random(200, 350) / 10.0; // Rango 20.0 a 35.0 °C
    float nivel_agua = random(10, 100);          // Rango 10 a 100 %

    // Estructuración del paquete JSON
    JsonDocument doc;
    doc["temperatura"] = temperatura;
    doc["nivel_agua"] = nivel_agua;

    char buffer[256];
    serializeJson(doc, buffer);

    // Envío directo por protocolo MQTT
    Serial.print("Publicando payload MQTT: ");
    Serial.println(buffer);

    if (client.publish("v1/devices/me/telemetry", buffer)) {
      Serial.println("-> Ok: Telemetría entregada a ThingsBoard");
    } else {
      Serial.println("-> Error: Fallo al publicar el paquete");
    }
  }
}