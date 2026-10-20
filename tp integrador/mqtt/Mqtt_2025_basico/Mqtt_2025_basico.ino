#include <WiFi.h>
#include "AsyncMqttClient.h"
#include <Wire.h>
#include "time.h"
#include "Arduino.h"


//////wifi
const char* ssid = "MECA-IoT";
const char* password = "IoT$2025";

const char name_device = 111;  ////device numero de grupo 5A 11x siendo x el numero de grupo
                             ///                        5B 12x siendo x el numero de grupo
                             /// 5a grupo 4 => 114
                             /// 5b grupo 3 => 123
                            /// 5C grupo 3 => 133
      
// Timers auxiliar variables//////////////////////////
unsigned long now = millis();    ///valor actual
unsigned long lastMeasure1 = 0;  ///variable para contar el tiempo actual
unsigned long lastMeasure2 = 0;  ///variable para contar el tiempo actual

const unsigned long interval_envio = 30000;  //Intervalo de envio de datos mqtt
const unsigned long interval_leeo = 60000;   //Intervalo de lectura de datos y guardado en la cola
int i = 0;

///time
long unsigned int timestamp;  // hora
const char* ntpServer = "south-america.pool.ntp.org";
const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

///variables ingresar a la cola struct
int indice_entra = 0;
int indice_saca = 0;
bool flag_vacio = 1;

/////mqqtt
#define MQTT_HOST IPAddress(192, 168, 10, 151)  ///se debe cambiar por el ip de meca o hall del 4
#define MQTT_PORT 1884
#define MQTT_USERNAME "esp32"
#define MQTT_PASSWORD "mirko15"
char mqtt_payload[150];  /////
// Test MQTT Topic
#define MQTT_PUB "/esp32/datos_sensores"
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

typedef struct
{
  long time;
  float T1;  ///temp en grados
  float H1;  ///valor presion entre 0 y 99 
  float luz;  ///valor entre 0 y 99 . si hay luz es cercano al 100 
  float G1; ///valor gas entre 0 y 99 
  float G2; ///valor corriente entre 0 y 99
  bool oct; ///Lectura sensores de presencia 1 presencia 0 no presencia
  bool Alarma; // si se supera un umbral

} estructura;
////////////////
const int valor_max_struct = 1000;          ///valor vector de struct
estructura datos_struct[valor_max_struct];  ///Guardo valores hasta que lo pueda enviar
estructura aux2;

/////*********************************************************************/////
////////////////////////////setup wifi/////////////////////////////////////////
/////*********************************************************************/////
void setupmqtt() {
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  WiFi.onEvent(WiFiEvent);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USERNAME, MQTT_PASSWORD);
  connectToWifi();
}
////////////////////////////Envio de datos mqtt//////////////////////////////////////////
////////Funcion que envia valores cuando la estructura no este vacia ///////////////////
///////////////////////////////////////////////////////////////////////////////////////
void fun_envio_mqtt() {
  fun_saca();           ////veo si hay valores nuevos
  if (flag_vacio == 0)  ////si hay los envio
  {
    Serial.print("enviando");
    ////genero el string a enviar 1. 2.   3.   4.   5.   6.   7   8  9.       1.         2.        3.      4.       5.           6.   7.        8.        9
    snprintf(mqtt_payload, 150, "%u&%ld&%.2f&%.2f&%.2f&%.2f&%.2f&%u&%u", name_device, aux2.time, aux2.T1, aux2.H1, aux2.luz, aux2.G1, aux2.G2, aux2.oct, aux2.Alarma);  //random(10,50)
    aux2.time = 0;                                                                                                                                                   ///limpio valores
    aux2.T1 = 0;
    aux2.H1 = 0;
    aux2.luz = 0;
    aux2.G1 = 0;
    aux2.G2 = 0;
    aux2.oct = 0;
    aux2.Alarma = 0;

    Serial.print("Publish message: ");
    Serial.println(mqtt_payload);
    // Publishes Temperature and Humidity values
    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB, 1, true, mqtt_payload);
  } else {
    Serial.println("no hay valores nuevos");
  }
}  ///////////////////////////////////////////////////

///////////////////////////////////////////////////
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
}  ///////////////////////////////////////////////////
void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}  ///////////////////////////////////////////////////

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0);  // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}  ///////////////////////////////////////////////////
////////////////NO TOCAR FUNCIONES MQTT///////////////////////////////////
void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}  ///////////////////////////////////////////////////

////////////////NO TOCAR FUNCIONES MQTT///////////////////////////////////
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}
////////////////NO TOCAR FUNCIONES MQTT///////////////////////////////////
void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}  ///////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
/////////////Funcion que saca un valor de la estructura para enviar //////
///////////////////////////////////////////////////////////////////////
void fun_saca() {
  if (indice_saca != indice_entra) {
    aux2.time = datos_struct[indice_saca].time;
    aux2.T1 = datos_struct[indice_saca].T1;
    aux2.H1 = datos_struct[indice_saca].H1;
    aux2.luz = datos_struct[indice_saca].luz;
    aux2.G1 = datos_struct[indice_saca].G1;
    aux2.G2 = datos_struct[indice_saca].G2;
    aux2.oct = datos_struct[indice_saca].oct;
    aux2.Alarma = datos_struct[indice_saca].Alarma;

    flag_vacio = 0;

    Serial.println(indice_saca);
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
  return;
}
/////////////////////////////////////////////////////////////////////
/////////////funcion que ingresa valores a la cola struct///////////
///////////////////////////////////////////////////////////////////
void fun_entra(void) {
  if (indice_entra >= valor_max_struct) {
    indice_entra = 0;  ///si llego al maximo de la cola se vuelve a cero
  }
  //////////// timestamp/////// consigo la hora
  Serial.print("> NTP Time:");
  timestamp = time(NULL);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;  //// si no puede conseguir la hora
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  ///////////////////////// fin de consigo la hora
  datos_struct[indice_entra].time = timestamp;
  datos_struct[indice_entra].T1 = 20;  /// leeo los datos //aca va la funcion de cada sensor
  datos_struct[indice_entra].H1 = 30;  //// se puede pasar por un parametro valor entre 0 y 100
  datos_struct[indice_entra].luz = 30;
  datos_struct[indice_entra].G1 = 1.5;
  datos_struct[indice_entra].G2 = 1.5;
  datos_struct[indice_entra].oct = 1.5;
  datos_struct[indice_entra].Alarma = 1;

  indice_entra++;
  Serial.print("saco valores de la struct ientra");
  Serial.println(indice_entra);
}
////////////////////////////////////////////////////////////////////
/////////////SETUP/////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  /////declaro pines digitales
  setupmqtt();
  //Setup de time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}  ///////////////////////////////////////////////////

void loop() {
  now = millis();
  if (now - lastMeasure1 > interval_envio) {  ////envio el doble de lectura por si falla algun envio
    lastMeasure1 = now;                       /// cargo el valor actual de millis
    fun_envio_mqtt();                         ///envio los valores por mqtt
  }
  if (now - lastMeasure2 > interval_leeo) {
    lastMeasure2 = now;  /// cargo el valor actual de millis
    fun_entra();         ///ingreso los valores a la cola struct
  }
}
