#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Function Prototypes
void handleConfigurationPage();
void handleWiFiConfig();
String scanNetworks();
void sendSensorData();
bool checkFirebaseConnection();

// Constants and Definitions
#define DEBUG_MODE                                      true

#if DEBUG_MODE
  #define DEBUG_PRINT(message_to_print) Serial.println(message_to_print)
#else
  #define DEBUG_PRINT(message_to_print)
#endif

#define ESP_BAUDRATE                                    115200
#define WIFI_SCAN_CACHE_DURATION_MS                     30000
#define SEND_DATA_INTERVAL_MS                           2000  // Data sending interval

#define CONFIG_NETWORK_SSID                            "Dr_Soil"
#define CONFIG_NETWORK_PASSWORD                        "12345678"

#define API_KEY                                        "AIzaSyDNg8jWhN7XnI7l8Oq0_A-I7HuupMChTXU"
#define DATABASE_URL                                   "https://dr-soil-db-default-rtdb.firebaseio.com/"

// HTML content stored as a constant string in flash memory to save RAM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Configuration</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #f2f2f2;
            text-align: center;
        }
        h1 {
            color: #333;
            margin-top: 50px;
            font-size: 2em;
        }
        form {
            background-color: #fff;
            padding: 30px;
            margin: 20px auto;
            width: 400px;
            border-radius: 10px;
            box-shadow: 0px 0px 15px 0px #aaa;
        }
        select, input[type="password"], input[type="submit"] {
            width: 90%;
            padding: 15px;
            margin-top: 10px;
            margin-bottom: 20px;
            border-radius: 5px;
            border: 1px solid #ccc;
            font-size: 18px;
        }
        input[type="submit"] {
            background-color: #4CAF50;
            color: white;
            border: none;
            cursor: pointer;
        }
        input[type="submit"]:hover {
            background-color: #45a049;
        }
        p {
            color: red;
            font-size: 1.2em;
        }
    </style>
</head>
<body>
    <h1>WiFi Configuration</h1>
    <form action="/wifi" method="post">
        <div id="networks">
            <!-- Wi-Fi networks will be populated here -->
        </div>
        <label for="password">Password:</label>
        <input type="password" id="password" name="password"><br>
        <input type="submit" value="Submit">
    </form>
</body>
</html>
)rawliteral";

// Create a web server object on port 80
ESP8266WebServer server(80);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool configuration_mode = true;
bool islogged_to_fireBase = false;

String cachedNetworks = "";
unsigned long lastScanTime = 0;
unsigned long sendDataPrevMillis = 0;

void setup() {
    #if DEBUG_MODE
        Serial.begin(ESP_BAUDRATE);
    #endif

    WiFi.softAP(CONFIG_NETWORK_SSID, CONFIG_NETWORK_PASSWORD);
    DEBUG_PRINT("Access Point Started");

    server.on("/", handleConfigurationPage);
    server.on("/wifi", handleWiFiConfig);
    server.onNotFound([]() {
        server.send(404, "text/plain", "404: Not Found");
    });

    server.begin();
    DEBUG_PRINT("HTTP server started");
}

void loop() {
    if (configuration_mode) {
        server.handleClient();

    } else if (!islogged_to_fireBase) {
        
      config.api_key = API_KEY;
      config.database_url = DATABASE_URL;

      if(Firebase.signUp(&config, &auth, "", "")) {
        DEBUG_PRINT("SignUp Successful");
        config.token_status_callback = tokenStatusCallback;
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
      
        islogged_to_fireBase = 1;
      } else {
        Serial.printf("%s\n", config.signer.signupError.message.c_str());
      }
    
    } else {
        
        unsigned long currentMillis = millis();
        if (currentMillis - sendDataPrevMillis >= SEND_DATA_INTERVAL_MS) {
            sendDataPrevMillis = currentMillis;
            sendSensorData();
        }

    }
}

