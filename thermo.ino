#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <LCD.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// begins at 2:00 a.m. on the second Sunday of March (at 2 a.m. the local time time skips ahead to 3 a.m. so there is one less hour in the day)
//ends at 2:00 a.m. on the first Sunday of November (at 2 a.m. the local time becomes 1 a.m. and that hour is repeated, so there is an extra hour in the day)

const long utcOffsetInSeconds = -14400;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
#define DEBUG 1

//pins for relays
#define relay_ac 14     // relay 1 pins for relay that controls heat & ac (D5)
#define relay_heat 12   // relay 2 D6
#define relay_fan 13    // relay 3 D7 fan
#define buttonUpPin 3
//const int buttonDownPin
#define buttonSelectPin A0

WiFiClient espClient;
PubSubClient client(espClient);

typedef struct {
  int six_am;
  int eight_am;
  int six_pm;
  int ten_pm;
} schedule;

schedule day[7] = {
  {24, 25, 25, 24},
  {24, 25, 25, 24},
  {24, 25, 25, 24},
  {24, 25, 25, 24},
  {24, 25, 25, 24},
  {24, 25, 25, 24},
  {24, 25, 25, 24}
};
schedule temp_buffer[7] = {0};

String scheMsg[4] = {"6am", "8am", "6pm", "10pm" };
String weekDays[7]={"Sun", "Mon", "Tues", "Wednes", "Thurs",  "Fri", "Sat"};

float last_temp;
float last_humid;
float temp;
float humid;

int target_temp = 24;

int eeAddress = 0;

int prevButtonUp_state = 0;
int prevButtonDown_state = 0;
int prevButtonSelect_state = 0; 
int buttonUp_state = 0;
int buttonDown_state = 0;
int buttonSelect_state = 0;
int menucounter = 0;
int menulevel = 0;
int schecounter = 10;
int tempcounter = 10;
int modecounter = 0;
int scheflag = 0;
int buffflag = 0;

int hvac_state = 0;


//wifi login
const char* ssid = "Android";
const char* password = "thatface128";
//connection to raspberrypi broker
const char* mqtt_server = "192.168.43.101";
const int mqtt_port = 1883;

//mqtt message buffer
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

//main menu:
// set temp     ->
// set mode
//

