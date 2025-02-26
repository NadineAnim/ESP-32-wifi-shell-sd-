#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "disruptors.h"
#include <flag_utils.cpp>
#include <esp_task_wdt.h>

// WiFi credentials
const char* apSSID = "ESP32_Config";
const char* apPassword = "12345678";

// Server variables
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Shell variables
bool authenticated = false;
String inputBuffer = "";
std::vector<String> commandHistory;
int historyIndex = -1;

// Operation mode
enum OperationMode { AP_MODE, STA_MODE };
OperationMode currentMode = AP_MODE;

void setupAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    IPAddress myIP = WiFi.softAPIP();
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(myIP);
    currentMode = AP_MODE;
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        String message = (char*)data;
        Serial.println("Received: " + message);

        inputBuffer += message;
        if (inputBuffer.endsWith("\r") || inputBuffer.endsWith("\n")) {
            inputBuffer.trim();
            Serial.println("Command: " + inputBuffer);

            if (!authenticated) {
                if (inputBuffer == "admin' OR '1'='1") {
                    authenticated = true;
                    ws.textAll("Login successful!");
                } else {
                    ws.textAll("Login failed. Try again.");
                }
            } else {
                commandHistory.push_back(inputBuffer);
                historyIndex = commandHistory.size();
                
                if (inputBuffer == "help") {
                    ws.textAll("Available commands: help, status, reboot, change_ip");
                } else if (inputBuffer == "status") {
                    String status = "ESP32 is running.\n";
                    status += "WiFi Mode: " + String(currentMode == AP_MODE ? "Access Point" : "Station") + "\n";
                    status += "IP Address: " + (currentMode == AP_MODE ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
                    ws.textAll(status);
                } else if (inputBuffer == "reboot") {
                    ws.textAll("Rebooting...");
                    delay(1000);
                    ESP.restart();
                } else if (inputBuffer == "change_ip") {
                    if (currentMode == AP_MODE) {
                        ws.textAll("Cannot change IP in AP mode. Connect to WiFi first.");
                    } else {
                        ws.textAll("Changing IP...");
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
                            ws.textAll("New IP Address: " + WiFi.localIP().toString());
                        } else {
                            Serial.println("\nFailed to reconnect.");
                            ws.textAll("Failed to reconnect.");
                        }
                    }
                } else {
                    ws.textAll("Unknown command. Type 'help' for list of commands.");
                }
            }
            inputBuffer = ""; // Clear buffer
        }
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            client->text("Welcome to ESP32 WebSocket Shell!");
            client->text("Login: ");
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_ERROR:
            Serial.println("WebSocket error");
            break;
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    
    // Configure watchdog
    esp_task_wdt_init(30, false);
    
    // Start in AP mode initially
    setupAP();
    
    // Configure server routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (currentMode == AP_MODE) {
            request->send(SPIFFS, "/index.html", String(), false);
        } else {
            request->send(SPIFFS, "/shell.html", String(), false);
        }
    });

    server.on("/wifi.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (currentMode == AP_MODE) {
            request->send(SPIFFS, "/wifi.html", String(), false);
        } else {
            request->send(403, "text/plain", "WiFi configuration is not accessible in station mode.");
        }
    });

    server.on("/shell.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/shell.html", String(), false);
    });

    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "ESP32 is online");
    });

    server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        String ssid = request->arg("ssid");
        String password = request->arg("password");
        
        if (!ssid.isEmpty()) {
            Serial.println("Received WiFi credentials:");
            Serial.println("SSID: " + ssid);
            Serial.println("Password: [provided]");
            
            // Send an immediate response to prevent browser timeout
            request->send(200, "text/plain", "Connecting to " + ssid + "...");
            
            // Now handle WiFi connection separately from the HTTP response
            delay(1000);  // Give time for the response to be sent
            
            // Configure WiFi station mode
            WiFi.disconnect(true);
            delay(1000);
            
            // Shut down AP mode
            WiFi.softAPdisconnect(true);
            delay(1000);
            
            // Connect to WiFi
            WiFi.mode(WIFI_STA);
            WiFi.setSleep(false);
            WiFi.begin(ssid.c_str(), password.c_str());
            
            Serial.println("Connecting to WiFi...");
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
                delay(500);
                Serial.print(".");
                attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nConnected!");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
                currentMode = STA_MODE;
                
                // Critical: Force the server to listen on all interfaces
                // This is done by temporarily shutting down and restarting the server
                server.end();
                ws.cleanupClients();
                delay(1000);
                
                // Restart the server and WebSocket
                server.begin();
                Serial.println("WebSocket server restarted");
            } else {
                Serial.println("\nFailed to connect! Reverting to AP mode.");
                setupAP();
            }
        } else {
            request->send(400, "text/plain", "Invalid input");
        }
    });

    server.serveStatic("/", SPIFFS, "/");
    
    // Setup WebSocket
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    
    // Start the server
    server.begin();
    Serial.println("Web Server started");
}

void loop() {
    // Maintain WebSocket connections
    ws.cleanupClients();
    
    // Check WiFi connection status periodically
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000) {  // Every 10 seconds
        lastCheck = millis();
        
        if (currentMode == STA_MODE && WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost! Attempting to reconnect...");
            WiFi.reconnect();
            
            // Wait for reconnection
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                Serial.print(".");
                attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nReconnected to WiFi!");
            } else {
                Serial.println("\nFailed to reconnect. Switching to AP mode.");
                setupAP();
                
                // Restart server to ensure it binds to AP IP
                server.end();
                ws.cleanupClients();
                delay(1000);
                server.begin();
            }
        }
    }
    
    // Run disruptor task if connected
    if (currentMode == STA_MODE && WiFi.status() == WL_CONNECTED) {
        disruptorTask();
    }
    
    // Reset watchdog timer
    esp_task_wdt_reset();
    
    // Short delay
    delay(10);
}