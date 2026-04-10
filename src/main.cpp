#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// -------- DATI WIFI --------
const char* ssid = "Uaifai";
const char* password = "bulangeu";

// -------- DATI MQTT (HIVEMQ PUBBLICO) --------
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// -------- I 3 TOPIC UFFICIALI --------
const char* topic_eventi = "pillmate/disp_01/eventi";
const char* topic_sensori = "pillmate/disp_01/telemetria";
const char* topic_comandi = "pillmate/disp_01/comandi";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long ultimoInvioSensori = 0;

// Il bottone fisico "BOOT" sulla scheda ESP32 è sul pin 0
#define BOOT_BTN 0
int lastBtnState = HIGH;

// --- FUNZIONE DI RICEZIONE COMANDI DAL WEB ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n[MQTT] Messaggio arrivato dal Web sul topic: ");
  Serial.println(topic);

  String messaggio = "";
  for (int i = 0; i < length; i++) {
    messaggio += (char)payload[i];
  }
  Serial.print("[MQTT] Comando ricevuto: ");
  Serial.println(messaggio);

  // Se il sito manda il comando "eroga_ora"
  if (messaggio == "eroga_ora") {
    Serial.println(">>> AZIONE: Il medico ha forzato l'erogazione da remoto! <<<");
    // Quando avrai collegato il servo, qui scriverai: myServo.write(90);
  }
}

// --- FUNZIONE PER CONNETTERSI AL SERVER MQTT ---
void reconnect() {
  while (!client.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("Tentativo di connessione a MQTT (HiveMQ)...");

    String clientId = "ESP32Client-TEST-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" ✅ Connesso a MQTT!");

      // APPENA CONNESSO, MI ISCRIVO AL CANALE DEI COMANDI
      client.subscribe(topic_comandi);

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

  // Imposto il bottone BOOT come input
  pinMode(BOOT_BTN, INPUT_PULLUP);

  // --- CONNESSIONE WIFI ---
  Serial.print("\nConnessione al WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connesso!");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());

  // --- CONFIGURO IL SERVER MQTT E L'ASCOLTO ---
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); // Dico all'ESP32 di usare la funzione callback

  Serial.println("Sistema di TEST avviato! In attesa di inviare/ricevere dati...");
}

// -------- LOOP --------
void loop() {
  // MANTIENI VIVA LA CONNESSIONE MQTT
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    reconnect();
  }
  client.loop(); // FONDAMENTALE per ricevere i messaggi in tempo reale

  // 🔥 INVIO DATI SENSORI(FINTI) DA MODIFICARE CON DATI REALI
  unsigned long tempoAttuale = millis();
  if (tempoAttuale - ultimoInvioSensori > 5000) {
    ultimoInvioSensori = tempoAttuale;

    if (client.connected()) {
      // Creo un pacchetto JSON finto
      String pacchettoJSON = "{\"temperatura\": 24.5, \"umidita\": 55.0}";

      client.publish(topic_sensori, pacchettoJSON.c_str());
      Serial.println("--> Telemetria inviata: " + pacchettoJSON);
    }
  }

  // --- SIMULAZIONE SENS. MOVIMENTO (Pulsante BOOT) ---
  int currentBtnState = digitalRead(BOOT_BTN);

  // Se premi il tasto BOOT (va LOW) e prima non lo era
  if (currentBtnState == LOW && lastBtnState == HIGH) {
    Serial.println("\nTasto BOOT premuto! Simulo l'erogazione...");

    if (client.connected()) {
      // Creo un pacchetto JSON per l'allarme
      String allarmeJSON = "{\"seriale\": \"disp_01\", \"azione\": \"pillola_erogata\"}";

      client.publish(topic_eventi, allarmeJSON.c_str());
      Serial.println("--> Messaggio Evento inviato!");
    }

    // Evita i rimbalzi del tasto
    delay(300);
  }
  lastBtnState = currentBtnState;

  delay(50);
}