#include <LiquidCrystal_I2C.h>
#include <DS1307RTC.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Wire.h>
#include <TimeLib.h>
#include <avr/wdt.h>

#define I2CADDR 0x20
#define DS1307_ADDRESS 0x68

#define HOUR_ADDRESS 0x08
#define MINUTE_ADDRESS 0x09
#define MORNING_ADDRESS 0x0A
#define NOON_ADDRESS 0x0B
#define EVENING_ADDRESS 0x0C
#define BEFORE_BED_ADDRESS 0x0D
#define ALARM_ADDRESS 0x0E
#define FAN_MODE_ADDRESS 0x0F

#define MORNING_TIME 8
#define NOON_TIME 12
#define EVENING_TIME 16
#define BEFOREBED_TIME 20


enum Mode {
  detection,  
  readText    
};

Mode deviceMode = detection;  

const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte rowPins[ROWS] = { 7, 6, 5, 4 };
byte colPins[COLS] = { 3, 2, 1, 0 };

Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR);
LiquidCrystal_I2C lcd(0x3F, 16, 2);

byte customChars[7][8] = {
  { B00000, B00000, B00001, B00010, B00000, B00000, B00000, B00000 },  // wifi_char_1
  { B00000, B01110, B10001, B00000, B01110, B10001, B00000, B00100 },  // wifi_char_2
  { B00000, B00000, B10000, B01000, B00000, B00000, B00000, B00000 },  // wifi_char_3
  { B00000, B00000, B00110, B00100, B00100, B00100, B01100, B00000 },  // fan_char_1
  { B00000, B00000, B10000, B11000, B00100, B00011, B00001, B00000 },  // fan_char_2
  { B00000, B00000, B00000, B00001, B11111, B10000, B00000, B00000 },  // fan_char_3
  { B00000, B00000, B00011, B00010, B00100, B01000, B11000, B00000 }   // fan_char_4
};

tmElements_t tm;
bool morning, noon, evening, beforebed, alarm, alarmStopped;
int select = 1, position = 0, frame = 0;
bool edit = false, blink_tx = false, flag = false, wifi_status = false, fan_status = false;
int Hour, Minute;
unsigned long last_time = 0, connection_time = 0, refresh_time = 0;
int MotorPin2 = 3, fan_mode;
String Temperature = "--.-";


void writeToDS1307RAM(byte address, byte data) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(address);
  Wire.write(data);
  Wire.endTransmission();
}

byte readFromDS1307RAM(int address) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 1);
  return Wire.read();
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    lcd.print('0');
  }
  lcd.print(number);
}

void Blink() {
  if ((millis() - last_time) > 500) {
    blink_tx = !blink_tx;
    last_time = millis();
  }
}

void pressButton() {

  switch (deviceMode) {
    case (detection):
      alarmStopped = true;
      Serial.println("STOP_ALARM");
      break;
    case (readText):
      Serial.println("Capture");
      break;
  }
}

void alarmAlert(int H, int M) {
  if (alarmStopped) {
    lcd.setCursor(0, 0);
    lcd.print("Date: ");
    print2digits(tm.Day);
    lcd.print("/");
    print2digits(tm.Month);
    lcd.print("/");
    lcd.print(tmYearToCalendar(tm.Year));
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    print2digits(tm.Hour);
    lcd.print(":");
    print2digits(tm.Minute);
    lcd.print(" ");
  } else {
    Blink();
    if (blink_tx)
      lcd.clear();
    else {
      lcd.setCursor(1, 0);
      lcd.print("Time to taking");
      lcd.setCursor(4, 1);
      lcd.print("Medicine");
    }

    if (!flag) {
      if (H == MORNING_TIME && M == 0)
        Serial.println("NOTIFICATION:Morning");  
      else if (H == NOON_TIME && M == 0)
        Serial.println("NOTIFICATION:Noon");  
      else if (H == EVENING_TIME && M == 0)
        Serial.println("NOTIFICATION:Evening");  
      else if (H == BEFOREBED_TIME && M == 0)
        Serial.println("NOTIFICATION:Beforebed");  
      else {
        String h = (Hour < 10) ? "0" + String(Hour) : String(Hour);
        String m = (Minute < 10) ? "0" + String(Minute) : String(Minute);
        Serial.println("NOTIFICATION:" + h + ":" + m);
      }
      flag = true;
    }
  }
}
void setupWatchdog() {
  wdt_enable(WDTO_8S);
  wdt_reset();
}

void processData(String data) {
  if (data == "MODE:1") {
    lcd.clear();          

  } else if (data == "MODE:2") {
    lcd.clear();         
    deviceMode = readText; 

  } else if (data == "WIFI") {
    if (!wifi_status) {    
      wifi_status = true;  
    }
    connection_time = millis(); 

  } else if (data == "LOST" && wifi_status) {

  } else if (data.startsWith("CPU:")) {
    int temp_index = data.indexOf("TEMP:") + 5; 
    Temperature = data.substring(temp_index);    
  }

  if (millis() - connection_time > 60000) {
  }
}

