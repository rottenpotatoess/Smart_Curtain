#define BLYNK_TEMPLATE_ID "YourBlynkTempletID"   // Replace with your Template ID
#define BLYNK_TEMPLATE_NAME "Curtain"       // Replace with your Template Name
#define BLYNK_AUTH_TOKEN "YourBlynkToken" // Replace with your Auth Token

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Access Point credentials for manual Wi-Fi configuration
const char* ssid_ap = "Smart_Curtain";
const char* password_ap = "123456789";

// Motor control pins for ESP32
#define IN1 5 // GPIO5
#define IN2 4 // GPIO4

// Light sensor pin (digital input from LDR)
#define LIGHT_SENSOR_PIN 34 // Use any digital GPIO pin

// LED pins for Wi-Fi connection status
#define RED_LED_PIN 26    // GPIO26 for RED LED
#define GREEN_LED_PIN 27  // GPIO27 for GREEN LED

// Blynk Virtual Pins
#define FORWARD_BUTTON V1
#define BACKWARD_BUTTON V2

// State variables
bool userOverride = false;
bool motorRunning = false;
bool lightDetected = false;
bool wifiConnected = false;
bool isConfigured = false;

// Timer variables
unsigned long motorStartTime = 0;
const unsigned long motorRunDuration = 5000;  // Motor runs for 5 seconds

// Wi-Fi credentials (to be set by the user via the web interface)
String input_SSID;
String input_PASSWORD;

// Web server for AP mode
AsyncWebServer server(80);

// HTML content for the Wi-Fi setup form
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
 <title>ESP Wi-Fi Config</title>
 <meta name="viewport" content="width=device-width, initial-scale=1">
 </head><body>
 <form action="/get">
  <p>Wi-Fi SSID: <input type="text" name="SSID"></p>
  <p>Password: <input type="text" name="password"></p>
  <p><input type="submit" value="Submit"></p>
 </form>
</body></html>)rawliteral";

void setup() {
  // Start serial communication
  Serial.begin(115200);

  // Initialize motor control pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  // Initialize light sensor pin
  pinMode(LIGHT_SENSOR_PIN, INPUT);

  // Initialize LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Turn on RED LED initially (not connected to Wi-Fi)
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);

  // Configure ESP32 as both AP and STA
  WiFi.mode(WIFI_MODE_APSTA);

  // Start Access Point
  WiFi.softAP(ssid_ap, password_ap);
  Serial.println("Access Point started.");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Serve the Wi-Fi setup form
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Handle form submission for Wi-Fi credentials
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("SSID") && request->hasParam("password")) {
      input_SSID = request->getParam("SSID")->value();
      input_PASSWORD = request->getParam("password")->value();
      Serial.println("SSID: " + input_SSID);
      Serial.println("Password: " + input_PASSWORD);

      // Attempt to connect to Wi-Fi
      WiFi.begin(input_SSID.c_str(), input_PASSWORD.c_str());
      isConfigured = true;
      request->send(200, "text/html", "<h1>Trying to connect to Wi-Fi...</h1>");
    } else {
      request->send(400, "text/plain", "Missing parameters.");
    }
  });

  // Start the web server
  server.begin();
}

// Forward button handler for Blynk
BLYNK_WRITE(FORWARD_BUTTON) {
  int buttonState = param.asInt(); // Get button state (1: pressed, 0: released)
  userOverride = buttonState;     // Set user override when the button is used
  if (buttonState) {
    motorRunning = false; // Stop automatic control
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    Serial.println("Motor moving forward (User override)");
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    Serial.println("Motor stopped (User override)");
  }
}

// Backward button handler for Blynk
BLYNK_WRITE(BACKWARD_BUTTON) {
  int buttonState = param.asInt(); // Get button state (1: pressed, 0: released)
  userOverride = buttonState;     // Set user override when the button is used
  if (buttonState) {
    motorRunning = false; // Stop automatic control
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    Serial.println("Motor moving backward (User override)");
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    Serial.println("Motor stopped (User override)");
  }
}

// Automatic curtain control based on light sensor
void handleLightSensor() {
  int lightState = digitalRead(LIGHT_SENSOR_PIN);

  // If the user has not overridden the system and the motor is not running
  if (!userOverride && !motorRunning) {
    if (lightState == HIGH && !lightDetected) {
      // Light detected: Start motor to open curtain
      lightDetected = true;
      motorRunning = true;
      motorStartTime = millis();
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      Serial.println("Light detected: Opening curtain for 5 seconds");
      delay(5000);
    } else if (lightState == LOW && lightDetected) {
      // Low light detected: Start motor to close curtain
      lightDetected = false;
      motorRunning = true;
      motorStartTime = millis();
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      Serial.println("Low light detected: Closing curtain for 5 seconds");
      delay(5000);
    }
  }

  // Stop motor after 5 seconds
  if (motorRunning && (millis() - motorStartTime >= motorRunDuration)) {
    motorRunning = false;
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    Serial.println("Motor stopped after 5 seconds");
  }
}

void updateLEDStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(RED_LED_PIN, LOW);  // Turn off RED LED
    digitalWrite(GREEN_LED_PIN, HIGH); // Turn on GREEN LED
  } else {
    digitalWrite(RED_LED_PIN, HIGH);  // Turn on RED LED
    digitalWrite(GREEN_LED_PIN, LOW); // Turn off GREEN LED
  }
}

void loop() {
  // Handle Wi-Fi connection
  if (isConfigured && WiFi.status() == WL_CONNECTED && !wifiConnected) {
    Serial.println("Connected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;

    // Start Blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, input_SSID.c_str(), input_PASSWORD.c_str());
  }

  // Update LED status
  updateLEDStatus();

  // Run Blynk
  if (wifiConnected) {
    Blynk.run();
    handleLightSensor();
  }
}
