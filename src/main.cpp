#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"

// ==========================================
//        CONFIGURAZIONE E PARAMETRI
// ==========================================

// --- WiFi & Orologio (NTP) ---
const char* ssid = "Uaifai";
const char* password = "bulangeu";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;      // Fuso orario Italia (+1h)
const int   daylightOffset_sec = 3600; // Ora legale (+1h)

// --- MQTT ---
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port = 1883;
const char* topic_eventi = "pillmate/disp_01/eventi";
const char* topic_sensori = "pillmate/disp_01/telemetria";
const char* topic_comandi = "pillmate/disp_01/comandi";

// --- Pin Hardware ---
#define DHTPIN 15
#define DHTTYPE DHT22
#define PIR_PIN 27
#define SERVO_PIN 13
#define BUZZER_PIN 23

// --- Display OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==========================================
//        ISTANZE E VARIABILI GLOBALI
// ==========================================

DHT dht(DHTPIN, DHTTYPE);
Servo myServo;
WiFiClient espClient;
PubSubClient client(espClient);

// Variabili Sensori
float t_letta = 0.0;
float h_letta = 0.0;

// Variabili Allarme e Buzzer
bool allarmeAttivo = false;
unsigned long ultimoBip = 0;
bool statoBuzzer = false;

// Variabili Logica PIR (QUELLE CHE MANCAVANO)
int ultimoStatoPIR = LOW;
unsigned long tempoUltimoScatto = 0;

// ==========================================
//        FUNZIONI DI SUPPORTO
// ==========================================

String ottieniOra() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "--:--";
  char oraFormattata[9];
  strftime(oraFormattata, 9, "%H:%M:%S", &timeinfo);
  return String(oraFormattata);
}

void aggiornaOLED() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Intestazione con Ora
  display.setCursor(30, 0);
  display.setTextSize(2);
  display.println(ottieniOra());
  display.setTextSize(1);
  display.println("---------------------");

  // Dati Sensori o Messaggio di Allarme
  display.printf("Temp: %.1f C\n", t_letta);
  if(allarmeAttivo) {
    display.println(">>> PRENDI PILLOLA <<<");
  } else {
    display.printf("Umid: %.1f %%\n", h_letta);
  }
  display.println();

  // Stato Connessioni
  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "DISCONN.");
  display.print("MQTT: ");
  display.println(client.connected() ? "OK" : "ERRORE");

  display.display();
}

void erogaPillola() {
  Serial.println(">>> EROGAZIONE IN CORSO <<<");

  // Spegne l'allarme
  allarmeAttivo = false;
  noTone(BUZZER_PIN);

  // Muove il servo
  myServo.write(90);
  delay(1000);
  myServo.write(0);

  // Invia conferma al server
  if (client.connected()) {
    client.publish(topic_eventi, "{\"azione\": \"erogata_e_confermata\"}");
  }
  aggiornaOLED();
}

// ==========================================
//        GESTIONE RETE E COMANDI
// ==========================================

void callback(char* topic, byte* payload, unsigned int length) {
  String messaggio = "";
  for (int i = 0; i < length; i++) messaggio += (char)payload[i];

  Serial.println("\n[MQTT] Comando ricevuto: " + messaggio);

  if (messaggio == "eroga_ora") {
    erogaPillola();
  }
  else if (messaggio == "attiva_allarme") {
    Serial.println("🔔 Allarme attivato dal Server!");
    allarmeAttivo = true;
    aggiornaOLED();
  }
  else if (messaggio == "BUZZER") {
    Serial.println("🔊 Comando BUZZER ricevuto! Faccio casino per 2 secondi...");
    tone(BUZZER_PIN, 1000);
    delay(2000);
    noTone(BUZZER_PIN);
  }
}

void reconnect() {
  while (!client.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("Connessione MQTT...");
    String clientId = "ESP32-PillMate-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" OK!");
      client.subscribe(topic_comandi);
    } else {
      Serial.println(" Fallita. Riprovo...");
      delay(3000);
    }
  }
}

// ==========================================
//                  SETUP
// ==========================================

void setup() {
  Serial.begin(115200);

  // Inizializza Pin
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // Inizializza Display OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Errore Display!");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("Avvio PillMate...");
  display.display();

  // Inizializza Sensori e Motori
  dht.begin();
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // Connessione WiFi
  Serial.print("Connessione WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connesso!");

  // Sincronizza Ora esatta (NTP)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Configura MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ==========================================
//                  LOOP
// ==========================================

void loop() {
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    reconnect();
  }
  client.loop(); // Mantiene viva la connessione MQTT

  unsigned long oraAttuale = millis();

  // 1. Gestione Buzzer (Suona a intermittenza se l'allarme è attivo)
  if (allarmeAttivo) {
    if (oraAttuale - ultimoBip > 500) {
      ultimoBip = oraAttuale;
      statoBuzzer = !statoBuzzer;

      if (statoBuzzer) {
        tone(BUZZER_PIN, 1000); // Suona a 1000Hz
      } else {
        noTone(BUZZER_PIN);     // Pausa
      }
    }
  } else {
    noTone(BUZZER_PIN); // Sicurezza extra per assicurarsi che sia zitto
  }

  // 2. Lettura Sensori e Aggiornamento Display (Ogni 2 secondi)
  static unsigned long lastDisplay = 0;
  if (oraAttuale - lastDisplay > 2000) {
    t_letta = dht.readTemperature();
    h_letta = dht.readHumidity();
    aggiornaOLED();
    lastDisplay = oraAttuale;
  }

  // 3. Invio Telemetria MQTT (Ogni 10 secondi)
  static unsigned long lastMqtt = 0;
  if (oraAttuale - lastMqtt > 10000 && client.connected()) {
     if (!isnan(t_letta) && !isnan(h_letta)) {
       String json = "{\"temperatura\":" + String(t_letta) + ",\"umidita\":" + String(h_letta) + "}";
       client.publish(topic_sensori, json.c_str());
     }
     lastMqtt = oraAttuale;
  }

  // 4. GESTIONE PIR INTELLIGENTE (Logica Medica e Anti-Spam)
  int statoAttualePIR = digitalRead(PIR_PIN);

  // Se rileva un NUOVO movimento
  if (statoAttualePIR == HIGH && ultimoStatoPIR == LOW) {

    // Controlla che siano passati almeno 5 secondi dall'ultimo scatto
    if (oraAttuale - tempoUltimoScatto > 5000) {
      tempoUltimoScatto = oraAttuale;

      // Eroga SOLO se il medico ha inviato l'allarme (è l'ora della medicina)
      if (allarmeAttivo == true) {
        Serial.println("🖐️ Movimento rilevato e Allarme attivo: EROGO!");
        erogaPillola();
      } else {
        Serial.println("🖐️ Movimento rilevato, ma NON è l'ora della pillola. Ignoro.");
      }
    }
  }

  // Salva lo stato per il prossimo ciclo
  ultimoStatoPIR = statoAttualePIR;
}