void setFanSpeed() {
  switch (fan_mode) {

    case 0:
      if (Temperature != "--.-") {                
        if (Temperature.toFloat() > 60) {        
          analogWrite(MotorPin2, 0);              
          fan_status = true;                      
        } else if (Temperature.toFloat() < 50) {  
          analogWrite(MotorPin2, 255);            
          fan_status = false;                     
        }
      } else {
        analogWrite(MotorPin2, 0);  
        fan_status = true;          
      }
      break;

    case 1:
      analogWrite(MotorPin2, 255);  
      fan_status = false;           
      break;

    case 2:
      analogWrite(MotorPin2, 0);  
      fan_status = true;         
      break;
  }
}

void setupDataFromDS1307RAM() {
  Hour = readFromDS1307RAM(HOUR_ADDRESS);
  Minute = readFromDS1307RAM(MINUTE_ADDRESS);
  if (Hour >= 24 || Hour < 0)
    Hour = 0;
  if (Minute >= 60 || Minute < 0)
    Minute = 0;

  morning = readFromDS1307RAM(MORNING_ADDRESS);
  noon = readFromDS1307RAM(NOON_ADDRESS);
  evening = readFromDS1307RAM(EVENING_ADDRESS);
  beforebed = readFromDS1307RAM(BEFORE_BED_ADDRESS);
  alarm = readFromDS1307RAM(ALARM_ADDRESS);
  fan_mode = readFromDS1307RAM(FAN_MODE_ADDRESS);

  if (fan_mode >= 3 || fan_mode < 0) {
    fan_mode = 0;
    writeToDS1307RAM(FAN_MODE_ADDRESS, fan_mode);
  }
}

void setup() {
  sei();
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), pressButton, FALLING);
  Wire.begin();
  keypad.begin();
  Serial.begin(9600);
  while (!Serial) {}
  lcd.init();
  lcd.home();
  setupWatchdog();
  setupDataFromDS1307RAM();
  pinMode(MotorPin2, OUTPUT);

  for (int i = 0; i < 7; i++) {
    lcd.createChar(i, customChars[i]);
  }
}

