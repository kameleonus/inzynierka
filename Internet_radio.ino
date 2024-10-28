#include "Arduino.h"
#include "Audio.h"
#include <ezButton.h>
#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "coolverticarg12.h"
#include "FS.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include "driver/rtc_io.h"

#define SPIFFS_FAILED true
#define I2S_DIN 25
#define I2S_LCK 26
#define I2S_BCK 27
#define TFT_CS         15
#define TFT_RST        4
#define TFT_DC         2
#define BUTTON_NEXT 32 
#define BUTTON_BACK 13
#define USE_EXT0_WAKEUP       1 
#define WAKEUP_GPIO    GPIO_NUM_33 
#define DEBOUNCE_TIME_MS     50 
TFT_eSPI tft = TFT_eSPI();
Audio audio;
char* Stations[20][2] = {
  {"Jedynka", "http://mp3.polskieradio.pl:8900/;"},
  {"Dwójka", "http://mp3.polskieradio.pl:8902/;"},
  {"RMF FM", "http://195.150.20.242:8000/rmf_fm"},
  {"Radio ZET", "http://zet090-02.cdn.eurozet.pl:8404/;"},
  {"Radio Złote Przeboje", "http://poznan7.radio.pionier.net.pl:8000/tuba9-1.mp3"},
  {"VIA - Katolickie Radio Rzeszów", "http://62.133.128.18:8040/;"}
};

AsyncWebServer server(80);

const char* PARAM_INPUT_1 = "station";
const char* PARAM_INPUT_2 = "newName";
const char* PARAM_INPUT_3 = "newLink";
int numerator = 2;
int toSleep = 0;
unsigned long lastButtonPress = 0;             
bool buttonPressed = false;                      

ezButton goBackStation(BUTTON_BACK);
ezButton goNextStation(BUTTON_NEXT);

void writeFile(const char* filename) {
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  for (int i = 0; i < 20; i++) {
    if (Stations[i][0] == NULL) break;
    file.println(Stations[i][0]);
    file.println(Stations[i][1]);
  }

  file.close();
}

void readFile(const char* filename) {
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  int i = 0;
  while (file.available() && i < 20) {
    Stations[i][0] = strdup(file.readStringUntil('\n').c_str());
    Stations[i][1] = strdup(file.readStringUntil('\n').c_str());
    i++;
  }

  file.close();
}

String processor(const String& var) {
  String tableContent;
  int i = 0;
  tableContent = "<select name ='station'>\n";
  
  while (Stations[i][0] != NULL) {
    tableContent += "<option value='";
    tableContent += String(Stations[i][0]); 
    tableContent += "'>";
    tableContent += String(Stations[i][0]); 
    tableContent += "</option>";
    ++i;
  }
  tableContent += "</select>";
  return tableContent;
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Internet Radio</title>
  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      color: black;
      background-color: #f0f0f0;
      margin: 0;
      padding: 0;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }
    .container {
      background: #fff;
      padding: 20px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
      border-radius: 10px;
    }
    h1 {
      color: #333;
      text-align: center;
    }
    form {
      margin-bottom: 20px;
    }
    input[type="text"], input[type="submit"] {
      width: auto;
      max-width: 350px;
      padding: 10px;
      margin: 5px 0 10px 0;
      border: 1px solid #ccc;
      border-radius: 5px;
      box-sizing: border-box;
    }
    input[type="submit"] {
      background-color: #007BFF;
      color: white;
      border: none;
      cursor: pointer;
    }
    select{
      width: auto;
      padding: 10px;
      margin: 5px 0 10px;
      border: 1px solid #ccc;
      border-radius: 5px;
      box-sizing: border-box;
      max-width: 350px;
    }
    input[type="submit"]:hover {
      background-color: #0056b3;
    }
    .station-list {
      background-color: #fff; 
      border: 1px solid #ccc; 
      border-radius: 5px;
      padding: 10px; 
      margin-bottom: 20px; 
      box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1); 
    }
  </style>

