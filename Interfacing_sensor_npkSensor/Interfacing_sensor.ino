#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <pgmspace.h>

// Define WiFi credentials
#define WIFI_SSID "Home"
#define WIFI_PASSWORD "123456789"

// Define Firebase API Key, Project ID, and user credentials
#define API_KEY "AIzaSyArxIp_XP4sT-qDI6WssXEqFwiX-wmB6z0"
#define FIREBASE_PROJECT_ID "dr-soil-ee1b6"
#define USER_EMAIL "aryantfk5@gmail.com"
#define USER_PASSWORD "zxcvbnm123"

#define baudRate 115200
#define utcOffsetInSeconds 3600

// Firebase objects and configuration
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Create a FirebaseJson object to store the sensor data
FirebaseJson content;

// Store the days of the week in flash memory (PROGMEM)
const char daysOfTheWeek[7][4] PROGMEM = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char day[4];

char documentPath[50];  // Pre-allocate memory to avoid dynamic allocation issues

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Function to print free heap memory
void printHeap() {
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
}

void setup() {
    Serial.begin(baudRate);
    timeClient.begin();

    // Connect to Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // Print Firebase client version
    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    // Assign the API key
    config.api_key = API_KEY;

    // Assign the user sign-in credentials
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    // Token status callback
    config.token_status_callback = tokenStatusCallback;

    // Initialize Firebase
    Firebase.begin(&config, &auth);

    // Enable Wi-Fi reconnection
    Firebase.reconnectWiFi(true);

    // Print initial heap size
    printHeap();
}

void loop() {
    // Update time from NTP
    timeClient.update();

    // Get day of the week from PROGMEM
    strncpy_P(day, daysOfTheWeek[timeClient.getDay()], sizeof(day));

    // Build document path without using String class
    snprintf(documentPath, sizeof(documentPath), "SensorData/%s, %02d:%02d", 
             day, timeClient.getHours(), timeClient.getMinutes());

    // Set sensor fields
    content.set("fields/Nitrogen/doubleValue", 23.0);
    content.set("fields/Phosphorous/doubleValue", 5.0);
    content.set("fields/Potassium/doubleValue", 15.5);
    content.set("fields/pH/doubleValue", 7.1);
    content.set("fields/Conductivity/doubleValue", 100.5);

    Serial.print("Storing Sensor Data... ");

    // Use the createDocument method to add a new document for each reading
    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath, content.raw())) {
        Serial.println("Success!");
        Serial.println(fbdo.payload());  // Print Firestore response payload
    } else {
        Serial.println(fbdo.errorReason());  // Print error if there's an issue
    }

    // Print heap size after storing data
    printHeap();

    delay(60000);  // Store readings every 60 seconds
}