void lcd_menu() {  
  buttonSelect_state = digitalRead(buttonSelectPin); // checks if the menu button is pressed
  buttonUp_state = digitalRead(buttonUpPin);  // checks if the up button is pressed
  //buttonDown_state = digitalRead(buttonDownPin); // checks if the down button is pressed

  if (buttonSelect_state != prevButtonSelect_state) {   
    if (buttonSelect_state == LOW) {  
      
      if (menulevel == 1 && menucounter == 0) { //if menu is pressed during set schedule 
            buffflag = 0;          
            scheflag = 0;
            switch (scheflag) {
              case 0:
                temp_buffer[buffflag].six_am = schecounter;
                break;
              case 1:
                temp_buffer[buffflag].eight_am = schecounter;
                break;
              case 2:
                temp_buffer[buffflag].six_pm = schecounter;
                break;
              case 3:
                temp_buffer[buffflag].ten_pm = schecounter;
                break;
            }
            scheflag++;
            if (scheflag == 4) {
              scheflag = 0;
              buffflag++;
            }
            if (buffflag == 7) {
              buffflag = 0;
              menulevel++;  // menulevel = 2
              memcpy(&day, &temp_buffer, sizeof(temp_buffer));
            }
        }
        
        if (menulevel == 1 && menucounter == 1) {    //change targetTemp to selected temp
          //todo: revert back to scheduled temp after one hour
          target_temp = tempcounter;
          menulevel++;            
          }

        if (menulevel == 1 && menucounter == 2) { //change mode of thermo          
          switch (modecounter) {
            case 0: //turn on ac
              digitalWrite(relay_ac, LOW);
              digitalWrite(relay_fan, LOW);
              digitalWrite(relay_heat, HIGH);
              client.publish("out/openhab/relay_ac", "ON");
              hvac_state = 1;
              menulevel++; 
              break;
            case 1:  //turn on heat
              digitalWrite(relay_heat, LOW);
              digitalWrite(relay_fan, LOW);
              digitalWrite(relay_ac, HIGH);
              client.publish("out/openhab/relay_heat", "ON");
              hvac_state = 2;
              menulevel++;
              break;
            case 2:  //turn off
              digitalWrite(relay_heat, HIGH);
              digitalWrite(relay_fan, HIGH);
              digitalWrite(relay_ac, HIGH);
              client.publish("out/openhab/relay_ac", "OFF");
              client.publish("out/openhab/relay_heat", "OFF");
              hvac_state = 0;
              menulevel++;
              break;
          }        
        }
      if (menulevel == 0) {
        menulevel++;
      }
      if (menulevel == 2) {
        menulevel = 0;
      }
        
      }
//      Serial.print("MENUmenulevel: ");Serial.println(menulevel);      
    }     
  
  prevButtonSelect_state = buttonSelect_state;      

  
  if (buttonUp_state != prevButtonUp_state) {
    if (buttonUp_state == LOW) {
      
      if (menulevel == 0) {
        menucounter++;
        if (menucounter == 3) { 
          menucounter = 0;
        }
      }        
      if (menulevel == 1 & menucounter == 0) {
          schecounter++;
          if (schecounter == 31) {
            schecounter = 10; 
          }
      } 
      if (menulevel == 1 & menucounter == 1) {
          tempcounter++;
          if (tempcounter == 31) {
            tempcounter = 10; 
          }
      } 
      if (menulevel == 1 & menucounter == 2) {
          modecounter++;
          if (modecounter == 3) {
            modecounter = 0; 
          }
      }     
      
    }
//    Serial.print("UPmenulevel: ");Serial.println(menulevel); 
//    Serial.print("UPmenucounter: ");Serial.println(menucounter); 
  }
  prevButtonUp_state = buttonUp_state;
    
//  if (buttonDown_state != prevButtonDown_state) {
//    if (buttonDown_state == LOW) {
//      
//      if (menulevel == 0) {
//        menucounter--;
//        if (menucounter == -1) { 
//          menucounter = 2;
//        }
//      }      
//      if (menulevel == 1 & menucounter == 0) {
//          schecounter--;
//          if (schecounter == 9) {
//            schecounter = 30; 
//          }
//      }      
//      if (menulevel == 1 & menucounter == 1) {
//          tempcounter--;
//          if (tempcounter == 9) {
//            tempcounter = 30; 
//          }
//      } 
//      if (menulevel == 1 & menucounter == 2) {
//          modecounter--;
//          if (modecounter == -1) {
//            modecounter = 2; 
//          }
//      } 
//    }
////    Serial.print("DOWNmenulevel: ");Serial.println(menulevel); 
////    Serial.print("DOWNmenucounter: ");Serial.println(menucounter); 
//  }
//  prevButtonDown_state = buttonDown_state;  


  if (menulevel == 0) {
    switch (menucounter){
        case 0:
          lcd.setCursor(0,0);
          lcd.print("Set Schedule"); lcd.print("        "); 
          lcd.setCursor(0,1);
          lcd.print("Target: "); lcd.print(target_temp); lcd.print("C        ");  
          break;
        case 1:
          lcd.setCursor(0,0);
          lcd.print("Set Temperature"); lcd.print("        "); 
          lcd.setCursor(0,1);
          lcd.print("Target: "); lcd.print(target_temp); lcd.print("C        ");           
          break;
        case 2:
          lcd.setCursor(0,0);
          lcd.print("Set Mode"); lcd.print("        "); 
          lcd.setCursor(0,1);
          lcd.print("Target: "); lcd.print(target_temp); lcd.print("C        ");           
          break;
      }
  }     
  
  //schedule time
  if (menulevel == 1 && menucounter == 0) {
      //schedule         
      switch (schecounter){
          case 10:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 11:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 12:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 13:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 14:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 15:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 16:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 17:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 18:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 19:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 20:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 21:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 22:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 23:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 24:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 25:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 26:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 27:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 28:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 29:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;
          case 30:
            lcd.setCursor(0,0);
            lcd.print("Set Sche: "); lcd.print(weekDays[buffflag]); lcd.print("        ");         
            lcd.setCursor(0,1);
            lcd.print(schecounter); lcd.print(" --> "); lcd.print(scheMsg[scheflag]); lcd.print("        ");       
            break;             
      }
    } 
    if (menulevel == 1 && menucounter == 1) {  //targetTemp
        switch (tempcounter) { //cycle from 20-30 (tempcounter), save new targetTemp           
          case 10:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");
            break;
          case 11:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");  
            break;
          case 12:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");      
            break;
          case 13:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 14:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");      
            break;
          case 15:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");      
            break;
          case 16:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");     
            break;
          case 17:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");
            break;
          case 18:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 19:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 20:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 21:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 22:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 23:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 24:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 25:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 26:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 27:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 28:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 29:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break;
          case 30:
            lcd.setCursor(0,0);
            lcd.print("Set Target Temp:             ");         
            lcd.setCursor(0,1);
            lcd.print(tempcounter); lcd.print("°C"); lcd.print("            ");       
            break; 
        }
    }  
    if (menulevel == 1 && menucounter == 2) {
        switch(modecounter) {
          case 0:
            lcd.setCursor(0,0);
            lcd.print("Set Mode:               ");
            lcd.setCursor(0,1);
            lcd.print("AC                      ");
            break;
  
          case 1:
            lcd.setCursor(0,0);
            lcd.print("Set Mode:               ");
            lcd.setCursor(0,1);
            lcd.print("Heat                    ");
            break;
          case 2:
            lcd.setCursor(0,0);
            lcd.print("Set Mode:               ");
            lcd.setCursor(0,1);
            lcd.print("Off                    ");
            break;
        }
    }
}

void setup_wifi() {
#ifdef DEBUG 
  Serial.println("");
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
#endif
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif 
  }
#ifdef DEBUG
  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
#endif
}

