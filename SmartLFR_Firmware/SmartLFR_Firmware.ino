#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Wire.h>

// Include our beautiful HTML frontend
#include "webapp.h"

// ==========================================
// 📍 HARDWARE PIN DEFINITIONS
// ==========================================
// Left Motor (L293D)
const int EN_LEFT = 25;
const int IN1_LEFT = 26;
const int IN2_LEFT = 27;

// Right Motor (L293D)
const int EN_RIGHT = 13;
const int IN3_RIGHT = 14;
const int IN4_RIGHT = 32;

// HC-SR04 Sonar
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// LM393 Encoders
const int ENC_LEFT = 34;
const int ENC_RIGHT = 35;

// Warning LED (turns ON when obstacle detected)
const int LED_OBSTACLE = 2; // D2 (GPIO 2) - wire through a 330 ohm resistor to GND

// MPU6050 Gyro uses default I2C pins: SDA (21), SCL (22)

// ==========================================
// 🌍 GLOBALS & OBJECTS
// ==========================================
AsyncWebServer server(80);

// MPU6050 Raw I2C Setup
const int MPU_ADDR = 0x68;
int16_t gyro_x, gyro_y, gyro_z;

// Encoder Counters (Must be volatile since they change in interrupts)
volatile unsigned long leftTicks = 0;
volatile unsigned long rightTicks = 0;

// Command Queue Variables
String commandQueue = "";
bool isExecuting = false;

// ==========================================
// 🎛️ TURN CALIBRATION (TUNE THESE VALUES!)
// ==========================================
// How many encoder ticks = exactly 90 degrees for each direction.
// They may differ because motors have slight power differences.
// Increase the value if the car under-turns (less than 90 degrees).
// Decrease the value if the car over-turns (more than 90 degrees).
const unsigned long TICKS_RIGHT_90 = 17; // <-- Tune this for RIGHT turns
const unsigned long TICKS_LEFT_90  = 15; // <-- Tune this for LEFT turns

// ==========================================
// ⚡ INTERRUPT SERVICE ROUTINES (ISRs)
// ==========================================
void IRAM_ATTR leftEncoderISR() { leftTicks++; }

void IRAM_ATTR rightEncoderISR() { rightTicks++; }

// ==========================================
// 🚀 MOVEMENT FUNCTIONS
// ==========================================
void stopMotors() {
  digitalWrite(IN1_LEFT, LOW);
  digitalWrite(IN2_LEFT, LOW);
  analogWrite(EN_LEFT, 0);

  digitalWrite(IN3_RIGHT, LOW);
  digitalWrite(IN4_RIGHT, LOW);
  analogWrite(EN_RIGHT, 0);
}

// Helper to get distance from Sonar
float checkSonarDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout set to 30ms (~5 meters max)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0)
    return 999.0;                // No obstacle
  return duration * 0.034 / 2.0; // Convert to cm
}

