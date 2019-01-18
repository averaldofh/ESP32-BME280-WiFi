/* __________________________________________________________________
 *|         Environmental plot using BME280 + Thingspeak             |
 *|   Date: Jan/2019                            Author: Averaldofh   |
 *|   Version: 1.0a                             Device: Wifi_kit_32  |
 *| This sketch is intended to use with ESP32 board with builtin     |
 *| 0.96" OLED screen, by Heltec. I Developed this to monitor        |
 *| ambient temperature, humidity and pressure. As i didn't had at   |
 *| the time another way to log the results because i don't had a    |
 *| SD module, I came up with this simple IoT Project, using a simple|
 *| BME280 Sensor via I2C and Thingspeak service to upload the data  |
 *| and plot to a graph                                              | 
 *|                                                                  |
 *| The Hardware needed is minimal: ESP32, BME280, Tactile Switch    |
 *| and an optional (But very handy) SSD1306 OLED Display, some      |
 *| jumpers and a breadboard. The display will show useful info like:|
 *| WiFi's SSID, the board IP if connected, will tell you if it found|
 *| the I2C Sensor, and then will jump to the main page which looks  |
 *| like this:                                                       |
 *|    ____________________                                          | 
 *|   |   Temp: 30.5 ºC    |                                         |
 *|   |   Humidity: 60.20% |                                         |
 *|   |   Alt: 1800.00 ft  |                                         |
 *|   |--------------------|                                         |
 *|   |______00:00:00______|                                         |
 *|                                                                  |
 *| With the Switch connected to ground and pin 23, you can press it |
 *| to (de)activate the screen, so you don't waste energy if you run |
 *| on batteries.                                                    |
 *|                                                                  |
 *| contact: averaldofh@gmail.com                                    |
 *|__________________________________________________________________|
 */

//....................................Libraries & objects
#include <WiFi.h>                     //Wifi Library    
#include <WiFiUdp.h>                  //this one,
#include <NTPClient.h>                //and this one will be used for time sync
#include <BME280I2C.h>                //Simple and powerful library for the sensor
#include <Wire.h>                     //I2C protocol library
#include <U8g2lib.h>                  //u8g2 Library, great for displays
#include <EnvironmentCalculations.h>  //Part of the sensor library, used to calculate the altitude (you can set another calibrated MSL pressure, but i'm using the standar 1013hPa
BME280I2C::Settings settings(         //Here, we are defining the settings used by sensor's library
   BME280::OSR_X8,                    //make an average reading with 8 Samples for temperature
   BME280::OSR_X8,                    //for humidity
   BME280::OSR_X8,                    //and for pressure
   BME280::Mode_Forced,               //not sure about this
   BME280::StandbyTime_1000ms,        //neither this
   BME280::Filter_16,                 //really need to read the documentation of this library
   BME280::SpiEnable_False,           //maybe telling that we'll use I2C?
   BME280I2C::I2CAddr_0x76            //Default I2C adress with 'SDO' pin grounded, if it is +VCC, address=0x77
);

BME280I2C bme(settings);                                          //Initializes the library with previous defined settings
WiFiUDP ntpUDP;                                                   //Initializes UDP and
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -10800, 60000);      //NTP with my offset (-3h or -10800s)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,16,15,4);        //Declaring Display, also I2C, default address 0X3C

//........................................Constants and Defines
const int MSLP = 1015;                    //Here we declare the pressure at MSL in your area, you can get it on meteorological stations near you
const char* host = "api.thingspeak.com";  //Define a constant host adress, in this case thingspeak.com
String api_key = "YOUR-API-KEY";      //Your API Key provied by thingspeak
#define ssid "SSID"                       //Your SSID                          
#define pass "PASSWORD"                   //Networks password

//........................................Variables
 float Temp, hmdt, prss, ft;              //Will store the Temperature, humidity, pressure and altitude
 int mil, lmil;                           //use with millis function
 bool flag=1;                             //general use flag
 
//........................................Functions Declarations
boolean connectWiFi();                    //Call this function to connect to WiFi
void Send_Data();                         //Call this function to Send data to thingspeak
void GetData();                           //Read the data from the sensor
void ShowOled();                          //Show the main screen on the display
void Scan();                              //Scans the I2C bus to make sure that the sensor is present


//.......................................Program........................................................

