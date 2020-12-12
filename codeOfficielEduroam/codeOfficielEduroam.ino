/*********************************** CODE DESCRIPTION ****************************************/
/*
 * Author: Xavier Marineau
 * Date: 19-10-2020
 * Version: 1
 * Purpose of sketch: This sketch allows the user to control a Dynamixel AX 12A  or a Dynamixel AX 18A digital servo motor 
 *                    through the subscription of an adafruit.io feed. 
 * Instructions: This sketch is aimed to work with a esp32 developpement kit microcontroler, but can be adapted to any other 
 *               microcontroler which can connect to WiFi. 
 *               Before using, the Dynamixel AX 18A digital servo motor's baud rate should previously have been changed to 
 *               115 200 bps. 
 *               Enter WiFi name and password where indicated. 
 *               Enter Adafruit.io feed name and feed key where indicated.
 *               Enter the Tx GPIO number .
 *               Enter motor ID where indicated.     
*/
//Libraries 
#include <WiFi.h>
#include <SPI.h>
#include "esp_wpa2.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "Arduino.h"
#include "AX12A.h"

//Define variables relative to WiFi and Adafruit connexion
const char* ssid = "eduroam";                               // Eduroam SSID
#define EAP_IDENTITY "USERNAME"                             // identity@youruniversity.domain
#define EAP_PASSWORD "PASSWORD"                             // your Eduroam password
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "motor_control"                     // Enter the adafruit.io username
#define AIO_KEY         "aio_roSb20kdZfyLZTYq9cQAL0c7w5Qz"  // Enter the feed's key

//Define variables relative to motor
#define DirectionPin   (1u)                                 // Enter Tx GPIO number
#define BaudRate      (115200ul)
#define ID        (1u)                                      // Enter motor ID

//Creating wifi client and subscribtion to adafruit server
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe voice_control = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/voice_control");

//Enableling functions
void MQTT_connect();
int calculate_speed(String feed);
int calculate_position(String feed);
void connect_to_eduroam(const char* ssid);

//Other variables
String feed;
int bitSpeed = 0;
int bitPosition;

/*********************************** SETUP ****************************************/
void setup() {
  Serial.begin(115200);
  
  //Wifi connexions
  connect_to_eduroam(ssid);

  //MQTT subscription
  mqtt.subscribe(&voice_control);
}



/*********************************** LOOP ****************************************/
void loop() {
  MQTT_connect(); 
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &voice_control) {     
      feed = String((char *)voice_control.lastread);
      Serial.print(F("Got: "));
      Serial.println(feed);
      delay(20);
      //The feed directs what action will be commanded to the motor   

      
      if (feed.compareTo("on") == 0){
        ax12a.begin(BaudRate, DirectionPin, &Serial);
        delay(100);
        ax12a.setEndless(ID, ON);
        ax12a.ledStatus(ID, ON);
        ax12a.turn(ID, LEFT, 512); //Default speed is around 48 RPM
        Serial.flush();
        delay(20);
        Serial.println(" ");
        Serial.println("Motor on");
      }
      
      if (feed.compareTo("off") == 0){
        ax12a.setEndless(ID, OFF);
        ax12a.ledStatus(ID, OFF);
        Serial.flush();
        delay(20);
        Serial.println(" ");
        Serial.println("Motor off");
      }
      
      if (feed[0] == 's'){
        Serial.println("Entered speed loop");
        delay(20);
        bitSpeed = calculate_speed(feed);
        Serial.print("The bit speed is: ");
        Serial.println(bitSpeed);
        ax12a.begin(BaudRate, DirectionPin, &Serial);
        delay(100);
        ax12a.setEndless(ID, ON);
        ax12a.ledStatus(ID, ON);
        ax12a.turn(ID, LEFT, bitSpeed); 
      }
      
      if(feed[0] == 'd'){
        Serial.println("Has entered position loop");
        ax12a.setEndless(ID, OFF);
        bitPosition = calculate_position(feed);
        ax12a.begin(BaudRate, DirectionPin, &Serial);
        delay(100);
        ax12a.move(ID, bitPosition);
        Serial.flush();
        Serial.println("Has mooved to target position");
      }
     } 
    }
  }




/*********************************************** FUCTIONS *******************************************************/

//Function to connect to eduroam WiFi. This function takes enables the device to work on WiFi such as eduroam. 
//The code was inspired by martinius96 and is available at https://gist.github.com/martinius96/8579d9a5e7f9ab367a6f6b88359520bd.
// 
void connect_to_eduroam(const char* ssid){
  byte error = 0;
  Serial.println("Connecting to: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  error += esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  error += esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  error += esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //Following times, it ran fine with just this line (connects very fast).
  if (error != 0) {
    Serial.println("Error setting WPA properties.");
  }
  WiFi.enableSTA(true);
  
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
  if (esp_wifi_sta_wpa2_ent_enable(&config) != ESP_OK) {
    Serial.println("WPA2 Settings Not OK");
  }
  
  WiFi.begin(ssid); //connect to Eduroam function
  WiFi.setHostname("RandomHostname"); //set Hostname for your device - not neccesary
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: ");
  Serial.println(WiFi.localIP()); //print LAN IP
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
// Function written without any serial port writtings

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}




//
//This funtion transforms rpm speed into a 0-1023 digit
//
int calculate_speed(String feed){
  int bitSpeed = 0;
  int rpmSpeed = 0; 
  if (feed[1] != 'n'){
    if (feed.length() <= 3){
      for (int i = feed.length(); i > 1 ; i--){
        rpmSpeed = rpmSpeed + (feed[i-1] - 48)* pow(10, (feed.length()-i));
      }

      if (rpmSpeed > 97){
        Serial.println("This speed is too high, please enter lower speed");
        Serial.flush();
        delay(20);
        bitSpeed = 1023;
      }
      
      else{
        bitSpeed = (rpmSpeed*1023)/97;
        Serial.print("The bit speed is: ");
        Serial.println(bitSpeed);
        delay(20);
      }
      return bitSpeed;  
    }
   }

  if (feed[1] == 'n'){
    if (feed.length() <= 4){
      for (int i = feed.length(); i > 2 ; i--){
        rpmSpeed = rpmSpeed + (feed[i-1] - 48)* pow(10, (feed.length()-i));
      }

      if (rpmSpeed > 97){
        Serial.println("This speed is too low, please enter lower speed");
        Serial.flush();
        delay(20);
        bitSpeed = 1023;
      }
      
      else{
        bitSpeed = (rpmSpeed*1023)/97;
        Serial.print("The bit speed is: ");
        Serial.println(bitSpeed);
        delay(20);
      }
      return bitSpeed;  
    }
  }
}



//
//This funtion transforms angle position into a 0-1023 digit
//
int calculate_position(String feed){  
  int bitPosition = 0;
  int anglePosition = 0;
  if(feed[1] != 'n'){
    if(feed.length() <= 4){
      for (int i = feed.length(); i > 1 ; i--){
        anglePosition = anglePosition + (feed[i-1] - 48)* pow(10, (feed.length()-i));
      }
      
      bitPosition = 1023*anglePosition/300;
      if(bitPosition > 1023){
        Serial.print("The angle is too high, postion set at 150 deg");
        delay(20);
        bitPosition = 1023;
        return bitPosition;
      }
      
      else{
        Serial.print("The bit angle is: ");
        Serial.println(bitPosition);
        delay(20);
        return bitPosition;
      }
    }
  }
}