void driveForward(float distanceCm) {
  // ⚙️ CALIBRATION NEEDED:
  // Standard 2WD wheel diameter is usually ~6.5cm. Circumference = ~20.4cm.
  // Standard LM393 encoder disk has 20 holes (ticks per rotation).
  // So 1 rotation = 20 ticks = 20.4 cm.
  // Roughly 1 tick = 1 cm! (Adjust this multiplier if your wheels differ)
  float ticksPerCm = 1.0;
  unsigned long targetTicks = distanceCm * ticksPerCm;

  // Reset counters
  leftTicks = 0;
  rightTicks = 0;

  // Set motor direction FORWARD
  digitalWrite(IN1_LEFT, HIGH);
  digitalWrite(IN2_LEFT, LOW);
  digitalWrite(IN3_RIGHT, HIGH);
  digitalWrite(IN4_RIGHT, LOW);

  // Base speed (0-255)
  int baseSpeed = 160;

  Serial.println("Moving Forward...");

  // Closed-loop PID control using Encoders to drive straight
  while (leftTicks < targetTicks && rightTicks < targetTicks) {

    // --- SONAR OBSTACLE CHECK ---
    float dist = checkSonarDistance();

    // Print the distance every loop so we can see if it is broken!
    Serial.print("Sonar Distance: ");
    Serial.print(dist);
    Serial.println(" cm");

    if (dist < 15.0) { // If obstacle is closer than 15cm
      Serial.println("OBSTACLE DETECTED! Pausing...");
      stopMotors();
      digitalWrite(LED_OBSTACLE, HIGH); // Turn LED ON
      delay(1000); // Wait for it to clear

      // Re-engage motors after waiting
      digitalWrite(LED_OBSTACLE, LOW);  // Turn LED OFF - resuming
      digitalWrite(IN1_LEFT, HIGH);
      digitalWrite(IN2_LEFT, LOW);
      digitalWrite(IN3_RIGHT, HIGH);
      digitalWrite(IN4_RIGHT, LOW);
      continue;
    }

    // --- DEBUG ENCODERS ---
    // Print ticks every so often to see if the sensors are actually working!
    Serial.print("Left Ticks: ");
    Serial.print(leftTicks);
    Serial.print(" | Right Ticks: ");
    Serial.println(rightTicks);

    // --- ENCODER CORRECTION ---
    // If left is spinning faster than right, error is positive.
    int error = leftTicks - rightTicks;

    int speedLeft = baseSpeed - (error * 3);  // Slow down left if it's ahead
    int speedRight = baseSpeed + (error * 3); // Speed up right if it's behind

    // Ensure speeds don't exceed limits
    speedLeft = constrain(speedLeft, 0, 255);
    speedRight = constrain(speedRight, 0, 255);

    analogWrite(EN_LEFT, speedLeft);
    analogWrite(EN_RIGHT, speedRight);

    delay(10); // Tiny delay for stability
  }

  stopMotors();
  Serial.println("Finished Forward Move.");
}

void turnRobot(int angle) {
  unsigned long targetTicks = 0;

  if (angle > 0) {
    // --- TURN RIGHT ---
    if (abs(angle) == 90)  targetTicks = TICKS_RIGHT_90;
    if (abs(angle) == 180) targetTicks = TICKS_RIGHT_90 * 2;
    Serial.println("Turning Right...");
    digitalWrite(IN1_LEFT, HIGH);
    digitalWrite(IN2_LEFT, LOW);
    digitalWrite(IN3_RIGHT, LOW);
    digitalWrite(IN4_RIGHT, HIGH);
  } else {
    // --- TURN LEFT ---
    if (abs(angle) == 90)  targetTicks = TICKS_LEFT_90;
    if (abs(angle) == 180) targetTicks = TICKS_LEFT_90 * 2;
    Serial.println("Turning Left...");
    digitalWrite(IN1_LEFT, LOW);
    digitalWrite(IN2_LEFT, HIGH);
    digitalWrite(IN3_RIGHT, HIGH);
    digitalWrite(IN4_RIGHT, LOW);
  }

  analogWrite(EN_LEFT, 150);
  analogWrite(EN_RIGHT, 150);

  // CRITICAL: Reset tick counters AFTER motors start (not before, or a
  // stray interrupt could trigger before we even start moving)
  leftTicks = 0;
  rightTicks = 0;

  // Wait until one of the wheels hits the target tick count
  while (leftTicks < targetTicks && rightTicks < targetTicks) {
    // Print ticks to debug turns!
    Serial.print("Turn Left Ticks: ");
    Serial.print(leftTicks);
    Serial.print(" | Turn Right Ticks: ");
    Serial.println(rightTicks);
    delay(10);
  }

  stopMotors();
  Serial.println("Turn Complete.");
}

