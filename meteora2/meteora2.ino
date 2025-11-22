#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>

// ===== CONFIG WIFI =====
const char* ssid = "Personal-366";           
const char* password = "vtbYtEnD5v";   



// ===== SENSORES =====
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp;

//  OPENWEATHER
String apiKey = "71ea023be853d15ea70635759233eecb";
String station_id = "";
String stationName = "Estacion Meteora Vera";
float latitude = -29.4650;
float longitude = -60.2110;
float altitude = 100;

//  VARIABLES 
float tempLocal = NAN;
float humLocal = NAN;
float presLocal = NAN;
String climaPronostico = "Cargando...";
bool sensorDHT_OK = false;
bool sensorBMP_OK = false;

//  TIEMPO 
unsigned long lastSend = 0;
const long sendInterval = 10 * 60 * 1000;

//  MEMORIA 
Preferences preferences;

// SERVIDOR WEB 
WebServer server(80);


void setup() {
  Serial.begin(115200);

  dht.begin();
  sensorBMP_OK = bmp.begin(0x76);
  if (!sensorBMP_OK) Serial.println("❌ No se encontró BMP280");

  // Conectar WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado");
  Serial.print("IP local: "); Serial.println(WiFi.localIP());

  // Cargar station_id
  preferences.begin("openweather", false);
  station_id = preferences.getString("station_id", "");
  preferences.end();

  if (station_id == "") registrarEstacion();

  // Servidor web
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Servidor web iniciado en /");
}


void loop() {
  server.handleClient();

  if (millis() - lastSend > sendInterval) {
    lastSend = millis();
    leerSensores();
    enviarMedicion();
    obtenerPronostico();
  }
}


void leerSensores() {
  // DHT22
  float temp = NAN;
  float hum = NAN;
  sensorDHT_OK = false;
  for (int i = 0; i < 5; i++) {
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
      sensorDHT_OK = true;
      break;
    }
    delay(200);
  }
  tempLocal = temp;
  humLocal = hum;

  // BMP280
  if (sensorBMP_OK) {
    presLocal = bmp.readPressure() / 100.0F;
    if (isnan(presLocal)) sensorBMP_OK = false;
  } else {
    presLocal = NAN;
  }

  // Mensajes Serial
  Serial.println("=== Lectura de sensores ===");
  if (sensorDHT_OK) Serial.printf("DHT22 OK: Temp %.1f°C, Hum %.1f%%\n", tempLocal, humLocal);
  else Serial.println("❌ Error leyendo DHT22");
  if (sensorBMP_OK) Serial.printf("BMP280 OK: Pres %.1f hPa\n", presLocal);
  else Serial.println("❌ Error leyendo BMP280");
}

// =======================
void registrarEstacion() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://api.openweathermap.org/data/3.0/stations?appid=" + apiKey);
    http.addHeader("Content-Type", "application/json");

    String body = "{";
    body += "\"external_id\":\"VERA_001\",";
    body += "\"name\":\"" + stationName + "\",";
    body += "\"latitude\":" + String(latitude, 6) + ",";
    body += "\"longitude\":" + String(longitude, 6) + ",";
    body += "\"altitude\":" + String(altitude);
    body += "}";

    int code = http.POST(body);
    String resp = http.getString();
    Serial.print("Registrar estación código: "); Serial.println(code);
    Serial.println("Respuesta: " + resp);

    if (code == 201 || code == 200) {
      StaticJsonDocument<512> doc;
      if (!deserializeJson(doc, resp)) {
        station_id = doc["ID"].as<String>();
        preferences.begin("openweather", false);
        preferences.putString("station_id", station_id);
        preferences.end();
        Serial.println("✅ Station_id guardado: " + station_id);
      }
    }
    http.end();
  }
}

// =======================
void enviarMedicion() {
  if (WiFi.status() == WL_CONNECTED && station_id != "") {
    if (!sensorDHT_OK && !sensorBMP_OK) return; // nada que enviar

    time_t now; time(&now);
    String payload = "[{";
    payload += "\"station_id\":\"" + station_id + "\",";
    payload += "\"dt\":" + String(now) + ",";
    if (sensorDHT_OK) payload += "\"temperature\":" + String(tempLocal, 2) + ",";
    if (sensorBMP_OK) payload += "\"pressure\":" + String(presLocal, 1) + ",";
    if (sensorDHT_OK) payload += "\"humidity\":" + String(humLocal, 1);
    payload += "}]";

    HTTPClient http;
    http.begin("http://api.openweathermap.org/data/3.0/measurements?appid=" + apiKey);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    Serial.print("Código POST medición: "); Serial.println(code);
    http.end();
  }
}

// =======================
void obtenerPronostico() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.openweathermap.org/data/2.5/forecast?lat=" + 
                 String(latitude, 6) + "&lon=" + String(longitude, 6) + 
                 "&appid=" + apiKey + "&units=metric&lang=es";

    http.begin(url);
    int code = http.GET();
    Serial.println("Código respuesta pronóstico: " + String(code));

    if (code == 200) {
      String resp = http.getString();
      StaticJsonDocument<4096> doc;
      DeserializationError error = deserializeJson(doc, resp);

      if (!error) {
        time_t now; time(&now);
        int mejorIndice = 0;
        long menorDif = 99999999;

        // Buscar el pronóstico más cercano a la hora actual
        for (int i = 0; i < 40; i++) {
          long dt = doc["list"][i]["dt"];
          long dif = abs(dt - now);
          if (dif < menorDif) {
            menorDif = dif;
            mejorIndice = i;
          }
        }

        float temp = doc["list"][mejorIndice]["main"]["temp"];
        float hum = doc["list"][mejorIndice]["main"]["humidity"];
        String desc = doc["list"][mejorIndice]["weather"][0]["description"].as<String>();
        String horaUTC = doc["list"][mejorIndice]["dt_txt"].as<String>();

        // Ajuste horario manual a Argentina (UTC-3)
        int hora = horaUTC.substring(11, 13).toInt();
        hora -= 3;
        if (hora < 0) hora += 24; // Si da negativo, corregir al día anterior
        String horaLocal = String(hora) + horaUTC.substring(13, 16);

        climaPronostico = "En " + horaLocal + 
                          "hs → " + desc + 
                          ", Temp: " + String(temp, 1) + "°C, Hum: " + String(hum, 1) + "%";
      } else {
        Serial.print("❌ Error parseando JSON: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.println("❌ Error al obtener pronóstico");
    }
    http.end();
  }
}
// =======================
void handleRoot() {
  leerSensores(); // actualizar antes de mostrar
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>" + stationName + "</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#e0f7fa;color:#006064;text-align:center;}";
  html += "h1{color:#004d40;}";
  html += ".card{background:white;border-radius:10px;padding:20px;margin:20px auto;width:80%;max-width:400px;box-shadow:0 4px 8px rgba(0,0,0,0.2);}";
  html += ".error{color:red;font-weight:bold;}";
  html += "</style></head><body>";

  html += "<h1>" + stationName + "</h1>";
  html += "<div class='card'>";
  if (sensorDHT_OK) html += "<p>Temperatura: " + String(tempLocal,1) + " °C</p><p>Humedad: " + String(humLocal,1) + " %</p>";
  else html += "<p class='error'>Error leyendo DHT22</p>";
  if (sensorBMP_OK) html += "<p>Presión: " + String(presLocal,1) + " hPa</p>";
  else html += "<p class='error'>Error leyendo BMP280</p>";
  html += "</div>";

  html += "<div class='card'>" + climaPronostico + "</div>";
  html += "<p>Última actualización: " + String(millis()/1000/60) + " minutos desde inicio</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}