void handleConfigurationPage() {
    unsigned long currentTime = millis();

    if (currentTime - lastScanTime > WIFI_SCAN_CACHE_DURATION_MS || cachedNetworks == "") {
        cachedNetworks = scanNetworks();
        lastScanTime = currentTime;
    }

    String html = FPSTR(index_html);
    html.replace("<!-- Wi-Fi networks will be populated here -->", cachedNetworks);

    server.send(200, "text/html", html);

    if (currentTime - lastScanTime > WIFI_SCAN_CACHE_DURATION_MS) {
        cachedNetworks = "";
    }
}

void handleWiFiConfig() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
        String ssid = server.arg("ssid");
        String password = server.arg("password");

        DEBUG_PRINT("SSID: " + ssid);
        DEBUG_PRINT("Password: " + password);

        WiFi.begin(ssid.c_str(), password.c_str());

        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            DEBUG_PRINT("Connected to Wi-Fi");

            WiFi.softAPdisconnect(true);
            DEBUG_PRINT("AP mode disabled.");
            server.stop();
            configuration_mode = false;
            DEBUG_PRINT("Web server stopped.");

            server.send_P(200, "text/html", PSTR("<!DOCTYPE html><html><body><h1>Connected successfully!</h1><p>ESP8266 will now begin its main operation.</p></body></html>"));
        } else {
            DEBUG_PRINT("Failed to connect. Check password and try again.");
            server.send_P(200, "text/html", PSTR("<!DOCTYPE html><html><body><h1>Connection failed!</h1><p>Please check your SSID and password, and try again.</p></body></html>"));
        }
    }
}

String scanNetworks() {
    char ssid[32];
    char networkOptions[1024];

    strcpy(networkOptions, "<select name=\"ssid\">");

    int numberNetworks_Scanned = WiFi.scanNetworks();
    if (numberNetworks_Scanned == 0) {
        strcat(networkOptions, "<option value=\"none\">No networks found</option>");
    } else {
        for (int i = 0; i < numberNetworks_Scanned; i++) {
            WiFi.SSID(i).toCharArray(ssid, sizeof(ssid));
            const char* securityType = (WiFi.encryptionType(i) == AUTH_OPEN) ? " (Open)" : " (Secured)";

            char networkOption[128];
            snprintf(networkOption, sizeof(networkOption), "<option value=\"%s\">%s (%d dBm)%s</option>", ssid, ssid, WiFi.RSSI(i), securityType);
            strcat(networkOptions, networkOption);
        }
    }
    strcat(networkOptions, "</select>");
    return String(networkOptions);
}

bool checkFirebaseConnection() {
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    if (Firebase.signUp(&config, &auth, "", "")) {
        DEBUG_PRINT("SignUp Successful");
        config.token_status_callback = tokenStatusCallback;
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        return true;
    } else {
        Serial.printf("%s\n", config.signer.signupError.message.c_str());
        return false;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sendSensorData() {
    float ph = generate_ph();
    float soil_humidity = generate_soil_humidity();
    float temperature = generate_temperature();
    float soil_conductivity = generate_soil_conductivity();
    float soil_nitrogen = generate_soil_nitrogen();
    float soil_phosphorus = generate_soil_phosphorus();
    float soil_potassium = generate_soil_potassium();

    if (Firebase.RTDB.setFloat(&fbdo, "Sensor/pH", ph) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/soil_humidity", soil_humidity) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/temperature", temperature) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/soil_conductivity", soil_conductivity) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/soil_nitrogen", soil_nitrogen) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/soil_phosphorus", soil_phosphorus) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/soil_potassium", soil_potassium)) {
        DEBUG_PRINT("Data has been sent successfully");
    } else {
        DEBUG_PRINT("FAILED: " + fbdo.errorReason());
    }
}

// Functions to generate random sensor data
float generate_ph() {
    return random(350, 900) / 100.0;
}

float generate_soil_humidity() {
    return random(1000, 9000) / 100.0;
}

float generate_temperature() {
    return random(1000, 3500) / 100.0;
}

float generate_soil_conductivity() {
    return random(10, 300) / 100.0;
}

float generate_soil_nitrogen() {
    return random(100, 5000) / 100.0;
}

float generate_soil_phosphorus() {
    return random(500, 8000) / 100.0;
}

float generate_soil_potassium() {
    return random(5000, 40000) / 100.0;
}
