
// ===== BIBLIOTECAS =====
// Algunas las instalé después de probar diferentes opciones
#include <WiFi.h>          // Para conexión WiFi
#include <HTTPClient.h>    // Para hacer peticiones web
#include <Adafruit_Sensor.h>
#include <DHT.h>           // Sensor de temperatura/humedad
#include <Adafruit_BMP280.h> // Sensor de presión
#include <ArduinoJson.h>   // Para manejar JSON
#include <Wire.h>          // Comunicación I2C

// ===== CONFIGURACIÓN SENSORES =====
// Probé diferentes pines, el 4 funcionó mejor para el DHT
#define PIN_DHT 4        
#define TIPO_DHT DHT11
DHT sensorDHT(PIN_DHT, TIPO_DHT);

// El BMP280 usa I2C, lo conecté a los pines estándar
Adafruit_BMP280 sensorBMP;    

// ===== CREDENCIALES WIFI =====
// Red wifi
const char* nombreRed = "";           
const char* claveRed = "";   

// ===== OPENWEATHER CONFIG =====
String claveAPI = "";  // Obtener de openweathermap.org
String servidor = "http://api.openweathermap.org/data/3.0";
String idEstacion = "";         // Se genera al registrar

// ===== UBICACIÓN ESTACIÓN =====
// Coordenadas 
String nombreEstacion = "F";
float lat = ;  
float lon =;  
float alt = ;       

// ===== VARIABLES DE CONTROL =====
unsigned long tiempoAnterior = 0;
const long intervaloLectura = 300000; // 5 minutos entre lecturas
int intentosRegistro = 0;
const int maxIntentos = 3; // Si falla 3 veces, esperar

// ==============================
// FUNCIONES PRINCIPALES
// ==============================

/**
 * Intenta conectar al WiFi
 * Tuve que ajustar los tiempos de espera
 */
void conectarWifi() {
  Serial.println("Intentando conectar al WiFi...");
  Serial.print("Red: ");
  Serial.println(nombreRed);
  
  WiFi.begin(nombreRed, claveRed);
  
  int contador = 0;
  bool conectado = false;
  
  // Esperar hasta 15 segundos
  while (contador < 30 && !conectado) {
    delay(500);
    Serial.print(".");
    contador++;
    
    if (WiFi.status() == WL_CONNECTED) {
      conectado = true;
    }
  }
  
  if (conectado) {
    Serial.println("\n¡Conexión WiFi exitosa!");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
    
    // Mostrar fuerza de señal
    int fuerzaSenal = WiFi.RSSI();
    Serial.print("Señal WiFi: ");
    Serial.print(fuerzaSenal);
    Serial.println(" dBm");
    
    if (fuerzaSenal > -50) {
      Serial.println("Excelente señal");
    } else if (fuerzaSenal > -70) {
      Serial.println("Buena señal");
    } else {
      Serial.println("Señal débil - considerar mover el router");
    }
  } else {
    Serial.println("\nError: No se pudo conectar al WiFi");
    Serial.println("Verificar credenciales y señal");
  }
}

/**
 * Verifica si sigue conectado al WiFi
 * Lo agregué después de perder conexión varias veces
 */
void chequearConexion() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("¡Conexión WiFi perdida! Reconectando...");
    conectarWifi();
  }
}

/**
 * Inicializa todos los sensores
 * Tuve problemas con el BMP280 al principio
 */
bool iniciarSensores() {
  Serial.println("Iniciando sensores...");
  delay(1000); // Pequeña pausa para estabilizar
  
  // Configurar I2C para el BMP280
  // Usé los pines por defecto del ESP32
  Wire.begin(21, 22);
  
  // Iniciar DHT11
  sensorDHT.begin();
  Serial.println("Sensor DHT11 listo");
  
  // Intentar iniciar BMP280 - tuve que probar diferentes direcciones
  bool bmpListo = false;
  
  // Primero intentar con dirección 0x76 (la más común)
  if (sensorBMP.begin(0x76)) {
    bmpListo = true;
    Serial.println("BMP280 encontrado en dirección 0x76");
  } 
  // Si no, intentar con 0x77
  else if (sensorBMP.begin(0x77)) {
    bmpListo = true;
    Serial.println("BMP280 encontrado en dirección 0x77");
  }
  
  if (!bmpListo) {
    Serial.println("ERROR: No se pudo encontrar el BMP280");
    Serial.println("Verificar conexiones SDA/SCL y alimentación");
    
    // Escanear bus I2C para diagnóstico
    escanearI2C();
    return false;
  }
  
  // Configurar BMP280 - ajusté estos valores después de probar
  sensorBMP.setSampling(
    Adafruit_BMP280::MODE_NORMAL,    // Modo normal
    Adafruit_BMP280::SAMPLING_X2,    // Temperatura
    Adafruit_BMP280::SAMPLING_X16,   // Presión
    Adafruit_BMP280::FILTER_X16,     // Filtro
    Adafruit_BMP280::STANDBY_MS_500  // Tiempo standby
  );
  
  Serial.println("¡Todos los sensores inicializados correctamente!");
  return true;
}

/**
 * Escanear dispositivos I2C - útil para debugging
 * Lo agregué cuando tuve problemas con el BMP280
 */
