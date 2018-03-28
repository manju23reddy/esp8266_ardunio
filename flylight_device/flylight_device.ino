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

/*
  void setFirebaseHost(char* fbHost){
    mFirebase_host = fbHost;
  }
  
  void setFirebaseAuth(char* fbAuth){
    mFirebase_auth = fbAuth;
  }
  */
  
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

char* USER_ID;

class UserDeatils{
  private:
  const char* mUser_UID;


  public:

  UserDeatils(){
    
  }

  UserDeatils(const char* UUID){
    mUser_UID = UUID;
  }
/*
  void setUserUID(char* UUID){
     mUser_UID = UUID;
  }*/
  
  const char* getUserUID(){
    return mUser_UID;
  } 
  
};

class SmartDeviceConfig{
  private:
  const char* mListenUrl;
  const char* mListenNode;
  int mOUT_LED;
  

  public:
  SmartDeviceConfig(){
    
  }

  SmartDeviceConfig(const char* listenUrl,const  char* listenNode, int OUT_LED){
    mListenUrl = listenUrl;
    mListenNode = listenNode;
    mOUT_LED = OUT_LED;
    pinMode(mOUT_LED, OUTPUT);
  }

  const char* getListenUrl(){
    return mListenUrl;
  }

  const char* getListenNode(){
    return mListenNode;
  }

  int getOUT_LED(){
    return mOUT_LED;
  }
  /*
  void setSwitchArea(char* swithArea){
    mSwitchArea = swithArea;
  }

  void setSwitchName(char* switchName){
    mSwithName = switchName;
  }*/

  void setOUT_LED(int state){
    digitalWrite(mOUT_LED, state);
  }
  
};

UserDeatils * userConfig = NULL;
FirebaseConfig * fbConfig = NULL;
SmartDeviceConfig * thisDeviceConfig = NULL;
 
void setup() {
    Serial.begin(115200);
    pinMode(BUILTIN_LED, OUTPUT);
    Serial.println("");
 
    // setup WiFi
    // no need to config SSID and password
    // turn on Auto Connect
    WiFi.mode(WIFI_STA);
#if 0
        if(WiFi.getAutoConnect() == false){
        WiFi.setAutoConnect(true);
    }
    WiFi.setAutoReconnect(false);
#else
    WiFi.setAutoConnect(false);
#endif
 
    // start WiFi connecting
    int cnt = 0;
    while (WiFi.status() != WL_CONNECTED) {
        // onboard LED flash twice quickly
        // UART output "..." 
        // ESP8266 is trying to connect to a known WiFi
        status_blink();
        delay(100);
        status_blink();
        delay(750);
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
 
void loop() {
  if (NULL == fbConfig && NULL == userConfig && NULL == thisDeviceConfig){
   listen_udp();
  }
  else{
    //read from firebase
     FirebaseObject curObj = Firebase.get(thisDeviceConfig->getListenUrl());
    if (Firebase.success()){
      Serial.println("Success");  
      int state = curObj.getInt(thisDeviceConfig->getListenNode());
      thisDeviceConfig->setOUT_LED(state);
      Serial.println(state);
      delay(200);
    }
    else{
       Serial.println("Firebase get failed");
      Serial.println(Firebase.error());
      return;
    }
  }
 
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
    
    Serial.printf("UDP packet contents: %s\n", incomingPacket);
#if 0
    Serial.printf("UDP packet contents:");

    
    for(int i = 0; i < len; i++)
    {
      if(incomingPacket[i] >= 0 && incomingPacket[i]<= 9)
      {
          Serial.write(incomingPacket[i] + '0');
        }
        else if(incomingPacket[i] >= 10 && incomingPacket[i] <= 15)
        {
          Serial.write(incomingPacket[i] - 10 + 'a');
          }
    } 
    
#endif

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
      const char * ID = root[FIREBASE_USER_UID];
      userConfig = new UserDeatils(ID);
      
      thisDeviceConfig = new SmartDeviceConfig(root[FIREBASE_LISTEN_URL], root[FIREBASE_LISTEN_NODE], root[FIREBASE_DEVICE_LED_NUMBER] );           
      
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
      
      
    }

    Serial.printf("\n");   
    
  }
  delay(500);
}
 
void status_blink(){
    BUILTIN_LED_ON();
    delay(50);
    BUILTIN_LED_OFF();
}
