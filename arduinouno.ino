#include <Servo.h>
#include <DHT.h>

#define DHTPIN 7
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int voltageSensorPin = A0;

Servo myServo;
const int servoPin = 9;

const int buttonPin = 6;

#define HALL_SENSOR_PIN 2
#define MOTOR_IN1 3
#define MOTOR_IN2 4
#define MOTOR_ENA 5

volatile int pulseCount = 0;
unsigned long lastMotorTime = 0;
const int RPM_AVG_COUNT = 5;
float rpmBuffer[RPM_AVG_COUNT];
int rpmBufferIndex = 0;
float magneticDiskRPM = 0;
int motorSpeed = 0;

unsigned long lastSensorTime = 0;
const unsigned long sensorInterval = 2000;

bool servoOpen = false;

void countPulse() {
  pulseCount++;
}

void adjustMotorSpeed(int targetSpeed) {
  int difference = targetSpeed - motorSpeed;
  motorSpeed += difference * 0.25;
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
  dht.begin();
  myServo.attach(servoPin);
  myServo.write(0);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), countPulse, FALLING);
  for (int i = 0; i < RPM_AVG_COUNT; i++) {
    rpmBuffer[i] = 0;
  }
  stopMotor();
  Serial.println("System initialized.");
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.indexOf("UNLOCKED") != -1)
