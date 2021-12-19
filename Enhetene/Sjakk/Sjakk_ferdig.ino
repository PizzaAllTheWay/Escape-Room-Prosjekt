// Code --------------------------------------------------
#include "UbidotsEsp32Mqtt.h"
#include <Wire.h>
#include <ICM20948_WE.h>

#define SDA 21
#define SCL 22

#define IMU_ADDR 0x68

// Variables --------------------------------------------------
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
char requestTimeoutArray[] = {}; /* Time ESP32 will sleep for (in seconds) */
int requestTimeout = 10;

// WiFi variables
const char *WIFI_SSID = "ChoudhryAirlines2"; // Wi-Fi name
const char *WIFI_PASS = "TalhaHarStorKUK"; // Wi-Fi password

// Ubidots conection variab'1les
const char *UBIDOTS_TOKEN = "BBFF-0OuPu7XtnaQxC8Dm4RGzKggbtQuaXS"; // Put here your Ubidots TOKEN


// |----------------------------------------------------------------------------------------------|
// |-------------------------------------- TIMER ID (START) --------------------------------------|
// |----------------------------------------------------------------------------------------------|
const char *TimerID = "sjakk-timeout"; // Replace with the timer ID of this node
const char *DEVICE_LABEL = "hoved-esp32"; // Replace with the device label to subscribe to
const char *MessageLabel = "state-sjakk"; // Replace with the device label to subscribe to
// |----------------------------------------------------------------------------------------------|
// |-------------------------------------- TIMER ID (START) --------------------------------------|
// |----------------------------------------------------------------------------------------------|



// Initialize ubidots instance
Ubidots ubidots(UBIDOTS_TOKEN);

ICM20948_WE imu = ICM20948_WE(IMU_ADDR);

void IMUConfig(){
  Wire.begin(SDA, SCL, 100000);
  while(!Serial){}

  if(!imu.init()){
    Serial.println("ICM20948 does not respond");
  }
  else{
    Serial.println("ICM20948 is connected");
  }

  if(!imu.initMagnetometer()){
    Serial.println("Magnetometer does not respond");
  }
  else{
    Serial.println("Magnetometer is connected");
  }

  imu.setMagOpMode(AK09916_CONT_MODE_20HZ);
}

float magRead(){
  imu.readSensor();
  xyzFloat magValue = imu.getMagValues();
  return magValue.z;
}

// Functions --------------------------------------------------
// Callback function to get information
void callback(char *topic, byte *payload, unsigned int length)
{
  // reset array and set it at payload lenght
  requestTimeoutArray[length] = {};

  // Read message from ubidots
  for (int i = 0; i < length; i++)
  {
    requestTimeoutArray[i] = (char)payload[i];
  }

  // Conver timout array to int
  requestTimeout = atoi(requestTimeoutArray);
  
  // Debug
  Serial.print("[");Serial.print(topic);Serial.print("] ");
  Serial.print(requestTimeout);
  Serial.println();
}



// Setup --------------------------------------------------
/*
* Setup will be reset everytime we go from deepsleep
* Loop WIl never run
*/
void setup()
{
  // Debug
  Serial.begin(115200);
  ubidots.setDebug(true);

  // Conect to wifi
  ubidots.connectToWifi(WIFI_SSID, WIFI_PASS);

  // Conect to ubidots
  ubidots.setCallback(callback); // Get a calback to se values we get
  ubidots.setup(); // Set up ubidots
  ubidots.reconnect();
  ubidots.subscribeLastValue(DEVICE_LABEL, TimerID); // Insert the dataSource and Variable's Labels

  /*
  * Have a delay at startup everytime because:
  * There is a BUG in ESP32 that sometimes when it goes to deep sleep
  * ESP32 will NOT wake up and then u r FUUUUKKKED!!!
  * Delay will perevent this by seting upp everything in the ESP32 properly
  */
  delay(500);

  // Connect to ubidots until succession
  while (!ubidots.connected()) {
    ubidots.reconnect();
    Serial.println("DEBUG: reconnecting to Ubidots");
  }

  // Get timeout value
  ubidots.subscribeLastValue(DEVICE_LABEL, TimerID);

  /*
   * Refresh ubidots values
   * We refresh 2 times VERRY IMPORTNAT!
   * Why 2 times? IDFK! it works and that is what matters XD
   */
  ubidots.loop();
  ubidots.loop();



  // |---------------------------------------------------------------------------------------------|
  // |----------------------------------- INFINITE LOOP (START) -----------------------------------|
  // |---------------------------------------------------------------------------------------------|
  Serial.println("#--------------------------------------------------#");

  IMUConfig();
  // If ESP gets active mode request (0s timeout), activate loop
  while (requestTimeout == 0) {
    // Variables ----------
    float magVal = magRead();
    
    // FERDIG
    // Sending values to ubidots
    if(abs(magVal) > 4700){
      int message = 2;
      ubidots.add(MessageLabel, message); // Insert your variable Labels and the value to be sent
      ubidots.publish(DEVICE_LABEL);

      delay(50);
      // Set timer to sleep for long time because we are done with the task
      requestTimeout = 100;
      
      // Debug
      Serial.print("Message Sent: ");Serial.println(message);
    }   
  }
  Serial.println("#--------------------------------------------------#"); 
  // |---------------------------------------------------------------------------------------------|
  // |----------------------------------- INFINITE LOOP (STOP) ------------------------------------|
  // |---------------------------------------------------------------------------------------------|


  
  /*
  * Enable deep sleep timer so that ESP32 will go out of deep sleep after a while
  * Go to deep sleep and start the deep sleep timer
  */
  esp_sleep_enable_timer_wakeup(requestTimeout * uS_TO_S_FACTOR);//requestTimeout * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}



// Loop that wil never run --------------------------------------------------
void loop(){
  
}