void setup(){  //Setup code
  pinMode(23, INPUT_PULLUP);              //Set pin 23 as input with internal pull up resistor, to turn the display on/off
  pinMode(25, OUTPUT);                    //Set pin 25, built-in led, to show us if it succeed to connect to wifi
  digitalWrite(25, LOW);                  //Set pin 25 to low as we don't have connection yet
  Wire.begin();                           //Initializes the I2C protocol
  u8g2.begin();                           //Initializes the display
  bme.begin();                            //Initializes the sensor
  u8g2.clearBuffer();                     //Now, we clear the buffer in order
  u8g2.setFont(u8g2_font_t0_12b_me);      //to send a splash screen to it
  u8g2.drawFrame(0,0,128,64);             //by drawing a frame on the display
  u8g2.drawStr(40,35,"Temp/Hum");         //and writing Temp/Hum at the center
  u8g2.drawStr(35,45,"By Averaldo");      //And also my name, yeh.
  u8g2.sendBuffer();delay(2000);          //Send this data to display and wait for 2 seconds
  connectWiFi();                          //The we can Connect to WiFi
  timeClient.begin();                     //Initialize the time server
  WiFi.disconnect();                      //and close connection to save battery
  digitalWrite(25,LOW);                   //inside the connection function the led will be turned on, showing we are online, then turn it back off here
  Scan();                                 //Scan the I2C bus to show the user if the sensor is present (scan only the 0X76 address)
  }

void loop(){   //loop code
  if(Temp==0)GetData();                   //at the first run, all the data will be zeroed, in order to display something useful instead of 'NaN' we get the data from the sensor right away
  if(millis()-lmil>1800000){              //and then, we wait half an hour (18000000 ms) between each data collect/send routine
  GetData();                              //Call the function that retrieve the data
  Send_Data();                            //and call the function that sends to Thingspeak
  lmil=millis();                          //store actual milis to use at our half hour wait period
  }
  if(!digitalRead(23) && !flag)           //Read if the button has been pressed and the flag is zero, meaning the user want to turn on the display
  {
   flag=1;                                //Set flag to 1, telling the code to lit the display
   while(!digitalRead(23)){ }             //a poor 'debounce' method, the code stuck here whilst the button still pressed, not neat but it works
  }
  if(!digitalRead(23) && flag)            //and if the button is pressed and the flag is 1, the user want to turn off the display
  {
   flag=0;                                //so we set the flag back to zero
   while(!digitalRead(23)){ }
  }
  if(flag)ShowOled();                     //and now we test the flag, if it is 1, show data on display, else, just clear it out
  else { 
    u8g2.clearBuffer();                   //by clearing the buffer
    u8g2.sendBuffer();                    //and sending it to display, blanking the screen
    }
}

  
void ShowOled(){                                                  //This routine will format and display our main page
  String str;                                                     //create a String variable to store the informations
  u8g2.clearBuffer();                                             //then clear the buffer to assure no garbage will be displayed
  u8g2.drawFrame(0,0,128,64);                                     //draw a frame around the display
  u8g2.setFont(u8g2_font_t0_12b_me);                              //set a good size font
  str = "Temp: "; str+= String(Temp,1); str+=" \xb0";str+="C";    //now we just concatenate the string we want to print, this time will be: "Temp: 00.0 ºC" 
  u8g2.setCursor(25,12);                                          //after many tries i found this coordinate in the screen is a good start place
  u8g2.print(str);                                                //so we print our temp
  str = "Umidade: "; str+=String(hmdt,0);str+= " %";              //now we do the same with the humidity: "Umidade: 00 %"
  u8g2.setCursor(25,27);                                          //a line below the temp
  u8g2.print(str);                                                //and print it
  str = "Alt: "; str += String(ft,0); str+= " ft";                //same with altitude: "Alt: 0000 ft"
  u8g2.setCursor(25,42);                                          //right below humidity
  u8g2.print(str);                                                //and print it
  u8g2.drawLine(10,47,118, 47);                                   //draw a nice line below the altitude
  u8g2.setCursor(64-(u8g2.getStrWidth("00:00:00")/2),61);         //centers the cursor to display the actual time
  u8g2.print(timeClient.getFormattedTime());                      //retrieve the actual time
  u8g2.sendBuffer();                                              //and finally displays on the oled screen
}

void GetData(){                                                                                     //This routine will get the data from the sensor and calculate estimated altitude
 EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Feet;   //Set the altitude unit for calculation to feet, you can use AltitudeUnit_Meters too
 EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;    //Set the temperature unit for calculation to Celsius, you can use TempUnit_Farenheit too
 BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);                                               //Set the sensor unit for temperature in Celsius
 BME280::PresUnit presUnit(BME280::PresUnit_hPa);                                                   //Set the sensor unit for pressure hPa
 bme.read(prss, Temp, hmdt, tempUnit, presUnit);                                                    //read the sensor data storing it at our three variables
 ft = EnvironmentCalculations::Altitude(prss, envAltUnit, MSLP, Temp, envTempUnit);                 //calculate the altitude using the pressure read, defined unit, main sea level pressure, actual temperature, and temperatur unit
}

