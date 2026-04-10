#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 13
#define POT_PIN 34 // Pin analogico collegato al centrale del potenziometro

Servo myServo;

void setup() {
    Serial.begin(115200);

    // Configuro il servo
    myServo.setPeriodHertz(50);
    myServo.attach(SERVO_PIN, 500, 2400); // Impostazioni per servo MG90S a 270 gradi

    Serial.println("Test Potenziometro -> Servo Avviato!");
}

void loop() {
    // 1. Leggo il valore analogico del potenziometro (da 0 a 4095)
    int potValue = analogRead(POT_PIN);
    int angolo = 0;

    // 2. Spezzo la lettura nelle due metà (SX e DX)
    if (potValue < 2048) {
        // --- METÀ SINISTRA (SX) ---
        // Mappo il valore letto (0-2047) nei gradi della prima metà (0-135°)
        angolo = map(potValue, 0, 2047, 0, 135);

        Serial.print("Zona: SINISTRA (SX) | ");
    }
    else {
        // --- METÀ DESTRA (DX) ---
        // Mappo il valore letto (2048-4095) nei gradi della seconda metà (135-270°)
        angolo = map(potValue, 2048, 4095, 135, 270);

        Serial.print("Zona: DESTRA (DX)   | ");
    }

    // 3. Muovo il servo in base all'angolo calcolato
    myServo.write(angolo);

    // 4. Stampo i valori sul terminale per fare Debug
    Serial.print("Lettura Potenziometro: ");
    Serial.print(potValue);
    Serial.print(" | Angolo Servo: ");
    Serial.print(angolo);
    Serial.println("°");

    // Piccolo ritardo per non inondare il terminale di scritte
    delay(50);
}