void escanearI2C() {
  Serial.println("Escaneando dispositivos I2C...");
  byte error, direccion;
  int dispositivos = 0;
  
  for(direccion = 1; direccion < 127; direccion++) {
    Wire.beginTransmission(direccion);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("Dispositivo I2C encontrado en 0x");
      if (direccion < 16) Serial.print("0");
      Serial.print(direccion, HEX);
      Serial.println(" !");
      dispositivos++;
    }
  }
  
  if (dispositivos == 0) {
    Serial.println("No se encontraron dispositivos I2C");
    Serial.println("Verificar:");
    Serial.println("1. Conexiones SDA/SCL");
    Serial.println("2. Resistores pull-up");
    Serial.println("3. Alimentación 3.3V");
  } else {
    Serial.print("Total dispositivos encontrados: ");
    Serial.println(dispositivos);
  }
}

/**
 * Registrar estación en OpenWeather (solo una vez)
 * Tuve que intentarlo varias veces hasta que funcionó
 */
bool registrarEstacionOpenWeather() {
  // Evitar intentar demasiadas veces
  if (intentosRegistro >= maxIntentos) {
    Serial.println("Demasiados intentos fallidos. Revisar configuración.");
    return false;
  }
  
  intentosRegistro++;
  Serial.print("Intento de registro #");
  Serial.println(intentosRegistro);
  
  // Asegurar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sin conexión WiFi, intentando conectar...");
    conectarWifi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("No se pudo conectar al WiFi");
      return false;
    }
  }

  HTTPClient clienteHTTP;
  String urlCompleta = servidor + "/stations?appid=" + claveAPI;
  
  Serial.print("Conectando a: ");
  Serial.println(urlCompleta);
  
  if (!clienteHTTP.begin(urlCompleta)) {
    Serial.println("Error: No se pudo conectar al servidor");
    return false;
  }
  
  // Configurar headers
  clienteHTTP.addHeader("Content-Type", "application/json");
  clienteHTTP.addHeader("User-Agent", "EstacionMeteoESP32/1.0");

  // Crear JSON manualmente para mejor control
  String jsonDatos = "{";
  jsonDatos += "\"external_id\":\"" + nombreEstacion + "\",";
  jsonDatos += "\"name\":\"" + nombreEstacion + "\",";
  jsonDatos += "\"latitude\":" + String(lat, 6) + ",";
  jsonDatos += "\"longitude\":" + String(lon, 6) + ",";
  jsonDatos += "\"altitude\":" + String(alt, 1);
  jsonDatos += "}";

  Serial.println("Enviando datos de registro...");
  Serial.println("Ubicación: Vera, Santa Fe");
  Serial.print("Coordenadas: ");
  Serial.print(lat, 4);
  Serial.print(", ");
  Serial.println(lon, 4);
  Serial.print("Altitud: ");
  Serial.print(alt);
  Serial.println(" m");
  Serial.println("JSON enviado:");
  Serial.println(jsonDatos);

  int codigoHTTP = clienteHTTP.POST(jsonDatos);
  Serial.print("Código HTTP recibido: ");
  Serial.println(codigoHTTP);
  
  if (codigoHTTP > 0) {
    String respuesta = clienteHTTP.getString();
    Serial.print("Respuesta del servidor: ");
    Serial.println(respuesta);
    
    if (codigoHTTP == 201) { // 201 = Creado
      // Intentar extraer el ID de la estación
      int inicioID = respuesta.indexOf("\"ID\":\"") + 6;
      int finID = respuesta.indexOf("\"", inicioID);
      
      if (inicioID != -1 && finID != -1) {
        idEstacion = respuesta.substring(inicioID, finID);
        Serial.println("¡Estación registrada con éxito!");
        Serial.print("ID asignado: ");
        Serial.println(idEstacion);
        clienteHTTP.end();
        return true;
      } else {
        Serial.println("Error: No se pudo extraer el ID de la respuesta");
      }
    } else if (codigoHTTP == 401) {
      Serial.println("Error 401: API Key inválida o expirada");
    } else if (codigoHTTP == 400) {
      Serial.println("Error 400: Datos incorrectos en la solicitud");
    } else {
      Serial.print("Error HTTP inesperado: ");
      Serial.println(codigoHTTP);
    }
  } else {
    Serial.print("Error de conexión: ");
    Serial.println(clienteHTTP.errorToString(codigoHTTP).c_str());
  }

  clienteHTTP.end();
  return false;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("==================================");
  Serial.println("   ESTACIÓN METEOROLÓGICA VERA SF");
  Serial.println("==================================");
  Serial.println("Iniciando sistema...");
  
  delay(2000); // Pausa para leer el mensaje inicial
  
  // Iniciar componentes en orden
  if (iniciarSensores()) {
    Serial.println("Sensores listos ✓");
  } else {
    Serial.println("Error en sensores ✗");
  }
  
  conectarWifi();
  
  // Intentar registrar estación solo si hay conexión
  if (WiFi.status() == WL_CONNECTED) {
    registrarEstacionOpenWeather();
  }
  
  Serial.println("Configuración completada");
  Serial.println("==================================");
}

// ===== LOOP PRINCIPAL =====
void loop() {
  unsigned long tiempoActual = millis();
  
  // Ejecutar cada 5 minutos
  if (tiempoActual - tiempoAnterior >= intervaloLectura) {
    tiempoAnterior = tiempoActual;
    
    Serial.println();
    Serial.println("--- NUEVA LECTURA ---");
    
    chequearConexion();
    
    if (WiFi.status() == WL_CONNECTED) {
      // Aquí irá el código para leer sensores y enviar datos
      Serial.println("Listo para leer sensores y enviar datos...");
    } else {
      Serial.println("Sin conexión - no se pueden enviar datos");
    }
  }
  
  delay(1000); // Pequeña pausa para no saturar
}

