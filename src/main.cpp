#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "disruptors.h"
#include <flag_utils.cpp>

const char *apSSID = "ESP32_Config";
const char *apPassword = "12345678";
AsyncWebServer server(80);

WiFiServer shellServer(23);
WiFiClient shellClient;

bool authenticated = false;

void setupAP() {
    WiFi.softAP(apSSID, apPassword);
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void handleWebRequests() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<form action='/setwifi' method='POST'>SSID: <input name='ssid'><br>Password: <input name='password' type='password'><br><input type='submit'></form>");
    });

    server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        String ssid = request->arg("ssid");
        String password = request->arg("password");
        if (!ssid.isEmpty() && !password.isEmpty()) {
            Serial.println("Received WiFi credentials:");
            Serial.println("SSID: " + ssid);
            Serial.println("Password: " + password);

            WiFi.disconnect(true); 

            WiFi.mode(WIFI_STA);
            WiFi.setSleep(false);
            WiFi.begin(ssid.c_str(), password.c_str());
            Serial.println("Connecting to WiFi...");

            unsigned long startAttemptTime = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
                delay(100);
                Serial.print(".");
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nConnected!");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
                
                server.end(); // Stop the web server
                shellServer.begin(); // Start the shell server
                Serial.println("Shell server started on port 23");

                request->send(200, "text/plain", "WiFi Connected. IP: " + WiFi.localIP().toString());
            } else {
                Serial.println("\nFailed to connect. Restarting AP mode.");
                setupAP();
                request->send(500, "text/plain", "Failed to connect to WiFi. Try again.");
            }
        } else {
            request->send(400, "text/plain", "Invalid input");
        }
    });
    server.begin();
}

void handleShell() {
    if (!shellClient || !shellClient.connected()) {
        shellClient = shellServer.available();
        if (shellClient) {
            Serial.println("New Shell Client Connected!");
            shellClient.println("Welcome to ESP32 Shell!");
            shellClient.print("Login: ");
        }
    }

    if (shellClient && shellClient.available()) {
        String input = shellClient.readStringUntil('\n');
        input.trim();
        Serial.println("Received: " + input);

        if (!authenticated) {
            if (input == "admin' OR '1'='1") {
                authenticated = true;
                shellClient.println("Login successful!");
                shellClient.print("> ");
            } else {
                shellClient.println("Login failed. Try again.");
                shellClient.print("Login: ");
            }
        } else {
            if (random(1, 10) <= 2) { // 20% chance to echo the command
                shellClient.println(input);
            } else {
                if (input == "help") {
                    shellClient.println("Available commands: help, status, reboot, change_ip");
                } else if (input == "status") {
                    shellClient.println("ESP32 is running.");
                } else if (input == "reboot") {
                    shellClient.println("Rebooting...");
                    delay(1000);
                    ESP.restart();
                } else if (input == "change_ip") {
                    shellClient.println("Changing IP...");
                    WiFi.disconnect();
                    delay(1000);
                    WiFi.reconnect();
                    unsigned long startAttemptTime = millis();
                    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
                        delay(100);
                        Serial.print(".");
                    }
                    if (WiFi.status() == WL_CONNECTED) {
                        Serial.println("\nConnected!");
                        Serial.print("New IP Address: ");
                        Serial.println(WiFi.localIP());
                        shellClient.println("New IP Address: " + WiFi.localIP().toString());
                        shellServer.begin(); // Restart TCP server
                    } else {
                        Serial.println("\nFailed to reconnect.");
                        shellClient.println("Failed to reconnect.");
                    }
                } else {
                    shellClient.println("Unknown command. Type 'help' for list of commands.");
                }
            }
            shellClient.print("> ");
        }
    }
}

void setup() {
    Serial.begin(115200);
    setupAP();
    handleWebRequests();
}

void loop() {
    handleShell(); // Обробка команд у shell
    disruptorTask(); // Виклик disruptorTask
}
