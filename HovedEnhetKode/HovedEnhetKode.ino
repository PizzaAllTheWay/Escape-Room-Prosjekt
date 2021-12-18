// Bibliotek inkludering 
#include <ESP32Servo.h>           // Servo
#include <analogWrite.h>          // analogwrite
#include <PubSubClient.h>         // Ubidots oppkobling
#include <UbiConstants.h>         // Ubidots oppkobling
#include <UbidotsEsp32Mqtt.h>     // Ubidots MQTT bibliotek
#include <UbiTypes.h>             // Ubidots oppkobling
#include <SPI.h>                  // SPI biblotek
#include <Adafruit_GFX.h>         // Skjerm grafikk biblotek
#include <Adafruit_ILI9341.h>     // ILI8341 skjerm importering
#include <ESP32Tone.h>            // Høytaler tone biblotek

/*
 * Verdier innenfor denne rammen er verdier som skal byttes når det legges til spill.
 * 
 */

int GameRI[3];            // Array av randomisert enhet indeks, bytt tall for antall spill(Game Device Randomize Index array)
int gameState[3];         // Array til tilstand av enhet, bytt tall for antall spill(Game state array)
int requestTimeout[3];    // Array av tidsverdier for enhet, bytt tall for antall spill(Request timeout array)

const int Games_amount = 3;                    //  Antall spill i systemet
int minutt = 20;                               //  Ønsket minutter for spillet
int sekund = 30;                               //  Ønsket sekunder for spillet
const int time_out_normal = 10;                //  Normal søvnperiode for nodene å sjekke hovedenheten

//-----Innsetting av node variabler-----

const char *Games_variables[] = {              // Spill variabler for spill tilstandene [0,1,2]([ikke-startet,start,godkjent])
  "state-bamse",
  "state-terning",
  "state-sjakk"
};
const char *Games_timeout_variables[] = {      // Tidsverdi variable for soving tid
  "bamse-timeout",
  "terning-timeout",
  "sjakk-timeout"
};
const char *Games_name[] = {                   // Spill navn, som vises på enhet skjerm
  "Bamse",
  "Terning",
  "Sjakk"
};

//*********************************************************************************************************
//*********************************************************************************************************


// Definering av pinner for Skjerm, og div
// _______Skjerm_______
#define TFT_CS   19     // Chip select
#define TFT_RST  18     // Reset på skjerm
#define TFT_DC    5     // DataCommand
#define TFT_MOSI 17     // Data (Bildet som sendes), SDA på skjerm
#define TFT_CLK 16      // Klokke, SCK på skjerm
#define TFT_MISO  32    // MISO PORT 

// _______Kabel kutting_______
#define Wrong_cable 22
#define Right_cable 21

// _______Lys, servo og høytaler_______
#define LED 13
#define Servo_pin 33
#define Piezo 23     // Pinnummer på høytaler

//Definerer frekvensen til dei forskjellige notene som blir brukt i melodien
const int Cfire = 262;
const int Gfire = 392;
const int Fseks = 1397;
const int Aseks = 1760;
const int Csju = 2093;
const int Esju = 2637;
const int Fsju = 2794;
const int Asju = 3520;

// Definerer variabler for globalt brukt
int sendDelay = 50;                         // Forsinkelse etter sending av data til ubidots
int Payload_sub = 0;                        // Brukes for avlest verdi som variabel 
String Topic_sub = "";                      // Brukes for å henvise til label for UBIDOTS
int Game_state = 0;                         // Spill tilstand
int oppgave_godkjent = 0;                   // Tellevariabel for godkjent spill
int prev_minutt = 61;                       // Feilsafe for systemet i oppstart
int suksess = 0;                            // Variabel for å vise om spillet er suksess
char Subscribe_value[] = "";                // Avlesningsliste
const int progression = 320/Games_amount;   // Konstant for å tegne progresjonsbar 

// Tids variabler
unsigned long one_second = 1000;     // Definering av et sekund
unsigned long timer;        // Definering av timer variabel
  
// Randomizer variablen
int GameDeviceLenght = sizeof(GameRI)/sizeof(GameRI[0]); 

