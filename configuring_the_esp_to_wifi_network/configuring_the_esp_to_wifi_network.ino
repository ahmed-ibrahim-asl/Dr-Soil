#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Create a web server object on port 80
ESP8266WebServer server(80);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
            width: 100%;
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEBUG_MODE                                      true

#if DEBUG_MODE
  #define DEBUG_PRINT(message_to_print) Serial.println(message_to_print)
#else
  #define DEBUG_PRINT(message_to_print)
#endif

#define ESP_BAUDRATE                                    115200
#define WIFI_SCAN_CACHE_DURATION_MS                     30000

// SSID and password for the configuration network (AP mode)
#define CONFIG_NETWORK_SSID                            "ESP_AP"
#define CONFIG_NETWORK_PASSWORD                        "12345678"

/**
 * Cached Wi-Fi networks scan results, to reduce the need for repeated scans 
 */
String cachedNetworks = "";
unsigned long lastScanTime = 0;
bool  configuration_mode=1;

// Function Prototypes
void ICACHE_FLASH_ATTR handleConfigurationPage();
void handleWiFiPage(const char* message = nullptr);
String ICACHE_FLASH_ATTR scanNetworks();
void handleWiFiConfig();

void setup(){
  #if DEBUG_MODE
    // Initialize serial communication for debugging
    Serial.begin(ESP_BAUDRATE);
  #endif
  
  // Start the configuration network (AP mode)
  WiFi.softAP(CONFIG_NETWORK_SSID, CONFIG_NETWORK_PASSWORD);
  DEBUG_PRINT("Access Point Started");

  // Start the web server that hosts the configuration page 
  server.on("/", handleConfigurationPage);
  server.on("/wifi", handleWiFiConfig);
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
  
  server.begin();
  DEBUG_PRINT("HTTP server started");
}

void loop(){
  // Handle incoming client requests for the web server
  
  if(configuration_mode){
    server.handleClient();
  
  } else {
    DEBUG_PRINT("Operation Mode");

  }

  
  
}

void ICACHE_FLASH_ATTR handleConfigurationPage() {
    unsigned long currentTime = millis();
  
    // Check if the cache is expired or empty
    if (currentTime - lastScanTime > WIFI_SCAN_CACHE_DURATION_MS || cachedNetworks == "") {
        cachedNetworks = scanNetworks();
        lastScanTime = currentTime;
    }

    // Handle the Wi-Fi configuration page
    String html = FPSTR(index_html);
    html.replace("<!-- Wi-Fi networks will be populated here -->", cachedNetworks);

    // Send the configuration page to the client
    server.send(200, "text/html", html);

    // Clear cache if expired
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

        // Attempt to connect to the selected Wi-Fi network
        WiFi.begin(ssid.c_str(), password.c_str());

        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            DEBUG_PRINT("Connected to Wi-Fi");

            // Disable the AP mode after successful connection
            WiFi.softAPdisconnect(true);
            DEBUG_PRINT("AP mode disabled.");
            // Stop the web server since we no longer need it
            server.stop();
            configuration_mode=0;
            DEBUG_PRINT("Web server stopped.");
            // Inform the user of success
            
            // Use send_P to send static success message
            server.send_P(200, "text/html", PSTR("<!DOCTYPE html><html><body><h1>Connected successfully!</h1><p>ESP8266 will now begin its main operation.</p></body></html>"));

            return; // Exit the function
        } else {
            DEBUG_PRINT("Failed to connect. Check password and try again.");
            // You could add an error message directly in the HTML if needed
            server.send_P(200, "text/html", PSTR("<!DOCTYPE html><html><body><h1>Connection failed!</h1><p>Please check your SSID and password, and try again.</p></body></html>"));
        }
    }
}

const char* networkOptionTemplate = "<option value=\"%s\">%s (%d dBm)%s</option>";
String ICACHE_FLASH_ATTR scanNetworks() {
    char ssid[32];
    char networkOptions[1024];  // Adjust size based on expected content

    strcpy(networkOptions, "<select name=\"ssid\">");

    int numberNetworks_Scanned = WiFi.scanNetworks();
    if (numberNetworks_Scanned == 0) {
        strcat(networkOptions, "<option value=\"none\">No networks found</option>");
    } else {
        for (int i = 0; i < numberNetworks_Scanned; i++) {
            WiFi.SSID(i).toCharArray(ssid, sizeof(ssid));
            const char* securityType = (WiFi.encryptionType(i) == AUTH_OPEN) ? " (Open)" : " (Secured)";

            char networkOption[128];
            snprintf(networkOption, sizeof(networkOption), networkOptionTemplate, ssid, ssid, WiFi.RSSI(i), securityType);
            strcat(networkOptions, networkOption);
        }
    }
    strcat(networkOptions, "</select>");
    return String(networkOptions);
}
