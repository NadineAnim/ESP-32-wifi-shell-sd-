#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "disruptors.h"

unsigned long lastInterrupt = 0;
bool flashActive = false;
unsigned long flashStart = 0;

extern WiFiClient shellClient;

void disruptorTask() {
    if (millis() - lastInterrupt > 60000) { // Кожні 60 секунд
        lastInterrupt = millis();
        
        int randomEvent = random(1, 6); // Випадковий ефект (додаємо флешку)
        switch (randomEvent) {
            case 1:
                if (shellClient && shellClient.connected()) {
                    shellClient.println("Warning: System integrity check failed!");
                } else {
                    Serial.println("Warning: System integrity check failed!");
                }
                break;
            case 2:
                if (shellClient && shellClient.connected()) {
                    shellClient.println("FAKE_FLAG{12345}");
                } else {
                    Serial.println("FAKE_FLAG{12345}");
                }
                break;
            case 3:
                if (shellClient && shellClient.connected()) {
                    shellClient.println("\033[2J\033[H"); // Очищення екрану
                } else {
                    Serial.println("\033[2J\033[H"); // Очищення екрану
                }
                break;
            case 4:
                if (shellClient && shellClient.connected()) {
                    shellClient.println("System will reboot in 30 seconds...");
                } else {
                    Serial.println("System will reboot in 30 seconds...");
                }
                break;
            case 5:
                if (shellClient && shellClient.connected()) {
                    shellClient.println("⚡ FLASHBANG ACTIVATED! ⚡");
                } else {
                    Serial.println("⚡ FLASHBANG ACTIVATED! ⚡");
                }
                flashActive = true;
                flashStart = millis();
                break;
        }
    }

    // Якщо флешка активна, засліплюємо екран
    if (flashActive) {
        if (millis() - flashStart < 5000) { // Засліплюємо на 5 секунд
            for (int i = 0; i < 10; i++) { // Reduce the amount of data being sent
                if (shellClient && shellClient.connected()) {
                    shellClient.println("██████████████████████████████████████████████████");
                } else {
                    Serial.println("██████████████████████████████████████████████████");
                }
            }
        } else {
            flashActive = false; 
            if (shellClient && shellClient.connected()) {
                shellClient.println("\033[2J\033[H");
                shellClient.println("System recovered from flashbang.");
            } else {
                Serial.println("\033[2J\033[H");
                Serial.println("System recovered from flashbang.");
            }
        }
    }
}