// Definering av UBIDOTS tilkoblingsverdier
const char *UBIDOTS_TOKEN = "BBFF-0OuPu7XtnaQxC8Dm4RGzKggbtQuaXS";  // UBIDOTS token
const char *WIFI_SSID = "ChoudhryAirlines";                         // Wi-Fi SSID
const char *WIFI_PASS = "TalhaHarStorKUK";                          // Wi-Fi password
const char *DEVICE_LABEL = "hoved-esp32";                           // Hoved Device label
const char *VARIABLE_LABEL = "spill-av-eller-pa";                   // Label for spill av og på 

// Initialisering av Ubidots
Ubidots ubidots(UBIDOTS_TOKEN);

// Aktivering av TFT panel ved å bruke Adafruit
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// oppstart av servo
Servo myservo;

// Definerer en suksess lyd
void Suksess(int Pin)
{
  tone(Pin, Fseks);
  delay(163);
  tone(Pin, Aseks);
  delay(162);
  tone(Pin, Fsju);
  delay(163);
  tone(Pin, Csju);
  delay(162);
  tone(Pin, Esju);
  delay(163);
  tone(Pin, Asju);
  delay(262);
  noTone(Pin);
  return;
}

// Definerer error lyd
void Feil(int Pin)
{
  tone(Pin, Gfire);
  delay(163);
  tone(Pin, Cfire);
  delay(325);
  noTone(Pin);
  return;
}

// Funksjon som beregner neste tidsverdi basert på nåværende verdi
void time_remaining(){
  if (sekund == 0){           
    sekund = 59;             // "Nulling" av sekund verdi
    if(minutt == 0){         // Isolere tilfelle med 0 min og 0 sekund
      Game_state = 0;        // nulling av konstanter og game_state
      sekund = 0;
      suksess = 0;
      exit;                 // Ut av tidsfunksjon, tiden er ute
    }
    else{
      minutt--;             // trekking av minutt
    }
  }
  else{
    sekund--;               // trekking av sekunder
  }
}

// Funksjon for å male inn startskjerm
void start_screen(){
  tft.fillScreen(ILI9341_BLACK);                                                    // startskjerm er svart
  tft.fillRect(10, 10, 300, 220, ILI9341_WHITE);                                    // Hvit boks, mindre
  tft.fillRect(20, 20, 280, 200, ILI9341_BLACK);                                    // Svart boks mindre enn hvite
  tft.setTextColor(ILI9341_CYAN);  tft.setTextSize(3);  tft.setCursor(45,80);       // Definerer parameterer for tekst i boksen                                       
  tft.println("GAME STARTING");                                                     // Skriver teksten i midten av skjermen
  for (int C = 0; C < 230; C++){                                                    // Oppstartsbar, bare for estetikk, kan fjernes eller endre delay
    tft.fillRect(45+C, 120, 1, 42,ILI9341_CYAN); 
    delay(20);
    }
  }

// Funksjon som maler in godkjente spill
void Task_status(int number){
  tft.fillRect(2, 2, 114,  70, ILI9341_RED);               // Nulling av høyre topp boksen
  tft.setCursor(25, 30);                                   // Plassering på skjermen 
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(2);    // Definering av tekst størrelse og farge
  tft.println(number);                                     // -------------------- 
  tft.setCursor(50, 30);      
  tft.println("/");
  tft.setCursor(70, 30);
  tft.println(Games_amount);                               // Antall spill/noder i systemet
  }

