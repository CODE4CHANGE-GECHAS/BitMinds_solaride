#include <Servo.h>
#include <DHT.h>

// --- DHT11 Sensor Setup ---
#define DHTPIN 7         // DHT11 sensor connected to digital pin 7
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- Voltage Sensor Setup ---
const int voltageSensorPin = A0;  // Analog input pin

// --- Servo Motor Setup ---
Servo myServo;
const int servoPin = 9;  // Servo control pin

// --- Push Button Setup (for servo locking) ---
// (Reassigned from pin 2 to pin 6 to avoid conflict with hall sensor)
const int buttonPin = 6;  

// --- Hall Sensor & Motor Driver Setup ---
#define HALL_SENSOR_PIN 2  // Hall Sensor connected to digital pin 2 (external interrupt)
#define MOTOR_IN1 3        // L298N IN1 → digital pin 3
#define MOTOR_IN2 4        // L298N IN2 → digital pin 4
#define MOTOR_ENA 5        // L298N ENA (PWM) → digital pin 5

volatile int pulseCount = 0;
unsigned long lastMotorTime = 0;       // For motor RPM update
const int RPM_AVG_COUNT = 5;           // Number of samples for averaging
float rpmBuffer[RPM_AVG_COUNT];
int rpmBufferIndex = 0;
float magneticDiskRPM = 0;
int motorSpeed = 0;

// --- Sensor Data Timing ---
unsigned long lastSensorTime = 0;
const unsigned long sensorInterval = 2000;  // Sensor data update interval (2 sec)

// --- Servo Control Variables ---
bool servoOpen = false;

// Interrupt service routine for hall sensor pulses
void countPulse() {
  pulseCount++;
}

// --- Motor Control Functions ---
void adjustMotorSpeed(int targetSpeed) {
  int difference = targetSpeed - motorSpeed;
  motorSpeed += difference * 0.25;  // Proportional adjustment (smoothing factor)
  motorSpeed = constrain(motorSpeed, 0, 255);
  runMotor(motorSpeed);
}

void runMotor(int speed) {
  if (speed == 0) {
    stopMotor();
  } else {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_ENA, speed);
  }
}

void stopMotor() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize sensors
  dht.begin();
  
  // Initialize servo and set to locked (closed) position
  myServo.attach(servoPin);
  myServo.write(0);
  
  // Set up push button with internal pull-up (for servo lock)
  pinMode(buttonPin, INPUT_PULLUP);
  
  // Set up hall sensor and motor driver pins
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  
  // Attach interrupt for hall sensor pulses
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), countPulse, FALLING);
  
  // Initialize RPM buffer
  for (int i = 0; i < RPM_AVG_COUNT; i++) {
    rpmBuffer[i] = 0;
  }
  
  // Ensure motor is stopped at startup
  stopMotor();
  
  Serial.println("System initialized: Sensors, Servo, and Motor control.");
}

void loop() {
  // --- Handle Serial Commands for Servo Control ---
  // (Type "UNLOCKED" in the Serial Monitor to open the servo.)
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.indexOf("UNLOCKED") != -1) {
      if (!servoOpen) {
        myServo.write(90);  // Open position (e.g., 90°)
        servoOpen = true;
        Serial.println("Servo opened via command.");
      }
    }
  }
  
  // --- Check Push Button to Lock Servo ---
  if (servoOpen && digitalRead(buttonPin) == LOW) {
    delay(50);  // Debounce delay
    if (digitalRead(buttonPin) == LOW) {
      myServo.write(0);  // Lock position (0°)
      servoOpen = false;
      Serial.println("Servo locked via push button.");
      // Wait until button is released
      while (digitalRead(buttonPin) == LOW) {
        delay(10);
      }
    }
  }
  
  // --- Transmit Sensor Data (DHT11 & Voltage Sensor) ---
  if (millis() - lastSensorTime >= sensorInterval) {
    lastSensorTime = millis();
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();  // in Celsius
    int sensorValue = analogRead(voltageSensorPin);
    float voltage = sensorValue * (5.0 / 1023.0);
    
    Serial.print("Sensor Data -> T:");
    Serial.print(temperature, 1);
    Serial.print(", H:");
    Serial.print(humidity, 1);
    Serial.print(", V:");
    Serial.println(voltage, 2);
  }
  
  // --- Motor RPM Calculation and Control ---
  unsigned long currentTime = millis();
  if (currentTime - lastMotorTime >= 200) {
    // Calculate instantaneous RPM
    // Formula: (pulseCount * (60/0.2)) / 4, where 60/0.2 = 300 and 4 pulses per revolution
    float instantRPM = pulseCount * 300.0 / 4.0;
    
    // Update RPM buffer for smoothing
    rpmBuffer[rpmBufferIndex] = instantRPM;
    rpmBufferIndex = (rpmBufferIndex + 1) % RPM_AVG_COUNT;
    
    // Compute smoothed RPM
    magneticDiskRPM = 0;
    for (int i = 0; i < RPM_AVG_COUNT; i++) {
      magneticDiskRPM += rpmBuffer[i];
    }
    magneticDiskRPM /= RPM_AVG_COUNT;
    
    Serial.print("Smoothed RPM: ");
    Serial.println(magneticDiskRPM);
    
    // Map RPM (0 to 300) to PWM value (0 to 255)
    int targetSpeed = map(magneticDiskRPM, 0, 300, 0, 255);
    targetSpeed = constrain(targetSpeed, 0, 255);
    
    adjustMotorSpeed(targetSpeed);
    
    // Reset pulse count for next interval and update timer
    pulseCount = 0;
    lastMotorTime = currentTime;
  }
}