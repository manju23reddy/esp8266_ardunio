// INCLUDE ESP WIFI lIBs
#include <ESP8266WiFi.h>
#if 0
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#endif

#include <WiFiUdp.h>

#include <EEPROM.h>

#include <FirebaseArduino.h>

#include <ArduinoJson.h>

WiFiUDP Udp;
unsigned int localUdpPort = 4210;  // local port to listen on


const int INCOMING_PACKET_SIZE = 2048;
char incomingPacket[INCOMING_PACKET_SIZE];
char replyOK[] = "OK";
char replyFAIL[] = "FAIL";

#define FIREBASE_PROJECT_URL  "FB_HOST_URL"
#define FIREBASE_AUTH_KEY  "FB_AUTH_KEY"
#define FIREBASE_USER_UID  "USER_UID"
#define FIREBASE_LISTEN_URL  "LISTEN_URL"
#define FIREBASE_LISTEN_NODE  "LISTEN_NODE"
#define FIREBASE_DEVICE_LED_NUMBER "LED_NUM"
#define FIREBASE_CONN_STATE_SUCCESS  0
#define FIREBASE_CONN_STATE_FAILED  -1

 
// PIN DEFINE
#define BUILTIN_LED 2     // ESP-12E module's onboard LED, used as status indicator
#define BUILTIN_LED_OFF() digitalWrite(BUILTIN_LED, HIGH)
#define BUILTIN_LED_ON()  digitalWrite(BUILTIN_LED, LOW)


// Actuators
#define actuator_LOAD 12
#define actuator_PWM 15

// Sensors
#define sensor_K1 2
#define sensor_K2 0
#define sensor_PIR 16
#define sensor_Intensity  0

// Controls
bool control_is_manual = false;

// Data
int actuator_LOAD_value = false;

class FirebaseConfig{

  private:
  const char* mFirebase_host;
  const char* mFirebase_auth;
  int CONN_STATE;

  public:
  FirebaseConfig(){
    
  }
  
  FirebaseConfig(const char* firebase_host, const char* firebase_auth){
    mFirebase_host = firebase_host;
    mFirebase_auth = firebase_auth;
    CONN_STATE= FIREBASE_CONN_STATE_FAILED;
  }

  
  const char* getFirebaseHost(){
    return mFirebase_host;
  }
  
  const char* getFirebaseAuth(){
    return mFirebase_auth;
  }

  void setConnState(int state){
    CONN_STATE = state;
  }

  int getConnState(){
    return CONN_STATE;
  }
};

class SmartDeviceConfig{
  private:
  const char* mListenUrl;
  const char* mListenNode;
  const char* mDeviceIntensity;
  const char* mActivateByMotion;

  const char* mControlByLigthSensor;
  int mOUT_LED;
  

  public:
  SmartDeviceConfig(){
    
  }

  SmartDeviceConfig(const char* listenUrl){
    mListenUrl = listenUrl;
    mListenNode = "deviceState";
    mDeviceIntensity = "deviceIntensity";
    mActivateByMotion = "activateonmotion";
    mControlByLigthSensor = "lightintensitybyambient";
    

    
  }

  const char* getControlByLight(){
    return mControlByLigthSensor;
  }

  const char* getListenUrl(){
    return mListenUrl;
  }

  const char* getListenNode(){
    return mListenNode;
  }

  const char* getIntensityNode(){
    return mDeviceIntensity;
  }

  const char* getActivateByMotion(){
    return mActivateByMotion;
  }

  void setOUT_LED(int state){
    digitalWrite(actuator_LOAD, state);
  }

  
 
  
};

FirebaseConfig * fbConfig = NULL;
SmartDeviceConfig * thisDeviceConfig = NULL;

void status_blink(){
    BUILTIN_LED_ON();
    delay(50);
    BUILTIN_LED_OFF();
}






void readEEPROM(){

  EEPROM.begin(INCOMING_PACKET_SIZE);
  Serial.print("Reading from EEPROM\n");
  char isAvail = EEPROM.read(0);
  char * data = NULL;
  if ('Y' == isAvail){
    Serial.println(isAvail);
    data = (char*)malloc(sizeof(char)+(INCOMING_PACKET_SIZE));
    char c;
    int j = 1;
    int i = 0;
    while( ( c = EEPROM.read(j)) != '\0'){
      data[i] = c;
      i++;
      j++;
    }
    Serial.println(data);
    if (NULL != data){
      data[i] = '}';
      Serial.printf("Data is %s\n", data);
  
      StaticJsonBuffer<INCOMING_PACKET_SIZE> jsonBuffer;
    
      JsonObject& root = jsonBuffer.parseObject(data);
    
      // send back a reply, to the IP address and port we got the packet from
      fbConfig = new FirebaseConfig(root[FIREBASE_PROJECT_URL], root[FIREBASE_AUTH_KEY]);
    
      const char * listenUrl = root[FIREBASE_LISTEN_URL];
      thisDeviceConfig = new SmartDeviceConfig(listenUrl); 
    } 
    
  }
  
}