// Funksjon som maler inn tiden på skjermen  
void paste_time(int color){
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(7);       // Definering av størrelse og farge på tekst

  int Color[2] = {ILI9341_RED, ILI9341_BLACK};                // Definering av farge liste for
  
  // Maler gitte/nåværende minutter
  tft.setCursor(70, 110);

  // If setning som isolerer tilfeller der minutt er mindre enn 10, slik av en 0 kan plasseres foran
  if(minutt < 10 && minutt != prev_minutt){                 
    tft.fillRect(70, 100, 80, 60,Color[color]);         // nuller ut plasseringen til minuttene              
    tft.println("0");                                        
    tft.setCursor(115, 110);    
    tft.println(minutt);                                // Gitt verdi plasseres
  }
  // Isolerer tilfeller der minuttene ikke er endret, unngår å skrive om hele skjermen
  else if (minutt != prev_minutt){                             
    tft.fillRect(70, 100, 80, 60,Color[color]);         // nuller ut plasseringen til minuttene                
    tft.setCursor(70, 110);   
    tft.println(minutt);                                // Gitt verdi plasseres
  }

  // maler inn skille mellom minutt og sekund
  tft.setCursor(142, 110);
  tft.println(":");

  // Maler på gitte/Nåværende sekunder
  // If setning for å isolere tid under 10 sekunder, slik at 0 kan plasseres foran
  if(sekund < 10){                                     
    tft.fillRect(170, 100, 80, 60,Color[color]);         // nuller ut plasseringen til sekundene
    tft.setCursor(170, 110);
    tft.println("0");
    tft.setCursor(210, 110);
    tft.println(sekund);                                // Gitt verdi plasseres
  }
  // Tilfelle der sekunder er større enn 10
  else{                                
    tft.fillRect(170, 100, 80, 60,Color[color]);         // nuller ut plasseringen til sekundene
    tft.setCursor(170, 110);
    tft.println(sekund);                                // Gitt verdi plasseres
  }
}

// Funksjon som maler inn spill navnet
void Paste_game_name(int game_name_index){
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(2);   // Farge og størrelse på tekst
  tft.fillRect(118, 2, 200, 70, ILI9341_RED);             // Nulling av høyre topp firkant
  tft.setCursor(140, 30);
  tft.println("Game:");
  tft.setCursor(200, 30);
  tft.println(Games_name[game_name_index]);               // innhenting av navm gjennom randomisert liste
}

// Maling av siste spill på hovedenheten med kabelkutting
void End_text(int color){

  int Color[2] = {ILI9341_WHITE, ILI9341_RED};    // Fargeliste
  
  tft.setTextColor(Color[color]);                 // Maling av farge på skjermen
  tft.setTextSize(5); 
  tft.setCursor(30,50);
  tft.println("LAST TASK");
  tft.setTextSize(3);
  tft.setCursor(30,180);
  tft.println("CUT RIGHT CABLE");
}

// Maling av skjermen for taper/vinner i slutten basert på resultatet av suksess variabelen
void Paste_winner_loser_screen(const char *Bomb_result, int color){

  int Color[3] = {ILI9341_GREEN, ILI9341_RED, ILI9341_WHITE};      // Fargeliste
  
  tft.setTextColor(Color[color]); tft.setTextSize(6);             // Definering av farge og tekststørrelse
  tft.setCursor(80, 60);
  tft.println("BOMB");
  tft.setCursor(20, 120);
  tft.println(Bomb_result);
  delay(300);
}

// Function for sending Data
void Send_data(int value, const char* variable){
  ubidots.add(variable, value);                     // Legger til verdi og variable som er valgt
  ubidots.publish(DEVICE_LABEL);                    // Sending til Device med valgt variabel
}
/***********************************
 * UBIDOTS sin egen funksjon som er modifisert for å hente ut payload som et tall
 */
void callback(char *topic, byte *payload, unsigned int length)
{
  // Get message
  Subscribe_value[length] = {};                     // Definering av streng som skal ta imot payload

  for (int i = 0; i < length; i++)                  // Iterer over payload for å hende ut verdier
  {
    Subscribe_value[i] = (char)payload[i];          // Legger til i den definerte listen
  }

  // Filtrering av topic, bare navnet av variablene
  int filter[2] = {0, 0};
  char TempTopic[100];
  
  strcpy(TempTopic, topic);                       // Lager variabel som topict fra "pointer char" til normal "char" data type
  
  Topic_sub = String(TempTopic);                  // Lage til streng for lett filtrering 
  Topic_sub.remove(0, 26);
  Topic_sub.remove(Topic_sub.length() - 3, 3);
  
  // Lage ny variabel verdi
  Payload_sub = atoi(Subscribe_value);              // Konverterer fra streng til int med atoi
}

