#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

const uint8_t ledPin = 2;
Servo directionServo;  // create servo object to control a servo

// Recommended PWM GPIO pins on the ESP32 include 2,4,12-19,21-23,25-27,32-33
int servoPin = 13;
int servoState = 0;
JsonDocument doc;

AsyncWebServer server(80);
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

// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Document</title>
</head>
<body>
    <div style="text-align:center;">
        <h1>ESP32 CONTROL PANEL</h1>
        <h3>Motor</h3>
        <button onclick="turnMotorOn()">Start</button>
        <button onclick="turnMotorOff()">Stop</button>
        <br>
        <button onclick="motorForward()">Forward</button>
        <button onclick="motorBackward()">Backward</button>
        <br>
        <button onclick="motorDecrease()">Decrease</button>
        <button onclick="motorIncrease()">Increase</button>
        <br>
        Current speed: <span id="speed"></span><br>
        Direction: <span id="direction"></span><br>
        Motor state: <span id="motor_state"></span><br>
        <h3>Servo</h3>
        <button onclick="turnLeft()">Left</button>
        <button onclick="turnRight()">Right</button>
        <br>
        Valor del servo: <span id="servo"></span>
        <h3>Messages</h3>
        <span id='message'></span>
    </div>
    <script>
        let socketConnection = new WebSocket('ws://' + location.hostname + ':81');
        var somePackage = {};
        somePackage.connect = function()  {
            var ws = new WebSocket('ws://'+document.location.host+ ':81');
            ws.onopen = function() {
                console.log('ws connected');
                somePackage.ws = ws;
            };
            ws.onerror = function() {
                console.log('ws error');
            };
            ws.onclose = function() {
                console.log('ws closed');
            };
            ws.onmessage = function(msgevent) {
                var json_data = JSON.parse(msgevent.data);
                var msg = msgevent.data;
                if (json_data["servoState"]) {
                  document.getElementById("servo").innerHTML = json_data["servoState"];
                }
                if (json_data["message"]) {
                  document.getElementById("message").innerHTML = json_data["message"];
                }
                if (json_data["speed"]) {
                  document.getElementById("speed").innerHTML = json_data["speed"];
                }
                if (json_data["motorDir"]) {
                  document.getElementById("direction").innerHTML = json_data["motorDir"];
                }
                if (json_data["motorState"]) {
                  document.getElementById("motor_state").innerHTML = json_data["motorState"];
                }

                console.log('in :', msg);
                // message received, do something
            };
        };

        somePackage.send = function(msg) {
            if (!this.ws) {
                console.log('no connection');
                return;
            }
            console.log('out:', msg)
            //this.ws.send(window.JSON.stringify(msg));
            this.ws.send(msg);
        };
        somePackage.connect();
        const turnLeft = () => somePackage.send("Left");
        const turnRight = () => somePackage.send("Right");
        const turnMotorOn = () => somePackage.send("Motor_on");
        const turnMotorOff = () => somePackage.send("Motor_off");
        const motorForward = () => somePackage.send("Motor_forward");
        const motorBackward = () => somePackage.send("Motor_backward");
        const motorDecrease = () => somePackage.send("Motor_decrease");
        const motorIncrease = () => somePackage.send("Motor_increase");
    </script>
</body>
</html>)rawliteral";

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
  directionServo.setPeriodHertz(50);
  directionServo.attach(servoPin);
  //pinMode(ledPin, OUTPUT);
  delay(2000);
  WiFi.softAP("ControlBoat", "");
  Serial.println("\nControlBoat");
  Serial.println(WiFi.softAPIP());

  if (!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
    return;
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });
  server.onNotFound(notFound);
  server.begin();

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
  }
  else if (speed.equalsIgnoreCase("decrease") && dutyCycle > 200) {
    dutyCycle -= 5;
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

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Página no encontrada!");
}