void clean(){
   EEPROM.begin(INCOMING_PACKET_SIZE);
    for (int i = 0; i < INCOMING_PACKET_SIZE; i++){
      EEPROM.write(i, '\0');
    }
  
    EEPROM.commit();
    EEPROM.end();
}
 
void setup() {

   pinMode(actuator_LOAD,OUTPUT);
   digitalWrite(actuator_LOAD,0);

   analogWriteRange(1024);
   analogWriteFreq(1000);

   pinMode(sensor_K1, INPUT_PULLUP);
   pinMode(sensor_K2, INPUT_PULLUP);
  
    Serial.begin(115200);
    pinMode(BUILTIN_LED, OUTPUT);
    Serial.println("");
 
    // setup WiFi
    // no need to config SSID and password
    // turn on Auto Connect

   
          
   clean();
   
   readEEPROM();
   
   
    if (NULL == fbConfig || NULL ==  thisDeviceConfig){
          configWifi();
    }
    else{
      WiFi.mode(WIFI_STA);   
      WiFi.setAutoConnect(true);   
      delay(1000);    
      Firebase.begin(fbConfig->getFirebaseHost(), fbConfig->getFirebaseAuth());
      if(Firebase.failed()){
        Serial.println("Firebase auth failed");
        Serial.println(Firebase.error());
      }
      else{
        Serial.println("Success");  
        
      }
        
      
    }
     
}

void configWifi(){

  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(false);

  // start WiFi connecting
          int cnt = 0;
          while (WiFi.status() != WL_CONNECTED) {
              // onboard LED flash twice quickly
              // UART output "..." 
              // ESP8266 is trying to connect to a known WiFi
              status_blink();
              delay(100);
              status_blink();
              delay(50);
              Serial.print(".");
       
              // if no connection available after timeout
              if (cnt++ >= 10) {
                  Serial.println("");
                  WiFi.beginSmartConfig();
                  while (1) {
                      // onboard LED flash slowly
                      // UART output "|||" 
                      // ESP8266 is in smartconfig loop
                      // use app to finish the smartconfig
                      status_blink();
                      delay(900);
                      
                      if (WiFi.smartConfigDone()) {
                          Serial.println();
                          Serial.println(F("[WIFI] SmartConfig: Success"));
      
                            Serial.print("ssid:");
                            Serial.println(WiFi.SSID());
                            Serial.print("password:");
                            Serial.println(WiFi.psk());
        
                          break;
                      }
                      Serial.print(F("|"));
                  }
              }
              BUILTIN_LED_OFF();
          }
       
          Serial.println("");
          Serial.println(F("WiFi connected"));  
          Serial.print(F("IP address: "));
          Serial.println(WiFi.localIP());
       
          WiFi.printDiag(Serial);
       
          Serial.print(F("RSSI: "));
          Serial.println(WiFi.RSSI());
          Serial.print(F("BSSID: "));
          Serial.println(WiFi.BSSIDstr());
      
          Udp.begin(localUdpPort);
          Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

          
}

#define CONTROL_AUTOMATICALLY 1
#define CONTROL_MANUALLY 2

void loop() {
  
  if (NULL == thisDeviceConfig || NULL == fbConfig){
    listen_udp();
  }
  else{ 
      //read from firebase
      if (WiFi.status() != WL_CONNECTED){
        Serial.println("Wifi is not connected");
        delay(200);
      }
      else{
      Serial.println(thisDeviceConfig->getListenUrl());
      Serial.println("\n");
      FirebaseObject curObj = Firebase.get(thisDeviceConfig->getListenUrl());
      if (Firebase.success()){
        Serial.println("Success");
        Serial.println(thisDeviceConfig->getListenNode());

       
       

        int control_intensity_by_light = curObj.getInt(thisDeviceConfig->getControlByLight());
        if (control_intensity_by_light > 0){
          int adc = analogRead(A0);          
          char intesityNode[512];
          sprintf(intesityNode, "%s/%s", thisDeviceConfig->getListenUrl(), thisDeviceConfig->getIntensityNode());
          Firebase.setInt(intesityNode, adc);
          analogWrite(actuator_PWM, adc);

          if (adc > 300){
            char LEDNODE[512];
            sprintf(LEDNODE, "%s/%s", thisDeviceConfig->getListenUrl(), thisDeviceConfig->getListenNode());
            Firebase.setInt(LEDNODE, 1);
          }
          else{
            char LEDNODE[512];
            sprintf(LEDNODE, "%s/%s", thisDeviceConfig->getListenUrl(), thisDeviceConfig->getListenNode());
            Firebase.setInt(LEDNODE, 0);
          }
        }
        else{
          int current_intensity = curObj.getInt(thisDeviceConfig->getIntensityNode());
          analogWrite(actuator_PWM, current_intensity);
        }

        int MAIN_LED_STATE = curObj.getInt(thisDeviceConfig->getListenNode());
        thisDeviceConfig->setOUT_LED(MAIN_LED_STATE);
        

     
      }
      else{
        Serial.println("Firebase get failed");
        //Serial.println(Firebase.error());
       
      }
    }
  }
  delay(500);
 
 
}