// ___________________________________________________________________________
// Sending av tidsverdi
void send_request_timeout() {
  // Bare den nyeset verdien blir tatt
  int tempNew = 0;

  // Hent nyeste aktive indeks
  for (int i = 0; i < GameDeviceLenght; i++) {
    if (gameState[GameRI[i]] == 1) {
      tempNew = i;
    }
    else if (i < (GameDeviceLenght - 1)) {
      if ((gameState[GameRI[i]] == 2) && (gameState[GameRI[i + 1]] == 0)) {
        gameState[GameRI[i + 1]] = 1;
  
        // Send verdi
        Send_data(gameState[GameRI[i + 1]], Games_variables[GameRI[i + 1]]);
        Send_data(time_out_normal, Games_timeout_variables[GameRI[i]]);
        Send_data(0, Games_timeout_variables[GameRI[i + 1]]);
        delay(sendDelay);
      }  
    }
    else if((i == (GameDeviceLenght - 1)) && (gameState[GameRI[i]] == 2)) {
      Send_data(time_out_normal, Games_timeout_variables[GameRI[i]]);
      delay(sendDelay);
    }
  }
  
  // Bare innhenting av nyeste indeks
  ubidots.subscribeLastValue(DEVICE_LABEL, Games_variables[GameRI[tempNew]]);
  ubidots.loop();
  ubidots.loop();         // Double loop for å få riktige verdier

  // Bare set "game state" hvis korekt "topic" varaibel har komet
  char TempTopic[100];
  strcpy(TempTopic, Games_variables[GameRI[tempNew]]); // Lage "topict" variabel fra "pointer char" til normal "char" data type

  if (Topic_sub == String(TempTopic)) {
    gameState[GameRI[tempNew]] = Payload_sub;
  }
}
// _____________________________________________________________________________________________
//______________________________________Oppsett av program______________________________________
//______________________________________________________________________________________________
void setup() {
  Serial.begin(9600);                           // Definering av Serial
  pinMode(Wrong_cable, INPUT);                  // Definerer pinmode for feil kabel
  pinMode(Right_cable, INPUT);                  // Definerer pinmode for riktig kabel
  pinMode(LED, OUTPUT);                         // Definerer pinmode for LED

  tft.begin();                                  // Starter skjerm
  tft.setRotation(3);                           // Setter riktig rotasjon på skjerm
  ubidots.connectToWifi(WIFI_SSID, WIFI_PASS);  // kobler UBIDOTS til WIFI
  ubidots.setCallback(callback);                // Sette Calback funksjonen
  ubidots.setup();                              // Initialiserer UBIDOTS
  ubidots.reconnect();                          // Funksjon for rekobling, sikkerhetsskyld
  
  myservo.attach(Servo_pin);                    // sette servo til sin egen pin
  myservo.write(100);                           // Skriver inn Servo verdi, må testes ut basert på servo eller plassering av luka
  
  start_screen();                               // Kaller start skjerm, maler den inn
  Game_state = 1;                               // Gir Gamestate verdien 1
  
  // Set all "requestTimeout" varaiblene som 10 sekunder på starten for sikerhetskyld, slik at alt er nullstilt
  requestTimeout[0] = 0;
  for (int i = 1; i < GameDeviceLenght; i++) {
    requestTimeout[i] = time_out_normal;
  }

  // Set ale "game states" som 0 (Ikke aktiv) untat første "game states", set den som 1 (Aktiv)
  gameState[0] = 1;
  for (int i = 1; i < GameDeviceLenght; i++) {
    gameState[i] = 0;
  }
  
  // Randomizer -----
  // Setter "game device" random index array som sortert etter randomisering
  for (int i = 0; i < GameDeviceLenght; i++) {
    GameRI[i] = i;
  }

  // Randomize
  for (int i = 0; i < GameDeviceLenght - 1; i++) {
    int ranIndex = random(0, GameDeviceLenght - i);
    
    int indexNow = GameRI[i];

    GameRI[i] = GameRI[ranIndex];
    GameRI[ranIndex] = indexNow;
  }

  // Sender ny "game state" data til ubidots
  for (int i = 0; i < GameDeviceLenght; i++) {
    Send_data(gameState[i], Games_variables[GameRI[i]]);
    delay(sendDelay);
  }

  // Sender ny "request timeout" data til ubidots
  for (int i = 0; i < GameDeviceLenght; i++) {
    Send_data(requestTimeout[i], Games_timeout_variables[GameRI[i]]);
    delay(sendDelay);
  }
}

