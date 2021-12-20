// Importing relevant libraries
#include "UbidotsEsp32Mqtt.h"
#include <Wire.h>

// Variables --------------------------------------------------
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
char requestTimeoutArray[] = {}; /* Time ESP32 will sleep for (in seconds) */
int requestTimeout = 0;

// WiFi variables
const char *WIFI_SSID = "NAME"; // Wi-Fi name
const char *WIFI_PASS = "PASSWORD"; // Wi-Fi password

// Ubidots conection variables
const char *UBIDOTS_TOKEN = "BBFF-0OuPu7XtnaQxC8Dm4RGzKggbtQuaXS"; // Put here your Ubidots TOKEN


const int MPU_ADDR = 0x68; // I2C address of the MPU-6050
int16_t GyX, GyY, GyZ;

// Declaring pin variables
const int blue = 27;
const int green = 12;
const int yellow = 25;
const int red = 14;
const int white = 18;
const int buzzer = 2;

// Variables used for analogue writing using PWM
const int frequency = 2000;
const int channel = 0;
const int resolution = 8;

/* The dice can be tilted in four different directions, each represented by a unique colour.
 * The dice must be tilted in the correct 4 symbol combination in order to complete the task.
 * An enum is therefore decalared to record which symbols (represented by colours) are chosen 
 * in which order. For example:
 * game_stage = stage_1 // The first symbol is to be selected by the player
 * colour_1 = GREEN // The first symbol selected by the user is 'green'
*/

enum Colour{NONE, GREEN, YELLOW, RED, BLUE};
enum Stage{stage_1, stage_2, stage_3, stage_4, complete};

// Resetting all the inital values
Stage gameStage = stage_1;
Colour colour = NONE;

Colour colour_1;
Colour colour_2;
Colour colour_3;
Colour colour_4;


// Function that creates a beeping noise from the buzzer
void beep(){
  ledcWriteTone(channel, 2000);
  delay(50);
  ledcWriteTone(channel, 0);
  delay(50);
}

// Function that creates a beeping sequence to be played after each failed attempt
void incorrect_sequence(){
  beep();
  beep();
  beep();

  // The task is then reset so that the user can try again
  colour = NONE;
  gameStage = stage_1;
  delay(600);
}

// Function that plays a victory noise and raises a flag to say that the task is complete
void victory_sequence(){
  Serial.println("NODE COMPLETED");
  digitalWrite(white, HIGH);
  for (int i=20; i>0; i--){
    beep();
  }
  gameStage = complete;
}


// |----------------------------------------------------------------------------------------------|
// |-------------------------------------- TIMER ID (START) --------------------------------------|
// |----------------------------------------------------------------------------------------------|
const char *TimerID = "terning-timeout"; // Replace with the timer ID of this node
const char *DEVICE_LABEL = "hoved-esp32"; // Replace with the device label to subscribe to
const char *MessageLabel = "state-terning";
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
* The loop at the end of the code will never run
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
  delay(500);

  // Connect to ubidots until succession
  while (!ubidots.connected()) {
    ubidots.reconnect();
    Serial.println("DEBUG: reconnecting to Ubidots");
  }

  // Get timeout value
  ubidots.subscribeLastValue(DEVICE_LABEL, TimerID);

  // Refresh ubidots values twice to avoid bugs
  ubidots.loop();
  ubidots.loop();



  // |---------------------------------------------------------------------------------------------|
  // |----------------------------------- INFINITE LOOP (START) -----------------------------------|
  // |---------------------------------------------------------------------------------------------|
  Serial.println("#--------------------------------------------------#");

  // Setting up a PWM channel for the buzzer
  ledcSetup(channel, frequency, resolution);
  ledcAttachPin(buzzer, channel);

  Serial.begin(115200);

  // Wire is used for reading specific values from specific registers
  Wire.begin(21, 22, 100000); // sda, scl, clock speed
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU−6050)
  Wire.endTransmission(true);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6C);  // PWR_MGMT_2 register
  Wire.write(0x38);  // Accelerometer turned off
  Wire.endTransmission(true);
  Serial.println("Setup complete");

  // Setting the output mode for the LEDs
  pinMode(blue, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(yellow, OUTPUT);
  pinMode(red, OUTPUT);
  pinMode(white, OUTPUT);

  
  // If ESP gets active mode request (0s timeout), activate loop
  while (requestTimeout == 0) {
    
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x43);  // starting with register 0x43 (GYRO_XOUT_H)
    Wire.endTransmission(true);
    Wire.beginTransmission(MPU_ADDR);
    Wire.requestFrom(MPU_ADDR, 4, true); // request a total of 4 registers
    GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  

    // These if statements determine the direction in which the dice is being turned
    if (GyX > 17000){
      digitalWrite(blue, HIGH);
      colour = BLUE;
    }
    else if (GyX < -17000){
      digitalWrite(green, HIGH);
      colour = GREEN;
    }
    else if (GyY > 17000){
      digitalWrite(yellow, HIGH);
      colour = YELLOW;
    }
    else if (GyY < -17000){
      digitalWrite(red, HIGH);
      colour = RED;
    }
    else{
      colour = NONE;
    }


    // The corresponding LED flashed so that the user know that there input has been registered
    delay(200);
  
    digitalWrite(blue, LOW);
    digitalWrite(green, LOW);
    digitalWrite(yellow, LOW);
    digitalWrite(red, LOW);
  
  
    // Each game stage represents the index of which sy
    switch(gameStage){
      case stage_1:{ // This is the first user input
        Serial.println("Stage 1");
        if (colour != NONE){ // Waits until a colour is inputted, then saves the inputted value
          colour_1 = colour;
          beep();
          gameStage = stage_2;
          delay(700);
        }
        break;
      }
  
      case stage_2:{ // This is the second user input
        Serial.println("Stage 2");
        if (colour != NONE){
          colour_2 = colour;
          beep();
          gameStage = stage_3;
          delay(700);
        }
        break;
      }
  
      case stage_3:{ // This is the third user input
        Serial.println("Stage 3");
        if (colour != NONE){
          colour_3 = colour;
          beep();
          gameStage = stage_4;
          delay(700);
        }
        break;
      }
  
      case stage_4:{ // This is the fourth user input
        Serial.println("Stage 4");
        if (colour != NONE){
          colour_4 = colour;
          
  
          // Each of the four stages must recieve the correct input to complete the task
          if ((colour_1 == YELLOW) and (colour_2 == BLUE) and (colour_3 == RED) and (colour_4 == GREEN)){
            victory_sequence();
          }
  
          else{ // Runs when an incorrect sequence is inputted
            incorrect_sequence();
          }
        }
        break;
      }
  
      case complete:{ // The value '2' is sent to Ubidots to represent that the task is complete
        Serial.println("Node Complete");
        ubidots.add(MessageLabel, 2);
        ubidots.publish(DEVICE_LABEL);

        requestTimeout = 60;
        
        break;
      }
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
  esp_sleep_enable_timer_wakeup(requestTimeout * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}



// Loop that wil never run --------------------------------------------------
void loop(){
  
}
