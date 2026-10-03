// ============================================================
// Wall-E 3D Robot Receiver Code - Arduino Mega2560
// Receives commands from ESP32 and controls motors and servos
// Communication Protocol:
// S,id,angle     - Servo control (id 0-7, angle 0-180)
// M,left,right   - Motor control (speed -255 to 255)
// V,value        - Velocity/Speed multiplier (30-255)
// X,mode         - Mode (0=Normal, 1=Turbo)
// A,action       - Action (1=Wave, 2=Spin, 3=Beep, 4=Music)
// E,status       - Emergency stop (1=Stop, 0=Resume)
// ============================================================

#include <Servo.h>
#include <Wire.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================

// Servo Pins (Analog pins A0-A6, or use A7 for 8th servo)
const int SERVO_PINS[8] = {
  A0,  // Servo 0: Left Arm
  A1,  // Servo 1: Right Arm
  A2,  // Servo 2: Neck Base (Rotation)
  A3,  // Servo 3: Neck Tilt (Up/Down)
  A4,  // Servo 4: Head Rotation
  A5,  // Servo 5: Left Eye
  A6,  // Servo 6: Right Eye
  A7   // Servo 7: Jaw/Mouth
};

// Motor Control Pins - L298N Motor Driver
// Left Motor
const int LEFT_MOTOR_PIN1 = 8;    // Forward
const int LEFT_MOTOR_PIN2 = 9;    // Backward
const int LEFT_MOTOR_PWM = 3;     // PWM speed control (Timer2)

// Right Motor
const int RIGHT_MOTOR_PIN1 = 10;  // Forward
const int RIGHT_MOTOR_PIN2 = 11;  // Backward
const int RIGHT_MOTOR_PWM = 5;    // PWM speed control (Timer3)

// Audio Output
const int BUZZER_PIN = 50;

// Status LED
const int STATUS_LED = 13;

// ============================================================
// GLOBAL VARIABLES
// ============================================================

Servo servos[8];
int servoAngles[8] = {90, 90, 90, 90, 90, 90, 90, 90};
int motorLeft = 0;
int motorRight = 0;
int speedMultiplier = 100;
bool turboMode = false;
bool emergencyStop = false;
bool connectionLost = false;
unsigned long lastCommandTime = 0;
const unsigned long COMMAND_TIMEOUT = 2000; // 2 seconds

// ============================================================
// SETUP FUNCTION
// ============================================================

void setup() {
  // Initialize serial communication
  Serial.begin(115200);  // USB Serial
  Serial1.begin(115200); // Hardware Serial for ESP32 (RX1=18, TX1=19)
  
  delay(1000);
  
  Serial.println("\n=== Wall-E 3D Robot Receiver Starting ===");
  
  // Initialize servo pins
  for (int i = 0; i < 8; i++) {
    servos[i].attach(SERVO_PINS[i]);
    servos[i].write(90); // Set all servos to center position
    Serial.print("Servo ");
    Serial.print(i);
    Serial.print(" attached to pin ");
    Serial.println(SERVO_PINS[i]);
  }
  
  // Initialize motor control pins
  pinMode(LEFT_MOTOR_PIN1, OUTPUT);
  pinMode(LEFT_MOTOR_PIN2, OUTPUT);
  pinMode(LEFT_MOTOR_PWM, OUTPUT);
  
  pinMode(RIGHT_MOTOR_PIN1, OUTPUT);
  pinMode(RIGHT_MOTOR_PIN2, OUTPUT);
  pinMode(RIGHT_MOTOR_PWM, OUTPUT);
  
  // Initialize buzzer and status LED
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  // Stop all motors
  stopAllMotors();
  
  Serial.println("Initialization complete. Waiting for commands...");
  Serial.println("Format: S,id,angle | M,left,right | V,value | X,mode | A,action | E,status\n");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  // Read incoming commands from ESP32
  while (Serial1.available()) {
    String command = Serial1.readStringUntil('\n');
    command.trim();
    
    if (command.length() > 0) {
      lastCommandTime = millis();
      digitalWrite(STATUS_LED, HIGH);
      
      Serial.print("Received: ");
      Serial.println(command);
      
      processCommand(command);
      
      digitalWrite(STATUS_LED, LOW);
    }
  }
  
  // Emergency timeout - if no command received for COMMAND_TIMEOUT
  if (!emergencyStop && (millis() - lastCommandTime) > COMMAND_TIMEOUT) {
    if (!connectionLost) {
      connectionLost = true;
      stopAllMotors();
      Serial.println("⚠️  CONNECTION LOST - MOTORS STOPPED");
      // Alert sound
      for (int i = 0; i < 3; i++) {
        tone(BUZZER_PIN, 1500, 100);
        delay(150);
      }
    }
  } else if (connectionLost && (millis() - lastCommandTime) <= COMMAND_TIMEOUT) {
    connectionLost = false;
    Serial.println("✅ CONNECTION RESTORED");
  }
  
  delay(10);
}

// ============================================================
// COMMAND PROCESSING
// ============================================================

