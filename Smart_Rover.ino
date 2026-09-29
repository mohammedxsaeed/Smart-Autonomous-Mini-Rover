/*
 * Smart Autonomous Mini Rover 
 * Features: Manual Control, Obstacle Avoidance, Target Tracking (Auto-Pilot), and Parallel Auto-Parking.
 * Hardware: Arduino Uno, L293D Motor Shield, HC-05 Bluetooth, HC-SR04 Ultrasonic, SG90 Servo.
 */

#include <AFMotor.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// --- Hardware Initialization ---
AF_DCMotor motor1(1); // Front-Left Motor
AF_DCMotor motor2(2); // Rear-Left Motor
AF_DCMotor motor3(3); // Rear-Right Motor
AF_DCMotor motor4(4); // Front-Right Motor

Servo radarServo;
SoftwareSerial bluetooth(A2, A3); // RX = A2, TX = A3

#define TRIG_PIN A0
#define ECHO_PIN A1

// --- Global State Variables ---
int robotSpeed = 180;         
bool isAutoParking = false; 
bool isAutoPilot = false;     // Target tracking mode

// --- Ultrasonic Sensor Function ---
long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout set to 25ms to prevent blocking (approx 400cm max range)
  long duration = pulseIn(ECHO_PIN, HIGH, 25000); 
  if (duration == 0) return 400; // Return max distance if out of range
  
  return (duration * 0.034 / 2); // Convert duration to centimeters
}

// --- Basic Locomotion Functions ---
void setMotorSpeed(int speed) {
  motor1.setSpeed(speed);
  motor2.setSpeed(speed);
  motor3.setSpeed(speed);
  motor4.setSpeed(speed);
}

void moveForward() {
  // Active Safety: Prevent forward movement if obstacle is closer than 15cm
  radarServo.write(90);
  long frontDist = getDistance();
  
  if (frontDist > 0 && frontDist <= 15) {
    stopRobot();
    bluetooth.println("WARNING: Obstacle Ahead! Forward Blocked.");
    return; 
  }

  setMotorSpeed(robotSpeed);
  motor1.run(FORWARD);  motor2.run(FORWARD);
  motor3.run(FORWARD);  motor4.run(FORWARD);
}

void moveBackward() {
  setMotorSpeed(robotSpeed);
  motor1.run(BACKWARD); motor2.run(BACKWARD);
  motor3.run(BACKWARD); motor4.run(BACKWARD);
}

void turnLeft() {
  setMotorSpeed(robotSpeed);
  motor1.run(BACKWARD); motor2.run(BACKWARD);
  motor3.run(FORWARD);  motor4.run(FORWARD);
}

void turnRight() {
  setMotorSpeed(robotSpeed);
  motor1.run(FORWARD);  motor2.run(FORWARD);
  motor3.run(BACKWARD); motor4.run(BACKWARD);
}

void stopRobot() {
  motor1.run(RELEASE); motor2.run(RELEASE);
  motor3.run(RELEASE); motor4.run(RELEASE);
}

// --- Obstacle Avoidance Maneuver ---
void avoidObstacleAndRecalibrate() {
  bluetooth.println("STATUS: Obstacle detected! Executing evasion maneuver...");
  stopRobot();
  delay(100);

  // 1. Back up safely
  setMotorSpeed(160);
  moveBackward();
  delay(400);
  stopRobot();
  delay(150);

  // 2. Scan surroundings (Right then Left)
  radarServo.write(30); 
  delay(250);
  long rightDist = getDistance();

  radarServo.write(150); 
  delay(250);
  long leftDist = getDistance();

  radarServo.write(90); // Reset servo to center

  // 3. Turn towards the clearest path
  if (rightDist > leftDist) {
    turnRight();
    delay(300);
  } else {
    turnLeft();
    delay(300);
  }
  stopRobot();
}

