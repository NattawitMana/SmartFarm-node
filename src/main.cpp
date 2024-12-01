#include <Wire.h>
#include <BH1750.h>
#include <esp_now.h>
#include <WiFi.h>

#define FC28_PIN 34

// Replace with the receiver's MAC address
uint8_t receiverMacAddress[] = {0xE0, 0xE2, 0xE6, 0x62, 0xF5, 0x68};

// Data structure to send
struct SensorData {
  int soilMoisture;  // Soil moisture sensor value
  float lightLevel;  // Light sensor (BH1750) value
};
SensorData sensorData;

// BH1750 instance
BH1750 lightMeter;

// Callback for when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Message sent" : "Message failed");
}

// Function to print ESP-NOW error codes
void printEspError(esp_err_t error) {
  switch (error) {
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESP-NOW not initialized");
      break;
    case ESP_ERR_ESPNOW_ARG:
      Serial.println("Invalid argument");
      break;
    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println("Internal error");
      break;
    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("Out of memory");
      break;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("Peer not found");
      break;
    case ESP_ERR_ESPNOW_IF:
      Serial.println("Interface error");
      break;
    default:
      Serial.print("Unknown error: ");
      Serial.println(error);
      break;
  }
}

// Function to print struct data
void printSensorData(const SensorData &data) {
  Serial.println("Sensor Data Struct:");
  Serial.print("  Soil Moisture: ");
  Serial.println(data.soilMoisture);
  Serial.print("  Light Level: ");
  Serial.println(data.lightLevel);
}

void setup() {
  Serial.begin(115200);

  // Set ESP32 to Station mode
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  // Register the peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;  // Default channel
  peerInfo.encrypt = false;  // No encryption for this example

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  } else {
    Serial.println("Peer added successfully");
  }

  // Configure GPIO34 as analog input for soil moisture sensor
  pinMode(FC28_PIN, INPUT);

  // Initialize BH1750 light sensor
  Wire.begin();
  if (!lightMeter.begin()) {
    Serial.println("Error initializing BH1750 sensor");
    while (true); // Halt execution if sensor fails
  }
  Serial.println("BH1750 initialized");
}

void loop() {
  // Read analog value from soil moisture sensor
  sensorData.soilMoisture = analogRead(FC28_PIN);  // Soil moisture sensor on GPIO34

  // Read light level from BH1750
  sensorData.lightLevel = lightMeter.readLightLevel();
  if (sensorData.lightLevel == -1.0) {
    Serial.println("Error reading light level from BH1750");
  }

  // Print the sensor data struct
  printSensorData(sensorData);

  // Attempt to send data
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t *)&sensorData, sizeof(sensorData));

  if (result == ESP_OK) {
    Serial.println("Data sent successfully");
  } else {
    Serial.println("Error sending data:");
    printEspError(result);

    // Retry logic
    int retryDelay = 500;  // Initial retry delay in milliseconds
    for (int i = 0; i < 3; i++) {  // Retry up to 3 times
      delay(retryDelay);
      result = esp_now_send(receiverMacAddress, (uint8_t *)&sensorData, sizeof(sensorData));
      if (result == ESP_OK) {
        Serial.println("Retry Success:");
        printSensorData(sensorData);
        break;
      } else {
        Serial.print("Retry failed, attempt #");
        Serial.println(i + 1);
      }
      retryDelay *= 2;  // Exponential backoff
    }

    if (result != ESP_OK) {
      Serial.println("Retry failed, giving up");
    }
  }

  delay(2000);  // Send data every 3 seconds
}