//________________________________________________________________________________________________________________
//______________________________________Hoved Loop for OS Programet START_________________________________________
//________________________________________________________________________________________________________________

void loop() {

  // Spillet starter etter at hovedskjermen er ferdig

  // Starter spillet
  if (Game_state == 1){
    
    // Sender verdi til UBDIOTS hovedsignal om at spillet er på
    Send_data(Game_state, VARIABLE_LABEL);
    send_request_timeout();

    // Maler inn hovedskjermen for spillet/ oversiktssiden
    tft.fillScreen(ILI9341_WHITE);
    tft.fillRect(2, 2, 114,  70, ILI9341_RED);          // boks venstre-top
    tft.fillRect(118, 2, 200, 70, ILI9341_RED);         // boks høyre-top
    tft.fillRect(2, 74, 316, 116,ILI9341_RED);          // boks midten
    tft.fillRect(0, 192, 320, 50,ILI9341_BLACK);        // Bunn progresjonsbar bakgrunn
    tft.fillRect(2, 196, 316, 42, ILI9341_WHITE);       // progresjonsbar

    // Skriver inn informasjon i blokkene på skjermen
    Task_status(oppgave_godkjent);                            // Oppgave status males inn
    paste_time(0);                                            // Tid males inn 
    Paste_game_name(GameRI[oppgave_godkjent]);                // oppgavenavn males inn
    timer = millis();                                         // Starter millis for tidsoversikt
    
    //______________________________________________________________________________
    //______________________________Spill LOOP STARTER______________________________
    //______________________________________________________________________________
    
    while(Game_state == 1){
      
      // Maler inn ny tid hvert sekund
      if ((millis()-timer) > one_second){
        one_second += 1000;                   // Adder på 1000ms for at den er cirka et sekund hver gang

        // Tidskalkulasjon og påmaling
        prev_minutt = minutt;
        time_remaining();                     // Innhenting av tid nå
        paste_time(0);                        // Påmaling av tid 

        send_request_timeout();               // Sending av tid til spill 

        Serial.print(oppgave_godkjent);Serial.print(": ");
        for (int i = 0; i < GameDeviceLenght; i++) {
          Serial.print("|");
          Serial.print(gameState[i]);
        }
        Serial.println("|");
        Serial.println(gameState[oppgave_godkjent]);
      }

      // IF for godkjenning av spill
      if (gameState[GameRI[oppgave_godkjent]] == 2){
   
        oppgave_godkjent ++;                              // fåre høyere verdi grunnet godkjent spill
        Task_status(oppgave_godkjent);                    // Oppdatering av statusbaren grunnet godkjent spill
        if(oppgave_godkjent < Games_amount){              // If for å isolere at det er flere spill videre
          Paste_game_name(GameRI[oppgave_godkjent]);      // maling av nåværende spill/nestespill
        }
        Suksess(Piezo);                                   // Suksesslyd fra høytaler
        
        // Maling av progresjonsbar grunnet godkjent spill
        for (int Bar=0; Bar<progression+1; Bar++){
          tft.fillRect((progression*(oppgave_godkjent-1))+Bar, 196, 1, 42,ILI9341_CYAN);  
        }
      }
     
      // If statenment for å stoppe loopen
      if (oppgave_godkjent == Games_amount){  // alle spill er godkjente når oppgave_godkjent = Games_amount
        suksess = 1;                        // for å vise suksess i nestesteg
        Game_state = 0;                     // nulles grunnet ferdig med hovedspillene
        break;                              // Break for å komme ut av loop
      }
    }
    //____________________________________________________________________________
    //______________________________GAME LOOP ENDING______________________________
    //____________________________________________________________________________
    
    // If Statement for suksess
    if (suksess == 1){ 
      send_request_timeout();                                           // sende tidsverdi til slutt til alle noder

      // Maler inn siste spill skjerm
      tft.fillScreen(ILI9341_BLUE);
      tft.fillRect(10, 10, 300, 220, ILI9341_YELLOW);
      tft.fillRect(20, 20, 280, 200, ILI9341_BLACK);
      digitalWrite(LED, HIGH);

      // Åpner Servoen for å vise frem kabler
      for (int servo_degrees = 100; servo_degrees > 0; servo_degrees--){
        myservo.write(servo_degrees);
        delay(10);                        // Kan justeres om man ønsker mer estetisk(opp) eller raskere(ned)
      }

      // Tidsverdier henger med, og man har en gående tid på siste oppgave også
      // timer og one_second starter bare på nytt, resterende tid er identisk
      timer = millis();
      one_second = 1000;
      prev_minutt = 61;
      time_remaining();                    // Henter nåværende tid
      paste_time(1);                       // Drawing on current time

      // whileloop for kabelkutting
      while(true){

        // maler inn siste teksten i to farger
        End_text(0);
        End_text(1);
        
        if ((digitalRead(Wrong_cable) == 0) or suksess == 0){         // feil kabel kuttet, åpenkrets
          // nulling av variabler
          Game_state = 0;
          suksess = 0;
          break;
        }
        if (digitalRead(Right_cable) == 0){                           // riktig kabel kuttet, åpenkrets
          // Nulling av spilltilstanden, UBIDOTS viser at spillet er over
          Send_data(Game_state, VARIABLE_LABEL);
          tft.fillScreen(ILI9341_GREEN);

          // For loop for å lukke luka igjen
          for (int servo_degrees = 0; servo_degrees < 101; servo_degrees++){
            myservo.write(servo_degrees);
            delay(10);                        // Kan justeres om man ønsker mer estetisk(opp) eller raskere(ned)
          }
            
          // For looping for å vise vinnerskjermen 
          for (int value = 0; value<4 ; value ++){
            Suksess(Piezo);                               // winnerlyd spilles av 
            Paste_winner_loser_screen("DISARMED", 0);     // Viser at bomben er desarmert i to farger
            Paste_winner_loser_screen("DISARMED", 2);
          }
          break;
        }
        if ((millis()-timer) > one_second){
          one_second += 1000;                   // Adder på 1000ms for at den er cirka et sekund hver gang 
  
          // Tidskalkulasjon og påmaling
          prev_minutt = minutt;
          time_remaining();                     // Innhenting av tid nå
          paste_time(0);                        // Påmaling av tid 
        }
      }

    }
    
    // If statement der man taper spille endten ved feil kutting av kabel eller tiden som går ut.
    if (suksess == 0){

      // Nulling av spilltilstanden, UBIDOTS viser at spillet er over
      Send_data(Game_state, VARIABLE_LABEL);
      tft.fillScreen(ILI9341_RED);
      
      // For loop for å lukke luka igjen
      for (int servo_degrees = 0; servo_degrees < 101; servo_degrees++){
        myservo.write(servo_degrees);
        delay(10);                        // Kan justeres om man ønsker mer estetisk(opp) eller raskere(ned)
      }
      
      // For looping som viser av bomben ikke ble disarmert
      for (int value = 0; value<4 ; value ++){
        Feil(Piezo);                                  // taperlyd spilles av 
        Paste_winner_loser_screen("EXPLODED", 1);     // Viser at bomben er desarmert i to farger
        Paste_winner_loser_screen("EXPLODED", 2);        
    }
  }
  ubidots.loop();     // brukes for å refreshe verdier fra UBIDOTS, kan brukes flere ganger for å tvinge UBIDOTS til å sende/oppdatere verdiene sine
    
  }
}
//________________________________________________________________________________________________________________
//______________________________________Hoved Loop for OS Programet END ___________________________________________
//________________________________________________________________________________________________________________
