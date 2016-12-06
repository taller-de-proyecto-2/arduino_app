/*
 * Sistema de seguimiento GPS.
 * 
 * Cátedra de Taller de Proyecto II
 * UNLP - 2016
 * 
 * Archivo principal de la aplicación.
 * Definición de la máquina de estados y sus  comportamientos.
 * 
 * Autores:
 *  - Ailán, Julián
 *  - Hourquebie, Lucas
 */

#include <gpstracker.h>

File file;
int flag = 0, redirection = 0;
int doneWithConfiguration;
String latitude, longitude, timestamp, hours, minutes, seconds;

PROGMEM const int l=D4,r=D3,f=D2,b=D1,h=D0;

WiFiClient client;
TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);
Ticker piezoTimerInterrupt, redLed, greenLed, blueLed;
File wifiConfig;
WiFiServer server(8080);

void setup() {  
  /* Inicio de la comunicación serie con la PC para debugging. */
  Serial.begin(115200);
  Serial.println("\nComienza la aplicación.");

  /* Incialización del watchdog por software. */
  ESP.wdtDisable();
  ESP.wdtEnable(0);
  /* Incialización del watchdog por hardware. */
  delay(1);
  feedWDT();
  yield();

  /* Configuración GPS */
  ss.begin(GPSBaud);

  /* Inicialización de almacenamiento en SPIFFS. */
  SPIFFS.begin();

  /* Quitar el comentario si se busca formatear el filesystem. */
  //SPIFFS.format();

  /* Quitar los comentarios si se busca eliminar el archivo de
   *  configuración de redes al inicio de la aplicación.
  */
  //(SPIFFS.exists("/wifiConfig.txt")) {
  //  SPIFFS.remove("/wifiConfig.txt");
  //}

  /* Configuración del pin para interrupciones de piezoeléctrico */
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), increment, RISING);

  /* Configuración de la interrupción de timer para el sensor piezoeléctrico. */
  piezoTimerInterrupt.attach(2, numberOfInterrupts);

  /* Configuración de las interrupciones de timer para leds indicadores de estado. */
  redLed.attach(0.5, configOrReady);
  greenLed.attach(0.5, performingTracking);
  blueLed.attach(0.5, uploading);
  
  digitalWrite(LED_BUILTIN, HIGH);

  /* Inicialización de la máquina de estados finitos. */
  state = WAITING;
}

void loop() {
  /* Refresca al watchdog por software. */
  ESP.wdtFeed();
  /* Refresca al watchdog por hardware. */
  WRITE_PERI_REG(0x60000914, 0x73);
  /* Actualización de la máquina de estados. */
  finite_state_machine();
}

static void finite_state_machine(void) {
  switch (state) {
    case WIFI_CONFIG:
      buildServer();
      Serial.println("Finalizó el armado del Access Point.");
      server.begin();
      Serial.println("Esperando obtener un cliente que envíe una configuración de red.");
      
      doneWithConfiguration = 0;
      do {
        doneWithConfiguration = saveConfiguration();
      } while (!doneWithConfiguration);

      Serial.println("Respuesta enviada al cliente.");
      
      if(redirection == 0) {
        state = WAITING;
        Serial.println("Esperando la señal de inicio del muestro.");
      } else {
        state = NET_CONNECTING;
      }
    break;
    case WAITING:
      if(!SPIFFS.exists("/wifiConfig.txt")) {
        Serial.println("No existe un archivo de configuración de redes.");
        state = WIFI_CONFIG;
        redirection = 0;
      } else {
        if(getTimes() >= 1) {
          setTimes(0);
          state = SAMPLING;
        }
      }
    break;
    case SAMPLING:
      Serial.println("Almacenando muestras del GPS.");
      gpsSampling();
      state = NET_CONNECTING;
    break;
    case NET_CONNECTING:
      Serial.println("Buscando conectarse a una red WiFi.");
      connectionEstablishment();
      if(redirection == 1) {
        state = WIFI_CONFIG;
      } else {
        state = UPLOADING;
      }
    break;
    case UPLOADING:
      Serial.println("Subiendo muestras al servidor.");
      dataUpload();
      Serial.println("Finalizó la subida de muestras al servidor.");
      state = WAITING;
    break;
  }
}

static void buildServer(void) {
  const char WiFiAPPSK[] = "123456789";
  WiFi.mode(WIFI_AP);

  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = "ESP8266 GPS Tracker " + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i = 0; i < AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar, WiFiAPPSK);
}

static int saveConfiguration(void) {
  client = server.available();
  if (!client) {
    return 0;
  }

  Serial.println("La IP estática del AP es: " + WiFi.softAPIP());

  // Lectura de la petición HTTP.
  String req = client.readStringUntil('\r');
  
  int ssidIndex = req.indexOf("ssid=");
  int passIndex = req.indexOf("password=");
  int httpIndex = req.indexOf("HTTP/");

  String networkSsid = req.substring(ssidIndex + 5, passIndex - 1);
  String networkPass = req.substring(passIndex + 9, httpIndex - 1);

  // Generación de la respuesta HTTP a partir del header general.
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";

  if((networkSsid != "") && (networkPass != "")) {
    s += "<body>Datos correctamente recibidos.</body>";
  } else {
    s += "<body>Datos recibidos de forma incorrecta.</body>";
  }

  s += "</html>\n";

  // Envia la respuesta al cliente.
  client.print(s);
  delay(1);

  File wifiConfig = SPIFFS.open("/wifiConfig.txt", "a");
  wifiConfig.println(networkSsid);
  wifiConfig.println(networkPass);
  wifiConfig.close();

  wifiConfig = SPIFFS.open("/wifiConfig.txt", "r");
  while(wifiConfig.available()){
    Serial.println(wifiConfig.readStringUntil('\n'));
  }
  wifiConfig.close();

  return 1;
}

