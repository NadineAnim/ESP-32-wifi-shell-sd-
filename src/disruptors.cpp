#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "disruptors.h"

unsigned long lastInterrupt = 0;
bool flashActive = false;
unsigned long flashStart = 0;

extern AsyncWebSocket ws;

void disruptorTask() {
    if (millis() - lastInterrupt > 60000) { // Every 60 seconds
        lastInterrupt = millis();
        
        int randomEvent = random(1, 6); // Random effect (including flashbang)
        switch (randomEvent) {
            case 1:
                ws.textAll("Warning: System integrity check failed!");
                break;
            case 2:
                ws.textAll("FAKE_FLAG{12345}");
                break;
            case 3:
                ws.textAll("\033[2J\033[H"); // Clear screen
                break;
            case 4:
                ws.textAll("System will reboot in 30 seconds...");
                break;
            case 5:
                ws.textAll("⚡ FLASHBANG ACTIVATED! ⚡");
                flashActive = true;
                flashStart = millis();
                break;
        }
    }

    // If flashbang is active, blind the screen
    if (flashActive) {
        if (millis() - flashStart < 5000) { // Blind for 5 seconds
            ws.textAll("\033[48;5;15m\033[2J\033[H"); // White background
        } else {
            flashActive = false; 
            ws.textAll("\033[0m\033[2J\033[H"); // Reset terminal
            ws.textAll("System recovered from flashbang.");
        }
    }
}