void callback(char* topic, byte* payload, unsigned int length) {
#ifdef DEBUG
  Serial.print(F("Message arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif  
  payload[length] = '\0';
  String s = String((char*)payload);
  if (strcmp(topic, "in/esp128/dht22/temp") == 0) {
    temp = atof(topic);
  }
  if (strcmp(topic, "in/esp128/dht22/humid") == 0) {
    humid = atof(topic);
  }
  
  if (strcmp(topic, "in/esp128/relay_ac") == 0) {
#ifdef DEBUG    
    Serial.print("ac topic");
#endif    
    if (s == "ON") {
#ifdef DEBUG      
      Serial.print("ac on");
#endif      
      digitalWrite(relay_ac, LOW);
      digitalWrite(relay_fan,LOW);
      digitalWrite(relay_heat,HIGH);
      hvac_state = 1;
    } else if (s == "OFF") {
      digitalWrite(relay_ac,HIGH);
      digitalWrite(relay_fan,HIGH);
      hvac_state = 0;
    }
  } else if (strcmp(topic, "in/esp128/relay_heat") == 0) {
    if (s == "ON") {
      digitalWrite(relay_heat, LOW);
      digitalWrite(relay_fan,LOW);
      digitalWrite(relay_ac,HIGH);
      hvac_state = 2;
    } else if (s == "OFF") {
      digitalWrite(relay_heat,HIGH);
      digitalWrite(relay_fan,HIGH);
      hvac_state = 0;
    }
  }  
}

void setup_mqtt() { 
  // Loop until we're reconnected
  while (!client.connected()) {
#ifdef DEBUG    
    Serial.print("Attempting MQTT connection...");
#endif    
    if (client.connect("esp128", "blimpi", "4167423398")) {
#ifdef DEBUG      
      Serial.println("connected");
#endif      
      // ... and resubscribe
      client.subscribe("in/esp128/relay_ac");
      client.subscribe("in/esp128/relay_heat");
      client.subscribe("in/esp128/dht22/temp");
      client.subscribe("in/esp128/dht22/humid");
    } else {
#ifdef DEBUG      
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
#endif      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void updateTemp() {

#ifdef DEBUG  
  Serial.print(F("\nTemp: "));  Serial.println(temp);
  Serial.print(F("Humid: "));  Serial.println(humid);
#endif  
  //send temp reading to openhab
  client.publish("out/openhab/dht22/temp", dtostrf(temp, 5, 2, msg));
  client.publish("out/openhab/dht22/humid", dtostrf(humid, 5, 2, msg));  

  //update temp schedule from EEPROM
  eeAddress = 0;
  for (int cnt = 0; cnt < sizeof(day) / sizeof(day[0]); cnt++) {
    EEPROM.get(eeAddress, day[cnt]);
#ifdef DEBUG
    Serial.print(F("\n6am temp:  ")); Serial.println(day[cnt].six_am);
    Serial.print(F("8am temp:  ")); Serial.println(day[cnt].eight_am);
    Serial.print(F("6pm temp:  ")); Serial.println(day[cnt].six_pm);
    Serial.print(F("10pm temp:  ")); Serial.println(day[cnt].ten_pm);
#endif
    eeAddress += sizeof(schedule);
  }

  //update targetTemp at scheduled times
  //6am 
  if (timeClient.getHours() == 6) {
    target_temp = day[timeClient.getDay()].six_am;
#ifdef DEBUG
    Serial.print(F("targetTemp is:  ")); Serial.println(target_temp);
#endif    
  }  
  //8am 
  if (timeClient.getHours() == 8) {
    target_temp = day[timeClient.getDay()].eight_am;
  }  
  //6pm 
  if (timeClient.getHours() == 18) {
    target_temp = day[timeClient.getDay()].six_pm;
  }  
  //10pm 
  if (timeClient.getHours() == 22) {
    target_temp = day[timeClient.getDay()].ten_pm;
  }  
}

void HVAC_control() {
  if (hvac_state == 1) {  //ac mode
    if (temp > target_temp) {
      digitalWrite(relay_ac, LOW);
      digitalWrite(relay_fan, LOW);
    }
  } else if (hvac_state == 2) { //heat mode
    if (temp < target_temp) {
      digitalWrite(relay_heat, LOW);
      digitalWrite(relay_fan, LOW);
    }
  }   
}
void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  pinMode(relay_ac, OUTPUT);
  pinMode(relay_heat, OUTPUT);  
  pinMode(relay_fan, OUTPUT);
  pinMode(buttonUpPin, INPUT);
  pinMode(buttonSelectPin, INPUT);

  digitalWrite(buttonUpPin, HIGH);
  digitalWrite(buttonSelectPin, HIGH);  
  digitalWrite(relay_fan, HIGH);
  digitalWrite(relay_ac, HIGH);
  digitalWrite(relay_heat, HIGH);
  Wire.begin(D1, D2);
  lcd.begin (16, 2); // for 16 x 2 LCD module
  lcd.setBacklight(HIGH);
  setup_wifi();
  timeClient.begin();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);  
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    setup_mqtt();    
  }
  client.loop();
  timeClient.update();
  updateTemp();
  lcd_menu();
  HVAC_control();
 
}