void listen_udp(){
   // put your main code here, to run repeatedly:

  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    // receive incoming UDP packets
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(incomingPacket, INCOMING_PACKET_SIZE);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }

    EEPROM.begin(INCOMING_PACKET_SIZE);
    for (int i = 0; i < INCOMING_PACKET_SIZE; i++){
      EEPROM.write(i, '\0');
    }
  
    EEPROM.commit();
    EEPROM.end();
    
    Serial.printf("UDP packet contents: %s\n", incomingPacket);
    EEPROM.begin(INCOMING_PACKET_SIZE);
    Serial.printf("data is %s\n",incomingPacket);
    int i = 0;
    int j = 0;
    EEPROM.write(0, 'Y');
    for (i = 1; i < len; i++, j++){
      EEPROM.write(i, incomingPacket[j]);      
    }
    delay(200);
    EEPROM.commit();
    EEPROM.end();
      
    Serial.printf("UDP packet contents: %s\n", incomingPacket);
    
    StaticJsonBuffer<INCOMING_PACKET_SIZE> jsonBuffer;

    JsonObject& root = jsonBuffer.parseObject(incomingPacket);

    if (!root.success()){
      
       // send back a reply, to the IP address and port we got the packet from
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(replyFAIL);
      Udp.endPacket();
  
      
    }
    else{
      // send back a reply, to the IP address and port we got the packet from
      fbConfig = new FirebaseConfig(root[FIREBASE_PROJECT_URL], root[FIREBASE_AUTH_KEY]);

      const char * listenUrl = root[FIREBASE_LISTEN_URL];
      thisDeviceConfig = new SmartDeviceConfig(listenUrl);           
      
       // send back a reply, to the IP address and port we got the packet from
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(replyOK);
      Udp.endPacket();

      Firebase.begin(fbConfig->getFirebaseHost(), fbConfig->getFirebaseAuth());
      if(Firebase.failed()){
        Serial.println("Firebase auth failed");
        Serial.println(Firebase.error());
      }
      else{
        Serial.println("Success");  
        
      }
      Udp.stop(); 
      
     
      WiFi.setAutoConnect(true);
      
    }

    Serial.printf("\n");   
    
  }
  delay(500);
}




void loopSensor(){
  int adc = 0;
  int actuator_LOAD_value_temp = false;

  // Read Intesity
  adc = analogRead(A0);
  Firebase.setInt("sensor_Intensity", 1024 - adc);

  control_is_manual = Firebase.getInt("control_is_manual");
  if(control_is_manual == 0)
  {
    analogWrite(actuator_PWM, adc/5);
    Firebase.setInt("actuator_PWM", adc);

    if(adc > 300)
    {
      actuator_LOAD_value_temp = actuator_LOAD_value = true;
    }
    else
    {
      actuator_LOAD_value_temp = actuator_LOAD_value = false;
    }

    digitalWrite(actuator_LOAD, actuator_LOAD_value);
    Firebase.setInt("actuator_LOAD", actuator_LOAD_value);
  }
  else
  {
    if(actuator_LOAD_value)
    {
      analogWrite(actuator_PWM, 500);
      Firebase.setInt("actuator_PWM", 500);
    }
    else
    {
      analogWrite(actuator_PWM, 0);
      Firebase.setInt("actuator_PWM", 0);
    }
  }

  actuator_LOAD_value_temp = Firebase.getInt("actuator_LOAD");
  if(actuator_LOAD_value_temp != actuator_LOAD_value)
  {
    control_is_manual = true;
    actuator_LOAD_value = actuator_LOAD_value_temp;
    Firebase.setInt("control_is_manual", control_is_manual);

    digitalWrite(actuator_LOAD, actuator_LOAD_value);
  }

  if(digitalRead(sensor_K1))
  {
    Firebase.setInt("sensor_K1", 0);
    //Serial.println("K1 Off"); 
  }
  else
  {
    Firebase.setInt("sensor_K1", 1);
    Serial.println("K1 On"); 
  }

  if(digitalRead(sensor_K2))
  {
    Firebase.setInt("sensor_K2", 0);
    //Serial.println("K2 Off"); 
  }
  else
  {
    Firebase.setInt("sensor_K2", 1);
    Serial.println("K2 On"); 

    actuator_LOAD_value = !actuator_LOAD_value;
    digitalWrite(actuator_LOAD, actuator_LOAD_value);
    Firebase.setInt("actuator_LOAD", actuator_LOAD_value);

    control_is_manual = true;
    Firebase.setInt("control_is_manual", control_is_manual);
  }

  if(digitalRead(sensor_PIR))
  {
    Firebase.setInt("sensor_PIR", 1);
    Serial.println("Motion"); 
  }
  else
  {
    Firebase.setInt("sensor_PIR", 0);
  }

  if (Firebase.failed()) {
    Serial.println("Firebase get failed"); 
    Serial.println(Firebase.error()); 
  }
  
  delay(250);
}




 