</head>
<body>
  <div class="container">
    <h1>Internet Radio</h1>
    <form action="/get" class="station-list">
      <label for="stations">Station:</label><br>
      %tableContent%
      <input type="submit" value="Zmień stację">
    </form>
    <form action="/get">
      <h2>Dodanie nowej stacji</h2>
      <label for="newName">Nazwa:</label><br>
      <input type="text" id="newName" name="newName" required><br>
      <label for="newLink">Link:</label><br>
      <input type="text" id="newLink" name="newLink" required><br>
      <input type="submit" value="Dodaj stację">
    </form>
  </div>
</body>
</html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void changeStation(String name) {
  numerator = 0;
  while (Stations[numerator][0] != NULL) {
    if (name == Stations[numerator][0]) {
      audio.connecttohost(Stations[numerator][1]);
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(3, 5);
      tft.print(WiFi.localIP());
      tft.setCursor(3,20);
      tft.print(Stations[numerator][0]);
      tft.fillRect(0, 40, 160, 3, TFT_CYAN);    

      return;
    }
    ++numerator;
  }
}

char* StringToChar(String str) {
  char* charArray = new char[str.length() + 1];
  str.toCharArray(charArray, str.length() + 1);
  return charArray;
}

void setup() {
  Serial.begin(115200);
  pinMode(WAKEUP_GPIO, INPUT_PULLUP);
  buttonPressed = false; 

  goBackStation.setDebounceTime(50);
  goNextStation.setDebounceTime(50);
 
  if (!SPIFFS.begin(SPIFFS_FAILED)) {
    Serial.println("Failed to mount file system");
    return;
  }

  readFile("/Stations.txt");

  WiFiManager wm;
  audio.setPinout(I2S_BCK, I2S_LCK, I2S_DIN);
  audio.setVolume(10);
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(3);
  tft.loadFont(coolverticarg12);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextPadding(3);
  tft.setCursor(3,10);
  tft.print("Nazwa sieci: Radio Internetowe AP");
  tft.setCursor(3,34);
  tft.print("Haslo: password");
  bool res = wm.autoConnect("Radio Internetowe AP", "password");
  
  
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yeey :)");
    server.onNotFound(notFound);
    server.begin();

    audio.connecttohost(Stations[numerator][1]);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(3, 5);
    tft.print(WiFi.localIP());
      tft.setCursor(3,20);
      tft.print(Stations[numerator][0]);
      tft.fillRect(0, 40, 160, 2, TFT_CYAN);      
  }
    
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    String inputMessage;

    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      changeStation(inputMessage);
    } else if (request->hasParam(PARAM_INPUT_2)) {
      char* param1 = StringToChar(request->getParam(PARAM_INPUT_2)->value());
      if (request->hasParam(PARAM_INPUT_3)) {
        char* param2 = StringToChar(request->getParam(PARAM_INPUT_3)->value());
        int num = 0;
        while (Stations[num][0] != NULL) {
          ++num;
        }
        Stations[num][0] = param1;
        Stations[num][1] = param2;
        writeFile("/Stations.txt");
        Serial.println(num);
        Serial.println(param1);
        Serial.println(param2);
      }
    }
    request->redirect("/");
  });
}

void loop() {
  audio.loop();
  goBackStation.loop();
  goNextStation.loop();
  if(goBackStation.isPressed())
  {
    --numerator;
    if(numerator==-1){
      numerator=sizeof(Stations)/sizeof(Stations[0])-1;
    }
    audio.connecttohost(Stations[numerator][1]);
  }
  if(goNextStation.isPressed())
  {
    ++numerator;
    if(numerator==sizeof(Stations)/sizeof(Stations[0])){
      numerator=0;
    }
    audio.connecttohost(Stations[numerator][1]);
  }
  if (digitalRead(WAKEUP_GPIO) == LOW  && (millis() - lastButtonPress > DEBOUNCE_TIME_MS)) {
    buttonPressed = true;  
    lastButtonPress = millis();  
    
    esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0); 

   
    buttonPressed = false;
    delay(500);
    esp_deep_sleep_start();
  }
  
  
}

void audio_showstreamtitle(const char *info) {
  tft.fillRect(0, 44, 160, 70, TFT_BLACK);
  tft.print("\n");
  tft.setCursor(3, 50);
  Serial.print("\nTytół:\n");
  Serial.print(info);
  tft.print(info);
}
