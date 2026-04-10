#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>

// -------- DATI WIFI --------
const char* ssid = "Uaifai";
const char* password = "bulangeu";

// -------- DATI MQTT (HIVEMQ PUBBLICO) --------
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// I VOSTRI DUE CANALI (TOPIC) SEPARATI:
const char* topic_allarme = "ITS/mattia_moustakim/allarme";
const char* topic_sensori = "ITS/mattia_moustakim/sensori";

WiFiClient espClient;
PubSubClient client(espClient);

// Variabile per il cronometro dei sensori (per mandare i dati ogni 5 secondi)
unsigned long ultimoInvioSensori = 0;

// -------- OLED --------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------- DHT22 --------
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// -------- DEFINIZIONE PIN --------
#define LED1 18
#define LED2 19
#define BTN1 4
#define BTN2 5
#define SERVO_PIN 13
#define BUZZER 23
#define PIR_PIN 27

Servo myServo;
int lastPirState = LOW;

// --- FUNZIONE PER CONNETTERSI AL SERVER MQTT ---
void reconnect() {
  while (!client.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("Tentativo di connessione a MQTT (HiveMQ)...");

    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" ✅ Connesso a MQTT!");
    } else {
      Serial.print(" Fallito, errore=");
      Serial.print(client.state());
      Serial.println(" Riprovo tra 5 secondi");
      delay(5000);
    }
  }
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  // OLED Inizializzazione
  Wire.begin(21, 22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Errore OLED");
    while(true);
  }
  display.clearDisplay();
  display.setTextColor(1);
  display.setTextSize(1);

  // --- CONNESSIONE WIFI ---
  Serial.print("Connessione al WiFi: ");
  Serial.println(ssid);

  display.setCursor(0,10);
  display.println("Connessione WiFi...");
  display.println(ssid);
  display.display();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connesso!");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0,20);
  display.println("WIFI CONNESSO!");
  display.display();
  delay(2000);

  // --- CONFIGURO IL SERVER MQTT ---
  client.setServer(mqtt_server, mqtt_port);

  // DHT
  dht.begin();

  // Pin
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  // Servo
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(0);

  Serial.println("Sistema avviato e pronto!");
}

// -------- LOOP --------
void loop() {

  // MANTIENI VIVA LA CONNESSIONE MQTT
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    reconnect();
  }
  client.loop();

  // Lettura DHT
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Errore lettura DHT!");
    delay(2000);
    return;
  }

  // 🔥 NOVITÀ: INVIO DATI SENSORI OGNI 5 SECONDI 🔥
  unsigned long tempoAttuale = millis();
  // Controlla se sono passati 5000 millisecondi (5 secondi) dall'ultimo invio
  if (tempoAttuale - ultimoInvioSensori > 5000) {
    ultimoInvioSensori = tempoAttuale; // Riavvia il cronometro

    if (client.connected()) {
      // Prepara la frase con i gradi e l'umidità
      String pacchetto = "Temp: " + String(temp) + "C | Umidita: " + String(hum) + "%";

      // Spedisce il messaggio sul canale "sensori"
      client.publish(topic_sensori, pacchetto.c_str());
      Serial.println("--> Dati meteo spediti al Web!");
    }
  }

  // OLED - Schermata principale
  display.clearDisplay();
  display.setCursor(0,0);

  if(WiFi.status() == WL_CONNECTED) {
    display.println("          WiFi: OK");
  } else {
    display.println("          WiFi: NO");
  }

  display.print("Temp: ");
  display.print(temp);
  display.println(" C");
  display.print("Hum:  ");
  display.print(hum);
  display.println(" %");

  // --- PIR (Sensore Movimento) ---
  int currentPirState = digitalRead(PIR_PIN);

  if (currentPirState == HIGH && lastPirState == LOW) {
    Serial.println("Movimento rilevato! Erogazione...");

    display.println("\nMOVIMENTO!");
    display.display();

    digitalWrite(BUZZER, HIGH);
    myServo.write(90);

    // 🔥 INVIO IL MESSAGGIO DI ALLARME SUL CANALE DEDICATO 🔥
    if (client.connected()) {
      client.publish(topic_allarme, "Allarme: Movimento Rilevato! Pillola Erogata.");
      Serial.println("--> Messaggio di allarme spedito!");
    }

    delay(1000);

    digitalWrite(BUZZER, LOW);
    myServo.write(0);
  }
  lastPirState = currentPirState;

  // --- Pulsanti -> LED ---
  if (digitalRead(BTN1) == LOW) {
    digitalWrite(LED1, HIGH);
  } else {
    digitalWrite(LED1, LOW);
  }

  if (digitalRead(BTN2) == LOW) {
    digitalWrite(LED2, HIGH);
  } else {
    digitalWrite(LED2, LOW);
  }

  display.display();
  delay(100);
}