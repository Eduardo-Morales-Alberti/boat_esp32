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
        <h3>Servo</h3>
        <button onclick="turnLeft()">Left</button>
        <button onclick="turnRight()">Right</button>
        <br>
        Valor del servo: <span id='servo'></span>
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
    </script>
</body>
</html>)rawliteral";

void setup() {

  Serial.begin(115200);
  directionServo.setPeriodHertz(50); 
  directionServo.attach(servoPin);
  //pinMode(ledPin, OUTPUT);
  delay(2000);
  WiFi.softAP("ControlBoat", "");
  Serial.println("\nsoftAP");
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

void loop() {
  websockets.loop();
}

bool moveServo(String dir) {
  if (dir.equalsIgnoreCase("left") && servoState >= 5) {
    servoState -= 5;
    String servoString = (String) servoState;
    directionServo.write(servoState);
    Serial.println("LEFT: " + servoString );
  }
  else if (dir.equalsIgnoreCase("right") && servoState <= 175) {
   servoState += 5;
   String servoString = (String) servoState;
   directionServo.write(servoState);
   Serial.println("right: " + servoString );
  }
  else {
    return false;
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
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
        //digitalWrite(ledPin, HIGH);
        if(moveServo("left") == false) {
          String servoString = (String) servoState;
          sendMessage(num, "No se puede girar más a la izquierda: " + servoString);
        }
      }

      if (msg.equalsIgnoreCase("right")) {
        //digitalWrite(ledPin, LOW);
        if(moveServo("right") == false) {
          String servoString = (String) servoState;
          sendMessage(num, "No se puede girar más a la derecha: " + servoString);
        }
      }
      String servoString = (String) servoState;
      sendMessage(num, "Estado Servo: " + servoString);
      
  }
}

void sendMessage(uint8_t num, String message) {
  String servoString = (String) servoState;
  doc["message"] = message;
  doc["servoState"] = servoString;

  String jsonString;
  size_t len = serializeJson(doc, jsonString);
  Serial.println(jsonString);
  websockets.sendTXT(num, jsonString);
  Serial.println(message);
  Serial.println("Estado servo: " + servoString);
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Página no encontrada!");
}