void Send_Data(){                                                        //This routine will send the collected data to thingspeak
  WiFi.begin(ssid, pass); while((WiFi.status() != WL_CONNECTED)){};      //connect to wifi and waits for a successiful connection
  digitalWrite(25,HIGH);                                                 //turn on built-in led to tell user it is online
  WiFiClient client;                                                     //Use WiFiClient class to create TCP connections
  String data_to_send;                                                   //Create a string to store our data
  const int httpPort = 80;                                               //define a port
  if (!client.connect(host, httpPort)) {                                 //Check if the ESP32 is able to connect to thingspeak
    return;                                         
  }
  else                                                                   //if it succeed then go ahead
  { 
    data_to_send = api_key;                                              //append your API key to our string
    data_to_send += "&field1=";                                          //concatenate the string with the field1 option (Maybe used for other fields)
    data_to_send += String(Temp);                                        //concatenate the value to send
    data_to_send += "&field2=";                                          //concatenate the string with the field2 option (Maybe used for other fields)
    data_to_send += String(hmdt);                                        //concatenate the value to send
    data_to_send += "&field3=";                                          //concatenate the string with the field3 option (Maybe used for other fields)
    data_to_send += String(prss);                                        //concatenate the value to send
    data_to_send += "\r\n\r\n";                                          //concatenate terminator string

    client.print("POST /update HTTP/1.1\n");                            //Begin communication with the Host
    client.print("Host: api.thingspeak.com\n");                         //at thingspeak.com
    client.print("Connection: close\n");                                
    client.print("X-THINGSPEAKAPIKEY: " + api_key + "\n");              //inform the API KEY to host
    client.print("Content-Type: application/x-www-form-urlencoded\n");  //inform the type of data
    client.print("Content-Length: ");                                   //inform the length of data
    client.print(data_to_send.length());                                //by means of .length() function      
    client.print("\n\n");                                                 
    client.print(data_to_send);                                         //Send the previously concatenated string to thingspeak

    delay(100);                                                         //Wait a little (not needed)
  }

  client.stop();                                                        //close connection
  timeClient.update();                                                  //update our time        
  WiFi.disconnect();digitalWrite(25,LOW);                               //disconnect the wifi to save battery and turn off the led
}

boolean connectWiFi(){                            //This routine check and connect the board to wifi at first run
  u8g2.clearBuffer();                             //and show at display
  u8g2.setFont(u8g2_font_t0_12b_me);              //using a good looking font
  u8g2.drawFrame(0,0,128,64);                     //with our standard frame 
  u8g2.setCursor(10,30);                          //set a suitable position to write, after many tries
  u8g2.print("Connecting to: ");                  //Tell the user the board is connecting
  u8g2.setCursor(10,40);                          //line below  
  u8g2.print(ssid);                               //to the SSID informed
  u8g2.sendBuffer();                              //then send to display
  WiFi.begin(ssid, pass);                         //and finally connect
                            
   while( (WiFi.status() != WL_CONNECTED))        //but while the board connects
   {
    delay(500);                                   //in half seconds intervals
    u8g2.print(".");                              //we print a new dot in the screen
    u8g2.sendBuffer();                            //to show the code is still running
   }
  
  u8g2.clearBuffer();                             //When connected
  u8g2.setFont(u8g2_font_t0_12b_me);
  u8g2.drawFrame(0,0,128,64);
  u8g2.setCursor(10,30);
  u8g2.print("Connected to Wifi");                //show the user that it has succeed
  u8g2.setCursor(10,40);
  u8g2.print("IP: ");                             //and show the IP
  u8g2.print(WiFi.localIP());
  u8g2.sendBuffer();
  digitalWrite(25, HIGH);                         //and light up the led to confirm to user it has connection
}

void Scan()                                       //This routine will check if the sensor is present at 0X76 address (MOSI/SDO pin of BME280 grounded)
      {
      byte error, address;
        Wire.beginTransmission(118);              //by transmiting to address 0X76 (118, DEC)
        error = Wire.endTransmission();           //and reading the error return at the end of transmission
        if (error == 0)                           //if there's no error, then the sensor is attached correctly
        {
            u8g2.setCursor(10,50);
            u8g2.print("BME280 Found!");          //So we tell the user the sensor has been found
            u8g2.sendBuffer();
         }     
        if (error!=0)                             //Else, 
        {
            u8g2.setCursor(10,50);
            u8g2.print("BME280 NOT Found!");      //Tell the user the sensor can't be found
            u8g2.sendBuffer();
            while(1){}
         }             
      delay(5000);                                // wait 5 seconds for the user to read the informations on screen
    
}