// ==========================================
// 🛠️ SETUP ROUTINE
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for Serial Monitor to catch up
  Serial.println("\n--- Smart LFR Booting ---");

  // 1. Motor Setup
  pinMode(EN_LEFT, OUTPUT);
  pinMode(IN1_LEFT, OUTPUT);
  pinMode(IN2_LEFT, OUTPUT);
  pinMode(EN_RIGHT, OUTPUT);
  pinMode(IN3_RIGHT, OUTPUT);
  pinMode(IN4_RIGHT, OUTPUT);
  stopMotors();

  // Obstacle Warning LED
  pinMode(LED_OBSTACLE, OUTPUT);
  digitalWrite(LED_OBSTACLE, LOW); // Make sure it starts OFF

  // 2. Sensor Setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ENC_LEFT, INPUT);
  pinMode(ENC_RIGHT, INPUT);

  // 3. MPU6050 Gyro Setup (Raw I2C for GY-521 Clone)
  // CRITICAL: Initialize Gyro BEFORE attaching encoder interrupts!
  Wire.begin(21, 22);
  Wire.setClock(100000); // Slow to 100kHz - clone chips often fail at fast 400kHz
  delay(200); // Give the I2C bus and chip time to fully stabilize

  // --- FULL I2C BUS SCANNER ---
  Serial.println("Scanning I2C bus...");
  bool foundAny = false;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found I2C device at address: 0x");
      Serial.println(addr, HEX);
      foundAny = true;
    }
  }
  if (!foundAny) {
    Serial.println("No I2C devices found! Check SDA(D21) and SCL(D22) wires.");
  }

  // Try to wake up the MPU at 0x68 first, then 0x69
  bool gyroOk = false;
  byte mpuAddr = 0x68;
  for (byte tryAddr : {(byte)0x68, (byte)0x69}) {
    Wire.beginTransmission(tryAddr);
    Wire.write(0x6B);
    Wire.write(0x00); // Wake up
    if (Wire.endTransmission() == 0) {
      mpuAddr = tryAddr;
      gyroOk = true;
      break;
    }
  }
  if (gyroOk) {
    Serial.print("GY-521 Gyro Found and Woken Up at 0x");
    Serial.println(mpuAddr, HEX);
  } else {
    Serial.println("WARNING: GY-521 Gyro not responding! Check D21/D22 wiring.");
  }

  // 4. Encoder Interrupts (Attached AFTER Gyro is safely initialized)
  // CRITICAL FIX: Pins 34 and 35 are input-only and do NOT have internal
  // pull-ups! Setting them to INPUT_PULLUP makes them float.
  attachInterrupt(digitalPinToInterrupt(ENC_LEFT), leftEncoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_RIGHT), rightEncoderISR, RISING);

  // 5. Wi-Fi Access Point Setup
  Serial.println("Starting Wi-Fi Access Point...");
  WiFi.softAP("SmartRobot_AP", "password123");
  Serial.print("Connect to Wi-Fi 'SmartRobot_AP' and go to IP: ");
  Serial.println(WiFi.softAPIP());

  // 5. Web Server Routes
  // Send the HTML App when someone connects
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Receive the Path commands
  server.on(
      "/upload-path", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        // Empty callback (required syntax)
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        String received = "";
        for (size_t i = 0; i < len; i++) {
          received += (char)data[i];
        }

        commandQueue = received;
        isExecuting = true;

        Serial.println("Received New Path: " + commandQueue);
        request->send(200, "text/plain", "OK");
      });

  // Start Server
  server.begin();
  Serial.println("Web Server Running.");
}

// ==========================================
// 🔄 MAIN LOOP
// ==========================================
void loop() {

  // If we have a path to execute
  if (isExecuting && commandQueue.length() > 0) {

    // Extract the next command from the comma-separated string
    int commaIdx = commandQueue.indexOf(',');
    String cmd = "";

    if (commaIdx != -1) {
      cmd = commandQueue.substring(0, commaIdx);
      commandQueue = commandQueue.substring(commaIdx + 1);
    } else {
      cmd = commandQueue;
      commandQueue = ""; // Empty the queue
    }

    cmd.trim();
    Serial.println("Executing Command: " + cmd);

    // Execute based on command letter
    if (cmd == "F") {
      driveForward(20.0); // Drive 20cm forward
    } else if (cmd == "R") {
      turnRobot(90); // Turn Right 90deg
    } else if (cmd == "L") {
      turnRobot(-90); // Turn Left 90deg
    } else if (cmd == "U") {
      turnRobot(180); // U-Turn 180deg
    } else if (cmd == "STOP") {
      stopMotors();
      isExecuting = false;
      Serial.println("Path Completed.");
    }

    delay(500); // 500ms pause between actions to stabilize
  }
}
