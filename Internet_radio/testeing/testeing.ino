#include "Arduino.h"
#include "Audio.h"
#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "coolverticarg12.h"

#define I2S_DIN 25
#define I2S_LCK 26
#define I2S_BCK 27
#define TFT_CS         15
#define TFT_RST        4
#define TFT_DC         2

TFT_eSPI tft = TFT_eSPI();

Audio audio;
char* Stacje[20][2] = {{"Jedynka","http://mp3.polskieradio.pl:8900/;"},{"Dwójka","http://mp3.polskieradio.pl:8902/;"},{"RMF FM","http://195.150.20.242:8000/rmf_fm"},{"Radio ZET","http://zet090-02.cdn.eurozet.pl:8404/;"},
{"Radio Złote Przeboje","http://poznan7.radio.pionier.net.pl:8000/tuba9-1.mp3"},{"VIA - Katolickie Radio Rzeszów","http://62.133.128.18:8040/;"}};

AsyncWebServer server(80);

const char* PARAM_INPUT_1 = "station";
const char* PARAM_INPUT_2 = "newName";
const char* PARAM_INPUT_3 = "newLink";
int numerator=2;
String processor(const String& var){
  String tableContent;
  int i=0;
   tableContent = "<select name ='station'>\n";
   
  while(Stacje[i][0]!=NULL) {
    tableContent += "<option value='";
    tableContent += String(Stacje[i][0]); 
    tableContent += "'>";
    tableContent += String(Stacje[i][0]); 
    tableContent += "</option>";
    ++i;
  }
  tableContent += "</select>";
  return tableContent;
  return String();
}
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Internet Radio</title>
  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    Station: 
    %tableContent%
    <input type="submit" value="Zmień stację">
  </form><br>
  <form action="/get">
    Dodanie nowej stacjii<br>
    Nazwa <input type = "text" name="newName"><br>
    Link <input type="text" name ="newLink"><br>
    <input type="submit" value="Dodaj stację">
  </form>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}
void changeStation(String name){
  numerator=0;
  while(Stacje[numerator][0]!=NULL){
    if(name==Stacje[numerator][0]){
      audio.connecttohost(Stacje[numerator][1]);
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
  WiFiManager wm;
  audio.setPinout(I2S_BCK, I2S_LCK, I2S_DIN);
  audio.setVolume(10);
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(3);
    tft.loadFont(coolverticarg12);
    tft.setTextSize(1);
    tft.setTextWrap(true);
  bool res;
  res = wm.autoConnect("Radio Internetowe AP","password");
  tft.println("Nazwa sieci: Radio Internetowe AP");
  tft.println("Haslo: password");
  if(!res) {
        Serial.println("Failed to connect");
        
    } 
    else {
          
        Serial.println("connected...yeey :)");
         server.onNotFound(notFound);
        server.begin();
  
  audio.connecttohost(Stacje[2][1]);    
    }
    
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html,processor);
  });

 server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    
    
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      changeStation(inputMessage);
    }
    else if(request->hasParam(PARAM_INPUT_2)){
     char* param1 = StringToChar(request->getParam(PARAM_INPUT_2)->value());
      if(request->hasParam(PARAM_INPUT_3)){
    char* param2 = StringToChar(request->getParam(PARAM_INPUT_3)->value());
      int num=0;
      while(Stacje[num][0]!=NULL){
        ++num;
      }
      Stacje[num][0]={param1};
      Stacje[num][1]={param2};
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

 
}
void audio_showstation(const char * info){
    tft.fillScreen(TFT_BLACK);tft.setCursor(0,0);Serial.print("\nStacja: ");tft.print("\n");Serial.print(info);tft.print(Stacje[numerator][0]);tft.drawRect(0,45,160,5,TFT_DARKCYAN);}
void audio_showstreamtitle(const char *info){
    tft.fillRect(0, 50, 160,  70, TFT_BLACK);tft.setCursor(100, 40);Serial.print("\nTytół:\n");tft.print("\n");Serial.print(info);tft.print(info);}

