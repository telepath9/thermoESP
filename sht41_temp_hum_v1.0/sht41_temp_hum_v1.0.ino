#include <Wire.h>
#include <SensirionI2cSht4x.h> 
#include <U8g2lib.h>
#include <NTPClient.h> 
#include <ESP8266WiFi.h>
#include <WiFiUdp.h> 

//https://github.com/Sensirion/arduino-i2c-sht4x/blob/master/src/SensirionI2cSht4x.h
//SSD1306 128X64_NONAME

  // macro definitions
  // make sure that we use the proper definition of NO_ERROR
  #ifdef NO_ERROR
  #undef NO_ERROR
  #endif
  #define NO_ERROR 0

//https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/

SensirionI2cSht4x mysht;

static char errorMessage[64];
static int16_t error;

  // CLK=14 , DATA=13
U8G2_SSD1306_128X64_NONAME_1_SW_I2C ssd1306(U8G2_R0, 14, 13 );

const char* ssid       = "your_ssid";
const char* password   = "your_pw";
const char* ntpServer = "ntp1.inrim.it";
const long  gmtOffset_sec = 7200; // italy is UTC+1, so 1*60*60= 3600sec
//const int   daylightOffset_sec = 3600;  //l'italia usa l'ora legale

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer);
    



void setup() {

  Serial.begin(115200);
  
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);   //non sottovalutare printf! è molto utile
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");

      
  }
  Serial.println("CONNECTED!!!");
  
  //init and get the time from NTP server
  timeClient.begin();
  timeClient.setTimeOffset(gmtOffset_sec); //può essere inclusa nella dichiarazione di timeClient

  unsigned long epochTime = timeClient.getEpochTime();
   struct tm *ptm = gmtime ((time_t *)&epochTime); 


  //disconnect WiFi as it's no longer needed
  //WiFi.disconnect(true);
  //WiFi.mode(WIFI_OFF);

  //END NTP
    
    Serial.begin(115200);
    ssd1306.begin();
    
    Wire.begin();
    mysht.begin(Wire, SHT41_I2C_ADDR_44);

    mysht.softReset();
    delay(10);
    uint32_t serialNumber = 0;
    error = mysht.serialNumber(serialNumber);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute serialNumber(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }

}

//messe fuori dal loop così non vengono resettate a 0 ogni volta che loop ricomincia - PS devi scriverle su EEPROM
// in C++ NULL == 0
float max_temp=0;
float min_temp=0;   
//float min_temp = 0;   

    

void loop() {
  
  //Adesso l'update di timeClient viene eseguito solo quando c'è una nuova Temp MAX o MIN
  //timeClient.update();

  unsigned long epochTime = timeClient.getEpochTime();


  //https://cplusplus.com/reference/ctime/tm/
  //NTP Client doesn’t come with functions to get the date.
  //So, we need to create a time structure (struct tm) and then, access its elements to get information about the date.

  
  //Create time structure 
  //struct tm *ptm = gmtime((time_t *)&epochTime); questa non funziona!!!

  //ma questa SI!!
  time_t rawtime = epochTime;
  struct tm *ptm = gmtime(&rawtime);

    int currentMinute = ptm->tm_min;
    int currentHour= ptm->tm_hour;
    int currentDay= ptm->tm_mday;     //giorno del mese, va da 1-31
    int currentMonth = ptm->tm_mon+1; // +1 perchè i mesi qui vanno da 0-11

  //vars da passare alla funzione che calcola temp&hum correnti
    float aTemperature = 0.0;
    float aHumidity = 0.0;

    //Data to write on EEPROM  | ssd1306 OUTPUT: Fri 27/06 @ 13:25
    char max_temp_data[45];
    char min_temp_data[45];
    //END Data to write on EEPROM

    delay(20);
    
    error = mysht.measureMediumPrecision(aTemperature, aHumidity);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute measureMediumPrecision(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);

         //ssd1306.drawStr(0,15, "Error");
         //ssd1306.drawStr(0,15, errorMessage);
        return;
    }

    /* TEST SU SERIALE
    Serial.print("aTemperature: ");
    Serial.print(aTemperature);
    Serial.print("\t");
    */

    // display
    ssd1306.firstPage();

    char mytemp[25];
    char myhum[25];

     //If current temperature reaches new maximum - WORKING!
    if(max_temp < aTemperature){
        //Fri 27/06 @ 13:25
        timeClient.update();
        max_temp = aTemperature;
        sprintf(max_temp_data, "MAX Temp: %0.2f°C | %d/%d @ %d:%d " , max_temp,  ptm->tm_mday,  ptm->tm_mon+1,  ptm->tm_hour,  ptm->tm_min );
        Serial.println(max_temp_data);

    }

         // min_temp== 0 mi serve per la prima lettura, dato che min_temp è inizializzata con 0.0
    if(max_temp > aTemperature && (min_temp > aTemperature || min_temp == 0) ){
      
      min_temp = aTemperature;
      timeClient.update();
      sprintf(min_temp_data, "Min Temp: %0.2f°C | %d/%d @ %d:%d " , min_temp,  ptm->tm_mday,  ptm->tm_mon+1,  ptm->tm_hour,  ptm->tm_min  );
      
      Serial.println(min_temp_data);
    } 
    
    // https://www.programmingelectronics.com/dtostrf/
    //dtostrf = per convertire float in string.
    sprintf(mytemp, "Temp: %0.2f", aTemperature);

    // il doppio % è staccato da %0.2f, ma per problemi di spazio nel display li ho messi attaccati
    sprintf(myhum, "Hum: %0.2f%%", aHumidity);
    
    //char fulltemp [] = strcat ("Temperature is: " , mytemp);
    //temp va inserito in un char[] e poi devi concatenare mytemp


  do {  //sposta fuori i calcoli della temperatura e metti qui solo le istruzioni per il display
    ssd1306.enableUTF8Print();
    ssd1306.setFont(u8g2_font_helvB14_tf);    //w:18 h:23
    //altro font:u8g2_font_8bitclassic_tf


    ssd1306.drawFrame(0, 0, 128, 64);
    ssd1306.drawStr(5,24,mytemp);
    ssd1306.drawLine(0, 32, 128, 32);
    
    ssd1306.drawStr(5,55, myhum);

    //Nella guida c'è scritto di usarlo per visualizzare le stringhe, ma funziona anche senza
    //ssd1306.sendBuffer();

  } while (ssd1306.nextPage() );

    delay(2000);
}


 /* UPDATE: 
    (DONE) DATA e ORA --> da NTP servers.
    - memorizzare la temp max (e poi min) registrata nell'intera giornata, con tanto di DATA e ORA. ES --> Temp Max: 29.80°C | Fri 27/06                                                                                             Temp Min: 15.10°C | Mon 15/12
    
    - (PERSISTENCE:FAST WAY) per la persistenza anche quando spengo MCU, memorizza le 2 vars nella EEPROM
    - (PERSISTENCE:ADVANCED WAY) memorizzare dati su DB postgresql
 */
