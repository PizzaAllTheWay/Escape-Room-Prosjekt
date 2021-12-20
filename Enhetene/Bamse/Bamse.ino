// Code --------------------------------------------------
#include "UbidotsEsp32Mqtt.h"
#include <Wire.h>
#define G 17400
#define SDA 21
#define SCL 22 



// Variables --------------------------------------------------
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
char requestTimeoutArray[] = {}; /* Time ESP32 will sleep for (in seconds) */
int requestTimeout = 10;

// WiFi variables
const char *WIFI_SSID = "NAME"; // Wi-Fi name
const char *WIFI_PASS = "PASSWORD"; // Wi-Fi password

// Ubidots conection variables
const char *UBIDOTS_TOKEN = "BBFF-0OuPu7XtnaQxC8Dm4RGzKggbtQuaXS"; // Put here your Ubidots TOKEN

const int IMU_ADDR = 0x68;    //I2C address of the ICM20948

void i2cConfig() {
  Wire.begin(SDA, SCL, 100000);    //SDA, SCL, Clock speed
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x06);             //PWR_MGMT_1 register
  Wire.write(0);                //set to zero(wakes up the ICM20948)
  Wire.endTransmission(true);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x07);             //PWR_MGMT_2 register
  Wire.write(0x07);             //Gyroskopen slått av
  Wire.endTransmission(true);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x10);             //ACCEL_SMPLRT_DIV_1 register
  Wire.write(0x00);             //MSB av akselerometerets utgangsdatarate er satt
  Wire.endTransmission(true);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x11);             //ACCEL_SMPLRT_DIV_2 register
  Wire.write(0x1D);             //LSB av akselerometerets utgangsdatarate er satt
  Wire.endTransmission(true);
  Serial.println("Accelerometer sample rate configured successfully");
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x05);             //LP_CONFIG register
  Wire.write(0x20);             //Akselerometeret er satt i redusert lesefrekvens (redusert utgangsdatarate)
  Serial.println("ODR configured successfully at 37.5 Hz");
  Serial.println("Setup complete");
}

float accRead() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x2D);             //starting with register 0x2D (ACCEL_XOUT_H)
  Wire.endTransmission(true);
  Wire.beginTransmission(IMU_ADDR);
  Wire.requestFrom(IMU_ADDR, 6, true);   //request a total of 6 registers
  int16_t AcX=Wire.read()<<8|Wire.read();   //0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  int16_t AcY=Wire.read()<<8|Wire.read();   //0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  int16_t AcZ=Wire.read()<<8|Wire.read();   //0x3F (ACCEL_XOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Serial.println(sqrt((AcX*AcX) + (AcY*AcY) + (AcZ*AcZ))/G);
  return (sqrt((AcX*AcX) + (AcY*AcY) + (AcZ*AcZ))/G);
}

// |----------------------------------------------------------------------------------------------|
// |-------------------------------------- TIMER ID (START) --------------------------------------|
// |----------------------------------------------------------------------------------------------|
const char *TimerID = "bamse-timeout"; // Replace with the timer ID of this node
const char *DEVICE_LABEL = "hoved-esp32"; // Replace with the device label to subscribe to
const char *MessageLabel = "state-bamse"; // Replace with the device label to subscribe to
// |----------------------------------------------------------------------------------------------|
// |-------------------------------------- TIMER ID (START) --------------------------------------|
// |----------------------------------------------------------------------------------------------|



// Initialize ubidots instance
Ubidots ubidots(UBIDOTS_TOKEN);



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

  i2cConfig();
  // If ESP gets active mode request (0s timeout), activate loop
  while (requestTimeout == 0) {
    // Variables ----------
    float accVal = accRead();
    
    // FERDIG
    // Sending values to ubidots
    if(accVal > 1.4){
      int message = 2;
      ubidots.add(MessageLabel, message); // Insert your variable Labels and the value to be sent
      ubidots.publish(DEVICE_LABEL);

      delay(50);                          // For voodoo reasons
      
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
