#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP32Servo.h>

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

// Variabile per ricordare lo stato precedente del sensore di movimento
int lastPirState = LOW;

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  // OLED
  Wire.begin(21, 22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Errore OLED");
    while(true);
  }

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
  myServo.write(0); // Assicuriamoci che parta da 0

  display.clearDisplay();
  display.setTextColor(1); // Colore bianco per il testo

  Serial.println("Sistema avviato");
}

// -------- LOOP --------
void loop() {

  // Lettura DHT
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Errore lettura DHT!");
    delay(2000);
    return;
  }

  // OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);

  display.print("Temp: ");
  display.print(temp);
  display.println(" C");

  display.print("Hum:  ");
  display.print(hum);
  display.println(" %");

  // --- PIR (Sensore Movimento) SCATTANTE ---
  int currentPirState = digitalRead(PIR_PIN);

  // Se rileva movimento ADESSO ma prima non c'era
  if (currentPirState == HIGH && lastPirState == LOW) {
    Serial.println("Movimento rilevato! Erogazione...");

    display.println("MOVIMENTO!");
    display.display(); // Aggiorna lo schermo subito

    digitalWrite(BUZZER, HIGH); // Suona
    myServo.write(90);          // Ruota il servo per far cadere la pillola

    delay(1000); // Aspetta ESATTAMENTE 1 secondo

    digitalWrite(BUZZER, LOW);  // Spegne il suono
    myServo.write(0);           // Torna indietro SUBITO
  }

  // Salva lo stato per il prossimo ciclo
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

  // Seriale
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print(" | Hum: ");
  Serial.println(hum);

  display.display();

  // Ridotto il ritardo generale per rendere i pulsanti più reattivi
  delay(100);
}