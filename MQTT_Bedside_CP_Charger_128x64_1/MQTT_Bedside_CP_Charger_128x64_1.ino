#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

// Update these with values suitable for your network.
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* mqtt_user = "";
const char* mqtt_pass = "";
char* mqtt_pub_topic = "bedside_cp";
char* mqtt_pub_button_status = "/button";


char myIP[16];
long lastReconnectAttempt = 0;
const int LDR = A0; // Defining LDR PIN 
const int PCS = D0; // Define the phone charger status pin

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long debounceDelay = 100;
unsigned long intervalChargeStatusUpdate = 1000;
unsigned long intervalUpdate = 30000;
unsigned long intervalScreenDim = 1000; 

const int btnCount = 6;
int btns[btnCount] = {0,2,14,12,13,10};
bool btnstates[btnCount] = {1,1,1,1,1,1};
bool lastChargeStatus = false;
bool btntriggered = true;
unsigned long lastDebounceTime[btnCount] = {0,0,0,0,0,0};
unsigned long lastChargeStatusUpdate = 0;
unsigned long lastIntervalUpdate = 0;
unsigned long lastIntervalScreenDim = 0;

String ti = "-:--";
String ap = "--";
String t = "-.-";
String r = "0";

void setup() {
  
  Serial.begin(115200);
  
  initDisplay();

  for (int thisbtn = 0; thisbtn < btnCount; thisbtn++) {
    pinMode(btns[thisbtn], INPUT_PULLUP);
  }

  pinMode(PCS, INPUT);

  pinMode(LDR, INPUT);
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  lastReconnectAttempt = 0;
  
  btntriggered = true;
  
  delay(1000);

  updateDisplay(ti, ap, t, r);

}

void loop() {

  for (int thisbtn = 0; thisbtn < btnCount; thisbtn++) {
    int reading = digitalRead(btns[thisbtn]);
    if (reading != btnstates[thisbtn]) {
      Serial.print("event: ");
      Serial.print(thisbtn);
      Serial.print(" ");
      if ((millis() - lastDebounceTime[thisbtn]) > debounceDelay) {
        Serial.println("triggered");
        btntriggered = true;
      }
      else{
        Serial.println("");
      }
      lastDebounceTime[thisbtn] = millis();
    }
    btnstates[thisbtn] = reading;
  }

  if ((millis() - lastChargeStatusUpdate) > intervalChargeStatusUpdate) {
    Serial.println("Checking Charging Status");
    int reading = digitalRead(PCS);
    if (reading != lastChargeStatus){
      lastChargeStatus = reading;
      btntriggered = true;
    }
    lastChargeStatusUpdate = millis();
  }

  if ((millis() - lastIntervalUpdate) > intervalUpdate) {
    Serial.println("timmer triggered");
    btntriggered = true;
    lastIntervalUpdate = millis();
  }

  if (btntriggered == true){
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      // Client connected
      char pubstring[21];
      for (int i = 0; i < btnCount; i++){
        sprintf(pubstring,"%s%s%i", mqtt_pub_topic, mqtt_pub_button_status, (i+1));
        if (btnstates[i] == 1){
          client.publish(pubstring, "Off");
        }else{
          client.publish(pubstring, "On");
      }
      sprintf(pubstring,"%s%s%i", mqtt_pub_topic, mqtt_pub_button_status, btnCount+1);
      if (lastChargeStatus == 1){
        client.publish(pubstring, "On");
      }else{
        client.publish(pubstring, "Off");
      }
      updateDisplay(ti, ap, t, r);
      btntriggered = false;
    }
    }
  }
  
  if ((millis() - lastIntervalScreenDim) > intervalScreenDim) {
        Serial.print("setting screen brightness:  ");
        dimDisplay();
        lastIntervalScreenDim = millis();
      }
  
  client.loop();
}


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  sprintf(myIP, WiFi.localIP().toString().c_str());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(myIP);
  
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(24, 42);
  display.println(myIP);

  display.display(); 
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
  
  if (String(topic) == "environment/time"){
    ti="";
    for (int i = 0; i < length; i++) {
      if((i==0) && ((char)payload[i] == '0')){
        ti.concat(" ");
      }else{
        ti.concat((char)payload[i]);
      }
    }
  }
  
  if (String(topic) == "environment/tap"){
    ap="";
    for (int i = 0; i < length; i++) {
      ap.concat((char)payload[i]);
    }
  }

  if (String(topic) == "weather/temperature"){
    t="";
    for (int i = 0; i < length; i++) {
      t.concat((char)payload[i]);
    }
  }

  if (String(topic) == "weather/raining"){
    r="";
    for (int i = 0; i < length; i++) {
        r.concat((char)payload[i]);
    }
  }
  
  updateDisplay(ti, ap, t, r);
  
}

boolean reconnect() {

    if (client.connect(mqtt_pub_topic, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe("environment/time");
      client.subscribe("environment/tap");
      client.subscribe("weather/temperature");
      client.subscribe("weather/raining");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
    return client.connected();
}

void initDisplay(){

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(15, 30);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(F("Initializing WIFI"));

  display.display();
}

void updateDisplay(String stime, String sap, String temp, String rain){
  display.clearDisplay();

  //display.setTextSize(1);
  //display.setTextColor(WHITE);
  //display.setCursor(0, 0);
  //display.println(sdate);
  display.setTextSize(1);
  display.setFont(&FreeSansBold18pt7b);
  display.setTextColor(WHITE);
  display.setCursor(0, 60);
  display.println(stime);
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(WHITE);
  display.setCursor(90, 60);
  display.println(sap);
  display.setCursor(65, 16);
  display.setFont(&FreeSans9pt7b);
  String fullTemp = temp + "F";
  display.println(fullTemp);
  String rainStatus = "";
  if (rain == "0"){
    rainStatus = " ";
  }else{
    rainStatus = "Rain";
    display.drawRoundRect(0, 0, 47, 21, 4, WHITE);
  }

  if (lastChargeStatus == true){
    display.fillCircle(105, 33, 10, WHITE);
    display.fillCircle(105, 33, 7, BLACK);
    display.fillCircle(105, 33, 4, WHITE);
  }
  
  display.setCursor(5, 16);
  display.setFont(&FreeSans9pt7b);
  display.println(rainStatus);
  
  display.display();
}

void dimDisplay(){

  int l = analogRead(LDR);
  int contrast = map(l,350,1024, 0, 157);
  int precharge = map(l,350,1024, 0, 34);

  if (contrast < 0){
    contrast = 0;
  }
  if (precharge < 0){
    precharge = 0;
  }
  
  Serial.print("LDR: ");
  Serial.print(l);
  Serial.print(" C: ");
  Serial.print(contrast);
  Serial.print(" PC: ");
  Serial.println(precharge);
  
  display.dim(true);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
  
  display.ssd1306_command(SSD1306_SETPRECHARGE);
  display.ssd1306_command(precharge);
  //delay(100);
}