void processCommand(String cmd) {
  // Extract command type (first character)
  char cmdType = cmd.charAt(0);
  
  // Handle heartbeat/ping command
  if (cmdType == 'P') {
    // Ping/heartbeat - just reset lastCommandTime
    return;
  }
  
  // Extract parameters (everything after first comma)
  int firstComma = cmd.indexOf(',');
  if (firstComma == -1) return;
  
  String params = cmd.substring(firstComma + 1);
  
  switch (cmdType) {
    case 'S': // Servo command: S,id,angle
      handleServoCommand(params);
      break;
      
    case 'M': // Motor command: M,left,right
      handleMotorCommand(params);
      break;
      
    case 'V': // Velocity/Speed: V,value
      handleSpeedCommand(params);
      break;
      
    case 'X': // Mode: X,0|1 (Normal/Turbo)
      handleModeCommand(params);
      break;
      
    case 'A': // Action: A,1|2|3|4
      handleActionCommand(params);
      break;
      
    case 'E': // Emergency: E,0|1
      handleEmergencyCommand(params);
      break;
      
    default:
      Serial.println("Unknown command type");
  }
}

// ============================================================
// SERVO CONTROL
// ============================================================
// Controls 8 servos with angles from 0-180 degrees
// Servo 0: Left Arm, Servo 1: Right Arm
// Servo 2: Neck Base (Rotation), Servo 3: Neck Tilt (Up/Down)
// Servo 4: Head Rotation, Servo 5: Left Eye
// Servo 6: Right Eye, Servo 7: Jaw/Mouth

void handleServoCommand(String params) {
  // Parse "id,angle"
  int commaPos = params.indexOf(',');
  if (commaPos == -1) return;
  
  int id = params.substring(0, commaPos).toInt();
  int angle = params.substring(commaPos + 1).toInt();
  
  if (id >= 0 && id < 8 && angle >= 0 && angle <= 180) {
    servoAngles[id] = angle;
    if (!emergencyStop) {
      servos[id].write(angle);
      Serial.print("Servo ");
      Serial.print(id);
      Serial.print(" -> ");
      Serial.print(angle);
      Serial.println("°");
    }
  }
}

// ============================================================
// MOTOR CONTROL
// ============================================================
// Controls left and right motors with speed values from -255 to 255
// Negative values = backward, Positive values = forward
// Uses PWM for speed control via L298N motor driver

void handleMotorCommand(String params) {
  // Parse "left,right"
  int commaPos = params.indexOf(',');
  if (commaPos == -1) return;
  
  motorLeft = params.substring(0, commaPos).toInt();
  motorRight = params.substring(commaPos + 1).toInt();
  
  // Constrain values
  motorLeft = constrain(motorLeft, -255, 255);
  motorRight = constrain(motorRight, -255, 255);
  
  if (!emergencyStop) {
    setMotorSpeed(motorLeft, motorRight);
    Serial.print("Motors -> L:");
    Serial.print(motorLeft);
    Serial.print(" R:");
    Serial.println(motorRight);
  }
}

// Sets motor speed with direction control and speed multiplier application
// Handles positive (forward) and negative (backward) speeds
// Applies speed multiplier to control overall robot velocity
void setMotorSpeed(int leftSpeed, int rightSpeed) {
  // Apply speed multiplier
  leftSpeed = (leftSpeed * speedMultiplier) / 100;
  rightSpeed = (rightSpeed * speedMultiplier) / 100;
  
  // Constrain to valid PWM range
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);
  
  // LEFT MOTOR CONTROL
  if (leftSpeed > 0) {
    // Forward
    digitalWrite(LEFT_MOTOR_PIN1, HIGH);
    digitalWrite(LEFT_MOTOR_PIN2, LOW);
    analogWrite(LEFT_MOTOR_PWM, leftSpeed);
  } else if (leftSpeed < 0) {
    // Backward
    digitalWrite(LEFT_MOTOR_PIN1, LOW);
    digitalWrite(LEFT_MOTOR_PIN2, HIGH);
    analogWrite(LEFT_MOTOR_PWM, -leftSpeed);
  } else {
    // Stop
    digitalWrite(LEFT_MOTOR_PIN1, LOW);
    digitalWrite(LEFT_MOTOR_PIN2, LOW);
    analogWrite(LEFT_MOTOR_PWM, 0);
  }
  
  // RIGHT MOTOR CONTROL
  if (rightSpeed > 0) {
    // Forward
    digitalWrite(RIGHT_MOTOR_PIN1, HIGH);
    digitalWrite(RIGHT_MOTOR_PIN2, LOW);
    analogWrite(RIGHT_MOTOR_PWM, rightSpeed);
  } else if (rightSpeed < 0) {
    // Backward
    digitalWrite(RIGHT_MOTOR_PIN1, LOW);
    digitalWrite(RIGHT_MOTOR_PIN2, HIGH);
    analogWrite(RIGHT_MOTOR_PWM, -rightSpeed);
  } else {
    // Stop
    digitalWrite(RIGHT_MOTOR_PIN1, LOW);
    digitalWrite(RIGHT_MOTOR_PIN2, LOW);
    analogWrite(RIGHT_MOTOR_PWM, 0);
  }
}