static void gpsSampling(void) {
  float previousLatitude = 0, previousLongitude = 0;
  float currentLatitude, currentLongitude;
  double difference = 0;
  file = SPIFFS.open("/gps_samples.txt", "w");
  while(!flag){
    currentLatitude = gps.location.lat();
    currentLongitude = gps.location.lng();
    difference = gps.distanceBetween(previousLatitude, previousLongitude, currentLatitude, currentLongitude);
    yield();
    Serial.println(difference);
    // No se guardan las muestras que estén a menos de 5 metros de la última muestra guardada.
    if(difference > 5){
      previousLatitude = currentLatitude;
      previousLongitude = currentLongitude;
      file.println(currentLatitude, 6);
      file.println(currentLongitude, 6);
      hours = String(gps.time.hour());
      hours = ((hours.length() > 1) ? hours : ("0" + hours));
      minutes = String(gps.time.minute());
      minutes = ((minutes.length() > 1) ? minutes : ("0" + minutes));
      seconds = String(gps.time.second());
      seconds = ((seconds.length() > 1) ? seconds : ("0" + seconds));
      yield();
      file.println(hours + minutes + seconds);
      Serial.println(hours + minutes + seconds);
    }
    smartDelay(1000);
    if(flag){
      flag = 0;
      break;    
    }
  }
  if(file) {
    file.close(); 
  }
}

static void connectionEstablishment(void) {
  File wifiConfig = SPIFFS.open("/wifiConfig.txt", "r");
  String ssid, pass;
  int numberOfTries = 0, maxNumberOfTries = 20;
    
  while(wifiConfig.available()){
    ssid = wifiConfig.readStringUntil('\n');
    pass = wifiConfig.readStringUntil('\n');
    char ssid_char_array[ssid.length()];
    char pass_char_array[pass.length()];
    ssid.toCharArray(ssid_char_array, ssid.length());
    pass.toCharArray(pass_char_array, pass.length());
    Serial.println(ssid_char_array);
    Serial.println(pass_char_array);

    WiFi.mode(WIFI_STA);
    
    if (WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid_char_array, pass_char_array);
      while ((WiFi.status() != WL_CONNECTED) && (numberOfTries < maxNumberOfTries)) {
        delay(500);
        Serial.println("Conectando...");
        numberOfTries++;
      }
      if(WiFi.status() == WL_CONNECTED) {
        break;
      } else {
        numberOfTries = 0;
        WiFi.mode(WIFI_OFF);
      }
    }
  }
  if(wifiConfig) {
    wifiConfig.close();
  }
  Serial.println(WiFi.status() == WL_CONNECTED);
  if(WiFi.status() != WL_CONNECTED) {
    redirection = 1;
  } else {
    Serial.println("Conectado.");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

static void dataUpload(void) {
  file = SPIFFS.open("/gps_samples.txt", "r");
  while(file.available()){
    latitude = file.readStringUntil('\n');
    latitude.remove(latitude.length() - 1);
    longitude = file.readStringUntil('\n');
    longitude.remove(longitude.length() - 1);
    timestamp = file.readStringUntil('\n');
    String url = "/create_position.php?latitude=" + latitude + "&longitude=" + longitude + "&registered_at=" + timestamp;
    client.connect(host, httpPort);
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  }
  if(file) {
    file.close();
    SPIFFS.remove("/gps_samples.txt");
  }
}

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available() > 0)
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void numberOfInterrupts(void) {
  if (getTimes() >= 2) {
    flag = 1;
  }
  setTimes(0);
}

static void configOrReady(void) {
  if(state == WIFI_CONFIG) {
    digitalWrite(b, HIGH);
  } else if(state == WAITING) {
    digitalWrite(b, !digitalRead(b));
  } else {
    digitalWrite(b, LOW);
  }
}

static void performingTracking(void) {
  if(state == SAMPLING) {
    digitalWrite(f, HIGH);
  } else {
    digitalWrite(f, LOW);
  }
}

static void uploading(void) {
  if(state == UPLOADING) {
    digitalWrite(r, HIGH);
  } else if(state == NET_CONNECTING) {
    digitalWrite(r, !digitalRead(r));
  } else {
    digitalWrite(r, LOW);
  }
}

static void feedWDT(void){
  pinMode(f, OUTPUT);
  pinMode(b, OUTPUT);
  pinMode(l, OUTPUT);
  pinMode(r, OUTPUT);
  pinMode(h, OUTPUT);
  digitalWrite(f, LOW);
  digitalWrite(b, LOW);
  digitalWrite(l, LOW);
  digitalWrite(r, LOW);
  digitalWrite(h, LOW);
}
