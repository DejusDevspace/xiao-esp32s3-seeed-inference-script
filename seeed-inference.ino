#include "WiFi.h"
#include "esp_camera.h"
#include "HTTPClient.h"
#include "ArduinoJson.h"

// WiFi credentials
const char* ssid = "Royalty";
const char* password = "nawaoooo";

// API endpoint - Replace with your computer's local IP
const char* apiUrl = "http://192.168.208.129:8000/predict";  // Update this IP address!

// Camera pins for XIAO ESP32S3 Sense
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// LED pin and timing
#define LED_PIN           21 // Built-in LED
#define CAPTURE_INTERVAL  15000  // 15 seconds in milliseconds

// You can easily change the capture interval by modifying CAPTURE_INTERVAL above:
// 5 seconds:  5000
// 10 seconds: 10000  
// 15 seconds: 15000 (current)
// 30 seconds: 30000
// 1 minute:   60000

void setup() {
  Serial.begin(115200);
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera initialization failed!");
    return;
  }
  
  // Connect to WiFi
  connectToWiFi();
  
  Serial.println("System ready! Capturing images every 15 seconds...");
  blinkLED(3); // Ready indicator
}

void loop() {
  static unsigned long lastCaptureTime = 0;
  unsigned long currentTime = millis();
  
  // Check if it's time to capture an image
  if (currentTime - lastCaptureTime >= CAPTURE_INTERVAL) {
    Serial.println("Time for automatic capture...");
    digitalWrite(LED_PIN, HIGH); // Indicate processing
    
    captureAndClassify();
    
    digitalWrite(LED_PIN, LOW);
    lastCaptureTime = currentTime;
    
    // Print countdown until next capture
    Serial.printf("Next capture in %d seconds\n", CAPTURE_INTERVAL / 1000);
  }
  
  // Print countdown every 5 seconds
  static unsigned long lastCountdownTime = 0;
  if (currentTime - lastCountdownTime >= 5000) {
    unsigned long timeUntilNext = CAPTURE_INTERVAL - (currentTime - lastCaptureTime);
    if (timeUntilNext > 1000) { // Only show if more than 1 second remaining
      Serial.printf("Next capture in %lu seconds...\n", timeUntilNext / 1000);
    }
    lastCountdownTime = currentTime;
  }
  
  delay(1000); // Check every second
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Frame size and quality settings
  config.frame_size = FRAMESIZE_VGA; // 640x480
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }
  
  // Adjust camera settings for better image quality
  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  
  return true;
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway IP: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.println();
    Serial.println("Make sure your API server is running on the same network!");
    Serial.print("API URL configured as: ");
    Serial.println(apiUrl);
    
    // Test connectivity to API server
    testApiConnection();
  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");
  }
}

void testApiConnection() {
  Serial.println("Testing API connection...");
  HTTPClient http;
  
  // Extract base URL from apiUrl
  String baseUrl = String(apiUrl);
  int lastSlash = baseUrl.lastIndexOf('/');
  if (lastSlash > 0) {
    baseUrl = baseUrl.substring(0, lastSlash);
  }
  
  http.begin(baseUrl);
  http.setTimeout(5000); // 5 second timeout
  
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.printf("✓ API server is reachable! Response code: %d\n", httpResponseCode);
    Serial.println("Response: " + http.getString());
  } else {
    Serial.printf("✗ Cannot reach API server. Error code: %d\n", httpResponseCode);
    Serial.println("Check that:");
    Serial.println("1. Your API server is running");
    Serial.println("2. The IP address in apiUrl is correct");
    Serial.println("3. Both devices are on the same WiFi network");
    Serial.println("4. No firewall is blocking the connection");
  }
  
  http.end();
  Serial.println();
}

void captureAndClassify() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected! Attempting to reconnect...");
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      blinkLED(5); // Error indicator
      return;
    }
  }
  
  Serial.printf("Capturing image at %lu ms...\n", millis());
  
  // Capture image
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    blinkLED(5); // Error indicator
    return;
  }
  
  Serial.printf("Image captured: %d bytes\n", fb->len);
  
  // Send to API
  HTTPClient http;
  http.begin(apiUrl);
  http.setTimeout(10000); // 10 second timeout
  
  // Create multipart form data
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);
  
  // Build multipart body header
  String bodyHeader = "";
  bodyHeader += "--" + boundary + "\r\n";
  bodyHeader += "Content-Disposition: form-data; name=\"file\"; filename=\"esp32_capture.jpg\"\r\n";
  bodyHeader += "Content-Type: image/jpeg\r\n\r\n";
  
  String bodyFooter = "\r\n--" + boundary + "--\r\n";
  
  // Calculate total content length
  int contentLength = bodyHeader.length() + fb->len + bodyFooter.length();
  http.addHeader("Content-Length", String(contentLength));
  
  // Create the complete body
  String completeBody = bodyHeader + String((char*)fb->buf, fb->len) + bodyFooter;
  
  // Send POST request
  int httpResponseCode = http.POST(completeBody);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("HTTP Response: %d\n", httpResponseCode);
    Serial.println("Response: " + response);
    
    // Parse JSON response
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc.containsKey("prediction")) {
      String prediction = doc["prediction"];
      float confidence = doc.containsKey("confidence") ? doc["confidence"] : 0.0;
      
      Serial.printf("Prediction: %s (%.2f%% confidence)\n", prediction.c_str(), confidence * 100);
      
      // Visual feedback based on prediction
      if (prediction == "circle") {
        Serial.println("🔵 Circle detected!");
        blinkPattern(1); // 1 blink for circle
      } else if (prediction == "rectangle") {
        Serial.println("🔲 Rectangle detected!");
        blinkPattern(2); // 2 blinks for rectangle
      }
    } else if (doc.containsKey("error")) {
      Serial.println("API Error: " + String((const char*)doc["error"]));
      blinkLED(10); // Long error indicator
    } else {
      Serial.println("Unexpected response format");
      blinkLED(7); // Medium error indicator
    }
  } else {
    Serial.printf("HTTP Error: %d\n", httpResponseCode);
    Serial.println("Failed to connect to API server");
    blinkLED(5); // Error indicator
  }
  
  http.end();
  esp_camera_fb_return(fb);
  
  Serial.println("Capture and classification complete.\n");
}

void blinkLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

void blinkPattern(int pattern) {
  for (int i = 0; i < pattern; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    delay(300);
  }
  delay(1000);
  
  // Confirmation blink
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}