// Immediately stops both motors by setting PWM to 0
// Called during emergency stop or system shutdown
void stopAllMotors() {
  setMotorSpeed(0, 0);
  Serial.println("All motors stopped");
}

// ============================================================
// SPEED CONTROL
// ============================================================
// Controls speed multiplier (30-255) to scale motor power
// Formula: actualSpeed = (commandSpeed * speedMultiplier) / 100
// Allows remote control of overall robot speed/power

void handleSpeedCommand(String params) {
  speedMultiplier = params.toInt();
  speedMultiplier = constrain(speedMultiplier, 30, 255);
  Serial.print("Speed multiplier set to: ");
  Serial.print(speedMultiplier);
  Serial.println("%");
}

// ============================================================
// MODE CONTROL (NORMAL / TURBO)
// ============================================================
// Normal Mode: Uses speed multiplier for controlled movement
// Turbo Mode: Uses full 255 PWM power for maximum speed
// Mode selection sent from ESP32 web interface

void handleModeCommand(String params) {
  int mode = params.toInt();
  turboMode = (mode == 1);
  Serial.print("Mode set to: ");
  Serial.println(turboMode ? "TURBO" : "NORMAL");
}

// ============================================================
// ACTION SEQUENCES
// ============================================================
// Action 1: Wave - Waves left arm 3 times
// Action 2: Spin - Rotates robot 360 degrees
// Action 3: Beep - Produces 3 beep sounds (1kHz tone)
// Action 4: Music - Plays a simple 8-note melody (C major scale)

void handleActionCommand(String params) {
  int action = params.toInt();
  
  if (emergencyStop) return;
  
  switch (action) {
    case 1:
      waveAction();
      break;
    case 2:
      spinAction();
      break;
    case 3:
      beepAction();
      break;
    case 4:
      musicAction();
      break;
    default:
      Serial.println("Unknown action");
  }
}

void waveAction() {
  Serial.println("Executing: WAVE");
  
  // Wave with left arm
  for (int i = 0; i < 3; i++) {
    // Raise arm
    for (int angle = 90; angle <= 180; angle += 10) {
      servos[0].write(angle);
      delay(20);
    }
    // Lower arm
    for (int angle = 180; angle >= 90; angle -= 10) {
      servos[0].write(angle);
      delay(20);
    }
  }
}

void spinAction() {
  Serial.println("Executing: SPIN");
  
  // Spin robot 360 degrees
  setMotorSpeed(150, -150);
  delay(1500);
  setMotorSpeed(0, 0);
}

void beepAction() {
  Serial.println("Executing: BEEP");
  
  // Multiple beeps
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000, 200); // 1kHz for 200ms
    delay(300);
  }
}

void musicAction() {
  Serial.println("Executing: MUSIC");
  
  // Simple melody - Play a short tune
  int melody[] = {262, 294, 330, 349, 392, 440, 494, 523}; // C D E F G A B C
  int noteDuration = 200; // milliseconds
  
  for (int i = 0; i < 8; i++) {
    tone(BUZZER_PIN, melody[i], noteDuration);
    delay(noteDuration + 50);
  }
}

// ============================================================
// EMERGENCY STOP
// ============================================================
// Status 1: Activates emergency stop, stops motors, produces alert sound
// Status 0: Deactivates emergency stop, resumes normal operation
// When active, all servo and motor commands are blocked

void handleEmergencyCommand(String params) {
  int status = params.toInt();
  
  emergencyStop = (status == 1);
  
  if (emergencyStop) {
    Serial.println("🛑 EMERGENCY STOP ACTIVATED!");
    stopAllMotors();
    
    // Alert sound
    for (int i = 0; i < 5; i++) {
      digitalWrite(STATUS_LED, HIGH);
      tone(BUZZER_PIN, 2000, 100);
      delay(150);
      digitalWrite(STATUS_LED, LOW);
      delay(150);
    }
  } else {
    Serial.println("Emergency stop released");
    digitalWrite(STATUS_LED, LOW);
  }
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

// Prints complete system status to serial console
// Includes emergency stop state, speed multiplier, mode, motor speeds, and servo angles
void printStatus() {
  Serial.println("\n=== Current Status ===");
  Serial.print("Emergency Stop: ");
  Serial.println(emergencyStop ? "ON" : "OFF");
  Serial.print("Speed Multiplier: ");
  Serial.print(speedMultiplier);
  Serial.println("%");
  Serial.print("Mode: ");
  Serial.println(turboMode ? "TURBO" : "NORMAL");
  Serial.print("Left Motor: ");
  Serial.println(motorLeft);
  Serial.print("Right Motor: ");
  Serial.println(motorRight);
  Serial.println("Servo Angles:");
  for (int i = 0; i < 8; i++) {
    Serial.print("  Servo ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(servoAngles[i]);
    Serial.println("°");
  }
  Serial.println();
}

// Sends status information back to ESP32 via Serial1
// Can transmit battery level (format: "B,value") or other status data
void sendStatus(String status) {
  Serial1.print(status);
  Serial1.println();
}
