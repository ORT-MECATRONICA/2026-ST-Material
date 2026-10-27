#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "time.h"
#include "Arduino.h"

//////wifi
const char* ssid = "POCO";        // <-- Cambiar si usas Wi-Fi externo
const char* password = "mirk";        // <-- Cambiar clave de Wi-Fi

const uint8_t name_device = 222; //// device numero de grupo 5A 11x
                             ///                        5B 12x
                             ///                        5C 13x

// Timers auxiliar variables//////////////////////////
unsigned long now = millis();    /// valor actual
unsigned long lastMeasure1 = 0;  /// variable para contar el tiempo actual
unsigned long lastMeasure2 = 0;  /// variable para contar el tiempo actual

const unsigned long interval_envio = 30000;  // Intervalo de envio de datos mqtt
const unsigned long interval_leeo = 60000;   // Intervalo de lectura de datos y guardado en la cola

/// time
long unsigned int timestamp;  // hora
const char* ntpServer = "south-america.pool.ntp.org";
const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

/// variables ingresar a la cola struct
int indice_entra = 0;
int indice_saca = 0;
bool flag_vacio = 1;

///// mqtt (EMQX CLOUD SSL/TLS)
#define MQTT_HOST "sa11b75f.ala.us-east-1.emqxsl.com"
#define MQTT_PORT 8883
#define MQTT_USERNAME "esp32"
#define MQTT_PASSWORD "mirko15"
#define MQTT_PUB "/esp32/datos_sensores"

char mqtt_payload[150];

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

typedef struct
{
  long time;
  float T1;   /// temp en grados
  float P1;   /// valor presion entre 0 y 99 
  float luz;  /// valor entre 0 y 99 . si hay luz es cercano al 100 
  float G1;   /// valor gas entre 0 y 99 
  float cor;  /// valor corriente entre 0 y 99
  bool pre;   /// Lectura sensores de presencia 1 presencia 0 no presencia
  bool oct;   /// Lectura sensores de presencia 1 presencia 0 no presencia
  bool Alarma; // si se supera un umbral

} estructura;

const int valor_max_struct = 1000;          /// valor vector de struct
estructura datos_struct[valor_max_struct];  /// Guardo valores hasta que lo pueda enviar
estructura aux2;

// Declaraciones de funciones
void connectToWifi();
void connectToMqtt();
void fun_saca();

void connectToWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectToMqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT (EMQX Cloud SSL)...");
    String clientId = "ESP32_Grupo_" + String(name_device) + "_" + WiFi.macAddress();
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println(" ¡Conectado a MQTT!");
    } else {
      Serial.print(" Falló conexión, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}

////////////////////////////Envio de datos mqtt//////////////////////////////////////////
////////Funcion que envia valores cuando la estructura no este vacia ///////////////////
///////////////////////////////////////////////////////////////////////////////////////
void fun_envio_mqtt() {
  fun_saca();           //// veo si hay valores nuevos
  if (flag_vacio == 0)  //// si hay los envio
  {
    Serial.print("enviando ");
    snprintf(mqtt_payload, sizeof(mqtt_payload), 
         "%d&%ld&%.2f&%.2f&%.2f&%.2f&%.2f&%u&%u&%u", 
         name_device, aux2.time, aux2.T1, aux2.P1, aux2.luz, 
         aux2.G1, aux2.cor, aux2.pre, aux2.oct, aux2.Alarma);
    
    aux2.time = 0;
    aux2.T1 = 0;
    aux2.P1 = 0;
    aux2.luz = 0;
    aux2.G1 = 0;
    aux2.cor = 0;
    aux2.pre = 0;
    aux2.oct = 0;
    aux2.Alarma = 0;

    Serial.print("Publish message: ");
    Serial.println(mqtt_payload);

    mqttClient.publish(MQTT_PUB, mqtt_payload);
  } else {
    Serial.println("no hay valores nuevos");
  }
}

///////////////////////////////////////////////////////////////////////////
/////////////Funcion que saca un valor de la estructura para enviar //////
///////////////////////////////////////////////////////////////////////
void fun_saca() {
  if (indice_saca != indice_entra) {
    aux2.time = datos_struct[indice_saca].time;
    aux2.T1 = datos_struct[indice_saca].T1;
    aux2.P1 = datos_struct[indice_saca].P1;
    aux2.luz = datos_struct[indice_saca].luz;
    aux2.G1 = datos_struct[indice_saca].G1;
    aux2.cor = datos_struct[indice_saca].cor;
    aux2.pre = datos_struct[indice_saca].pre;
    aux2.oct = datos_struct[indice_saca].oct;
    aux2.Alarma = datos_struct[indice_saca].Alarma;

    flag_vacio = 0;

    if (indice_saca >= (valor_max_struct - 1)) {
      indice_saca = 0;
    } else {
      indice_saca++;
    }
    Serial.print("saco valores de la struct isaca:");
    Serial.println(indice_saca);
  } else {
    flag_vacio = 1;  ///// no hay datos
  }
}

/////////////////////////////////////////////////////////////////////
/////////////funcion que ingresa valores a la cola struct///////////
///////////////////////////////////////////////////////////////////
void fun_entra(void) {
  if (indice_entra >= valor_max_struct) {
    indice_entra = 0;
  }

  Serial.print("> NTP Time:");
  timestamp = time(NULL);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  datos_struct[indice_entra].time = timestamp;
  datos_struct[indice_entra].T1 = 33;  
  datos_struct[indice_entra].P1 = 42;  
  datos_struct[indice_entra].luz = 52;
  datos_struct[indice_entra].G1 = 6;
  datos_struct[indice_entra].cor = 7;
  datos_struct[indice_entra].pre = 8;
  datos_struct[indice_entra].oct = 9;
  datos_struct[indice_entra].Alarma = 1;

  indice_entra++;
  Serial.print("saco valores de la struct ientra: ");
  Serial.println(indice_entra);
}

////////////////////////////////////////////////////////////////////
/////////////SETUP/////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  
  connectToWifi();

  // Habilitar conexión SSL sin verificar certificado CA en flash
  espClient.setInsecure();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  // Setup de time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }
  if (!mqttClient.connected()) {
    connectToMqtt();
  }
  mqttClient.loop();

  now = millis();
  if (now - lastMeasure1 > interval_envio) {
    lastMeasure1 = now;
    fun_envio_mqtt();
  }
  if (now - lastMeasure2 > interval_leeo) {
    lastMeasure2 = now;
    fun_entra();
  }
}