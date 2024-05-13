#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

Servo directionServo;  // create servo object to control a servo

// Recommended PWM GPIO pins on the ESP32 include 2,4,12-19,21-23,25-27,32-33
int servoPin = 13;
int servoState = 0;
JsonDocument doc;

WebSocketsServer websockets(81);

// Motor A
int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 14;
bool motorStopped = true;
String boatDir = "forward";

// Setting PWM properties
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 200;


void setup() {

  // Sets the pins as outputs:
  // Motors.
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

  // configure LED PWM functionalitites
  ledcSetup(pwmChannel, freq, resolution);

  // attach the channel to the GPIO to be controlled
  ledcAttachPin(enable1Pin, pwmChannel);

  Serial.begin(115200);
  // @note: Review servo.
  
  directionServo.attach(servoPin);

  delay(2000);
  WiFi.softAP("ControlBoat", "");
  Serial.println("\nControlBoat");
  Serial.println(WiFi.softAPIP());

  websockets.begin();
  websockets.onEvent(webSocketEvent);
}

void changeDirection(String newDir) {
  // Do not change the direction if the boat
  // alredy have the new direction.
  if (newDir.equalsIgnoreCase(boatDir)) {
    return;
  }

  int currentPower = dutyCycle;
  stopMotor();

  if (boatDir.equalsIgnoreCase("forward")) {
    boatDir = "backward";

  } else {
    boatDir = "forward";
  }
  startMotor();
  Serial.println("Motor changed direction: " + boatDir);
}

void changeSpeed(String speed) {
  if (speed.equalsIgnoreCase("increase") && dutyCycle < 255) {
    dutyCycle += 5;
    ledcWrite(pwmChannel, dutyCycle);
  }
  else if (speed.equalsIgnoreCase("decrease") && dutyCycle > 200) {
    dutyCycle -= 5;
    ledcWrite(pwmChannel, dutyCycle);
  }
  else {
    Serial.println("Could not " + speed + " the speed");
  }
}

void stopMotor() {
  if (motorStopped) {
    return;
  }
  int currentPower = dutyCycle;
  while (currentPower >= 0) {
    ledcWrite(pwmChannel, currentPower);
    Serial.print("Stop the motor: ");
    Serial.println(currentPower);
    currentPower -= 5;
    delay(10);
  }
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  motorStopped = true;
  Serial.println("Motor stopped");
}

void startMotor() {
  if (!motorStopped) {
    return;
  }
  int currentPower = 0;
  if (boatDir.equalsIgnoreCase("forward")) {
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW);

  } else {
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH);
  }

  while (currentPower <= dutyCycle) {
    ledcWrite(pwmChannel, currentPower);
    Serial.print("Accelerating the motor: ");
    Serial.println(currentPower);
    currentPower += 5;
    delay(10);
  }
  Serial.println("Motor started");
  motorStopped = false;
}

void loop() {
  websockets.loop();
}

bool moveServo(String dir) {
  if (dir.equalsIgnoreCase("left") && servoState >= 5) {
    servoState -= 5;
    String servoString = (String)servoState;
    directionServo.write(servoState);
    Serial.println("LEFT: " + servoString);
  } else if (dir.equalsIgnoreCase("right") && servoState <= 175) {
    servoState += 5;
    String servoString = (String)servoState;
    directionServo.write(servoState);
    Serial.println("right: " + servoString);
  } else {
    return false;
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Desconectado!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = websockets.remoteIP(num);
        Serial.printf("[%u] Conectado en %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[4]);
        sendMessage(num, "Conectado al servidor");
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] Mensaje recibido: %s\n", num, payload);
      String msg = String((char *)(payload));

      if (msg.equalsIgnoreCase("left")) {
        String servoString = (String)servoState;
        if (moveServo("left") == false) {

          sendMessage(num, "No se puede girar más a la izquierda: " + servoString);
        }
        else {
          servoString = (String)servoState;
          sendMessage(num, "Servo left: " + servoString);
        }
      } else if (msg.equalsIgnoreCase("right")) {

        String servoString = (String)servoState;
        if (moveServo("right") == false) {
          sendMessage(num, "No se puede girar más a la derecha: " + servoString);
        }
        else {
          servoString = (String)servoState;
          sendMessage(num, "Servo right: " + servoString);
        }
      } else if(msg.equalsIgnoreCase("motor_on")) {
        startMotor();
        sendMessage(num, "Motor on");
      } else if(msg.equalsIgnoreCase("motor_off")) {
        stopMotor();
        sendMessage(num, "Motor off");
      } else if(msg.equalsIgnoreCase("Motor_backward")) {
        changeDirection("backward");
        sendMessage(num, "Motor backward");
      } else if(msg.equalsIgnoreCase("Motor_forward")) {
        changeDirection("forward");
        sendMessage(num, "Motor forward");
      } else if(msg.equalsIgnoreCase("Motor_increase")) {
        changeSpeed("increase");
        sendMessage(num, "Motor increase");
      } else if(msg.equalsIgnoreCase("Motor_decrease")) {
        changeSpeed("decrease");
        sendMessage(num, "Motor decrease");
      }
  }
}

void sendMessage(uint8_t num, String message) {
  String servoString = (String)servoState;
  doc["message"] = message;
  doc["servoState"] = servoString;
  if (motorStopped) {
    doc["motorState"] = "Stopped";
  }
  else {
    doc["motorState"] = "Running";
  }
  doc["speed"] = dutyCycle;
  doc["motorDir"] = boatDir;

  String jsonString;
  size_t len = serializeJson(doc, jsonString);
  Serial.println(jsonString);
  websockets.sendTXT(num, jsonString);
  Serial.println(message);
  Serial.println("Estado servo: " + servoString);
}