// --- Parallel Auto-Parking Algorithm ---
void executeSmartAutoParking() {
  bluetooth.println("STATUS: Searching for parking barrier...");
  
  radarServo.write(0); // Look completely to the right
  delay(400);

  bool spotFound = false;
  int searchCycles = 0;

  // Search for an open slot while moving forward
  while (!spotFound && searchCycles < 15) {
    long sideDist = getDistance();

    if (sideDist > 35) { 
      bluetooth.println("STATUS: Potential slot detected. Verifying length...");
      
      setMotorSpeed(140);
      motor1.run(FORWARD); motor2.run(FORWARD);
      motor3.run(FORWARD); motor4.run(FORWARD);
      delay(500);
      stopRobot();
      delay(200);

      // Verify the slot is wide enough for the chassis
      if (getDistance() > 35) {
        spotFound = true;
        bluetooth.println("STATUS: Slot verified! Initiating reverse maneuver...");
      }
    } else {
      // Continue crawling forward if blocked
      setMotorSpeed(140);
      motor1.run(FORWARD); motor2.run(FORWARD);
      motor3.run(FORWARD); motor4.run(FORWARD);
      delay(250);
      stopRobot();
      delay(150);
    }
    searchCycles++;
  }

  if (!spotFound) {
    bluetooth.println("ERROR: Parking spot not found. Aborting.");
    radarServo.write(90);
    isAutoParking = false;
    return;
  }

  // Execute Reverse Parallel Parking Sequence
  setMotorSpeed(150);
  
  // Align
  motor1.run(FORWARD); motor2.run(FORWARD);
  motor3.run(FORWARD); motor4.run(FORWARD);
  delay(350);
  stopRobot();
  delay(300);

  // Reverse Right
  turnRight();
  delay(650);
  stopRobot();
  delay(300);

  // Reverse Left (Straighten)
  turnLeft();
  delay(650);
  stopRobot();
  delay(300);

  // Center alignment
  motor1.run(FORWARD); motor2.run(FORWARD);
  motor3.run(FORWARD); motor4.run(FORWARD);
  delay(200);
  stopRobot();

  radarServo.write(90);
  bluetooth.println("STATUS: Auto-Parking successfully completed!");
  isAutoParking = false;
}

// --- Target Tracking Algorithm (Auto-Pilot) ---
void trackAndComeToUser() {
  radarServo.write(90);
  long frontDist = getDistance();

  // Trigger evasion if a dynamic obstacle blocks the path
  if (frontDist > 0 && frontDist <= 15) {
    avoidObstacleAndRecalibrate();
    return;
  }

  int closestAngle = 90;
  long minDistance = 400;

  // Radar sweep to locate the target (user)
  for (int angle = 30; angle <= 150; angle += 30) {
    radarServo.write(angle);
    delay(50);
    long dist = getDistance();
    
    if (dist < minDistance && dist > 5) {
      minDistance = dist;
      closestAngle = angle;
    }
  }

  radarServo.write(90); // Reset servo

  // Target reached safety threshold
  if (minDistance <= 22 && minDistance >= 12) {
    stopRobot();
    bluetooth.println("STATUS: Target reached successfully.");
    isAutoPilot = false;
    return;
  }

  if (minDistance > 150) {
    stopRobot();
    bluetooth.println("STATUS: Target lost. Searching...");
    return;
  }

  // Adjust heading towards the target
  if (closestAngle < 70) {
    setMotorSpeed(160);
    turnRight();
    delay(180);
    stopRobot();
  } 
  else if (closestAngle > 110) {
    setMotorSpeed(160);
    turnLeft();
    delay(180);
    stopRobot();
  } 
  else {
    setMotorSpeed(160);
    moveForward();
    delay(250);
    stopRobot();
  }
}

// --- System Initialization ---
void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  radarServo.attach(10);
  radarServo.write(90);

  bluetooth.begin(9600);
  bluetooth.println("System Booted. Awaiting Telemetry Commands.");

  stopRobot();
}

// --- Main Execution Loop ---
void loop() {
  // Command Parser
  if (bluetooth.available()) {
    char command = bluetooth.read();

    // Mode Selection
    if (command == 'P' || command == 'p') {
      isAutoPilot = false;
      isAutoParking = true;
      executeSmartAutoParking();
    }
    else if (command == 'F' || command == 'f') {
      isAutoParking = false;
      isAutoPilot = true;
      bluetooth.println("STATUS: Target Tracking Engaged.");
    }
    else if (command == 'M' || command == 'm' || command == 'X' || command == 'x') {
      isAutoParking = false;
      isAutoPilot = false;
      stopRobot();
      bluetooth.println("STATUS: Manual Override Active.");
    }

    // Manual Locomotion Control
    if (!isAutoParking && !isAutoPilot) {
      switch (command) {
        case 'W': case 'w': 
          moveForward(); 
          break; 
        case 'S': case 's': 
          moveBackward(); 
          break; 
        case 'A': case 'a': // Tap to turn Left
          turnLeft();
          delay(150);
          stopRobot(); 
          break;
        case 'D': case 'd': // Tap to turn Right
          turnRight(); 
          delay(150);
          stopRobot(); 
          break;
        case 'C': case 'c': // Continuous Right turn
          turnRight(); 
          break;
        case 'Z': case 'z': // Continuous Left turn
          turnLeft(); 
          break;
        case '1': robotSpeed = 130; break;
        case '2': robotSpeed = 190; break;
        case '3': robotSpeed = 255; break;
        default: break;
      }
    }
  }

  // Background Task Execution
  if (isAutoPilot) {
    trackAndComeToUser();
  }

  delay(20);
}