void loop() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    processData(data);
  }

  char key = keypad.getKey();
  if (key != NO_KEY) {
    wdt_reset();
    
    if (key == 'A') {
      select++;
      if (select > 7)
        select = 1;
      lcd.clear();
    } 
    else if (key == 'C') {
      lcd.clear();
      deviceMode = detection;
      Serial.println("MODE:1");
    } 
    else if (key == 'D') {
      lcd.clear();
      Serial.println("MODE:2");
      deviceMode = readText;
    }
  }

  switch (deviceMode) {
    case (detection):
      switch (select) {
        case 1:
          if (RTC.read(tm)) {
            if (tm.Hour == Hour && tm.Minute == Minute && alarm) {
              alarmAlert(Hour, Minute);
            } else if (tm.Hour == MORNING_TIME && tm.Minute == 0 && morning) {
              alarmAlert(Hour, Minute);
            } else if (tm.Hour == NOON_TIME && tm.Minute == 0 && noon) {
              alarmAlert(Hour, Minute);
            } else if (tm.Hour == EVENING_TIME && tm.Minute == 0 && evening) {
              alarmAlert(Hour, Minute);
            } else if (tm.Hour == BEFOREBED_TIME && tm.Minute == 0 && beforebed) {
              alarmAlert(Hour, Minute);
            } else {
              if (flag) {
                Serial.println("STOP_ALARM");
              }
              flag = false;
              lcd.setCursor(0, 0);
              lcd.print("Date: ");
              print2digits(tm.Day);
              lcd.print("/");
              print2digits(tm.Month);
              lcd.print("/");
              lcd.print(tmYearToCalendar(tm.Year));
              lcd.setCursor(0, 1);
              lcd.print("Time: ");
              print2digits(tm.Hour);
              lcd.print(":");
              print2digits(tm.Minute);
              lcd.print(" ");
              if (wifi_status) {
                lcd.write(byte(0));
                lcd.write(byte(1));
                lcd.write(byte(2));
              } else {
                Blink();
                if (blink_tx)
                  lcd.print("   ");
                else {
                  lcd.write(byte(0));
                  lcd.write(byte(1));
                  lcd.write(byte(2));
                }
              }
              if (fan_status) {
                if (millis() - refresh_time > 100) {
                  if (frame == 0) {
                    lcd.write(byte(3));
                    frame = (frame + 1) % 4;
                  } else if (frame == 1) {
                    lcd.write(byte(4));
                    frame = (frame + 1) % 4;
                  } else if (frame == 2) {
                    lcd.write(byte(5));
                    frame = (frame + 1) % 4;
                  } else if (frame == 3) {
                    lcd.write(byte(6));
                    frame = (frame + 1) % 4;
                  }
                  refresh_time = millis();
                }
              } else {
                lcd.write(byte(6));
              }
              alarmStopped = false;
            }
          } else {
            lcd.setCursor(0, 0);
            lcd.print("Can't Read Time");
          }
          wdt_reset();
          break;
        
        case 2:
          lcd.setCursor(0, 0);
          lcd.print("Alarm Morning");
          lcd.setCursor(0, 1);
          if (key == 'B') {
            morning = !morning;
            writeToDS1307RAM(MORNING_ADDRESS, morning);
            lcd.clear();
            lcd.setCursor(0, 1);
          }
          if (morning) {
            lcd.print("Status: ON");
          } else {
            lcd.print("Status: OFF");
          }
          break;
        
        case 3:
          lcd.setCursor(0, 0);
          lcd.print("Alarm Noon");
          lcd.setCursor(0, 1);
          if (key == 'B') {
            noon = !noon;
            writeToDS1307RAM(NOON_ADDRESS, noon);
            lcd.clear();
            lcd.setCursor(0, 1);
          }
          if (noon) {
            lcd.print("Status: ON");
          } else {
            lcd.print("Status: OFF");
          }
          break;

        case 4:
          lcd.setCursor(0, 0);
          lcd.print("Alarm Evening");
          lcd.setCursor(0, 1);
          if (key == 'B') {
            evening = !evening;
            writeToDS1307RAM(EVENING_ADDRESS, evening);
            lcd.clear();
            lcd.setCursor(0, 1);
          }
          if (evening) {
            lcd.print("Status: ON");
          } else {
            lcd.print("Status: OFF");
          }
          break;

        case 5:
          lcd.setCursor(0, 0);
          lcd.print("Alarm BeforeBed");
          lcd.setCursor(0, 1);
          if (key == 'B') {
            beforebed = !beforebed;
            writeToDS1307RAM(BEFORE_BED_ADDRESS, beforebed);
            lcd.clear();
            lcd.setCursor(0, 1);
          }
          if (beforebed) {
            lcd.print("Status: ON");
          } else {
            lcd.print("Status: OFF");
          }
          break;

        case 6:
          lcd.setCursor(0, 0);
          lcd.print("Set Alarm");
          if (key == '*') {
            edit = true;
          } else if (key == '#') {
            writeToDS1307RAM(HOUR_ADDRESS, Hour);
            writeToDS1307RAM(MINUTE_ADDRESS, Minute);
            edit = false;
          } else if (key == 'B') {
            alarm = !alarm;
            writeToDS1307RAM(ALARM_ADDRESS, alarm);
          }
          if (edit) {
            if (key != NO_KEY && key >= '0' && key <= '9') {
              int num = key - '0';
              if (position == 0) {
                if (num <= 2) {
                  Hour = num * 10;
                  position++;
                }
              } else if (position == 1) {
                if (Hour < 20) {
                  Hour += num;
                  position++;
                } else {
                  if (num <= 3) {
                    Hour += num;
                    position++;
                  }
                }
              } else if (position == 2) {
                if (num <= 5) {
                  Minute = num * 10;
                  position++;
                }
              } else if (position == 3) {
                Minute += num;
                position = 0;
              }
            }
          }
          lcd.setCursor(0, 1);
          lcd.print("Time: ");
          if (edit) {
            Blink();
            if (position == 0) {
              if (blink_tx)
                lcd.print(" ");
              else
                lcd.print(Hour / 10);
              lcd.print(Hour % 10);
              lcd.print(":");
              print2digits(Minute);
            } else if (position == 1) {
              lcd.print(Hour / 10);
              if (blink_tx)
                lcd.print(" ");
              else
                lcd.print(Hour % 10);
              lcd.print(":");
              print2digits(Minute);
            } else if (position == 2) {
              print2digits(Hour);
              lcd.print(":");
              if (blink_tx)
                lcd.print(" ");
              else
                lcd.print(Minute / 10);
              lcd.print(Minute % 10);
            } else if (position == 3) {
              print2digits(Hour);
              lcd.print(":");
              lcd.print(Minute / 10);
              if (blink_tx)
                lcd.print(" ");
              else
                lcd.print(Minute % 10);
            }
          } else {
            print2digits(Hour);
            lcd.print(":");
            print2digits(Minute);
          }
          if (alarm) {
            lcd.print("   ON");
          } else {
            lcd.print("  OFF");
          }
          break;

        case 7:
          if (key == 'B') {
            fan_mode = (fan_mode + 1) % 3;
            writeToDS1307RAM(FAN_MODE_ADDRESS, fan_mode);
            lcd.clear();
          }
          lcd.setCursor(0, 0);
          lcd.print("Set Fan");
          lcd.print("   ");
          lcd.print(Temperature);
          lcd.print((char)223);
          lcd.print("C");
          lcd.print(" --.-");
          lcd.print((char)223);
          lcd.print("C");
          lcd.setCursor(0, 1);
          lcd.print("Status: ");
          if (fan_mode == 0) {
            lcd.print("AUTO");
          } else if (fan_mode == 1) {
            lcd.print("OFF");
          } else if (fan_mode == 2) {
            lcd.print("ON");
          }
          break;
      }
      break;

    case (readText):
      wdt_reset();
      lcd.setCursor(0, 0);
      lcd.print("   Read  Text   ");
      lcd.setCursor(0, 1);
      lcd.print("  Press switch  ");
      break;
  }

  setFanSpeed();
}