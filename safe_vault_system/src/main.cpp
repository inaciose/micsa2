#include "I2Cdev.h"
#include "MPU6050.h"
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <Arduino.h>
#include <MFRC522.h>
#include <LiquidCrystal_PCF8574.h>
#include <Adafruit_PWMServoDriver.h>
#include <avr/wdt.h>

/*
// Keypad Configuration
*/

#define ROW_NUM 4
#define COLUMN_NUM 3

/*
// Keypad configuration
*/

char keys[ROW_NUM][COLUMN_NUM] = {{'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'} };
byte   pin_rows[ROW_NUM] = {10, 9, 8, 7}; byte pin_column[COLUMN_NUM] = {6, 5, 4};
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

/*
// Display Configuration
*/

LiquidCrystal_PCF8574 lcd(0x27);

/*
// RFID Configuration
*/

#define SS_PIN 2
#define RST_PIN 3

String config_tag_id = "baaeb115";
String open_tag_id = "4ae1c8a646984";
MFRC522 rfid_reader(SS_PIN, RST_PIN); 

/*
// EEPROM
*/

#define EPROM_BASE_PIN_CODE 0
#define EPROM_PIN_CODE_BYTES 6
#define EPROM_PIN_CODE_TRIES 6

/*
// Gyroscope
*/

MPU6050 accelgyro;
int16_t ax, ay, az, gx, gy, gz;
int16_t prev_ax, prev_ay, prev_az, prev_gx, prev_gy, prev_gz;

/*
// Servo configuration
*/

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVOMIN  250 // This is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  400 // This is the 'maximum' pulse length count (out of 4096)
//#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
//#define USMAX  2400 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
// our servo # counter
uint8_t servonum = 0;


/*
// Other pin definitions
*/

#define RED_LED_PIN 13
#define DOOR_SWITCH_PIN A0
#define BUZZER_PIN A1
#define GREEN_LED_PIN A2

char pin_code[6] = {'0', '0', '0', '0', '0', '0'};
byte number_of_tries = 3;
int movement_sensitivity = 1500;

/*
// State machine declaration
//
// State machine with 5 main states: V_IDLE, V_AUTH, V_ALARM, V_OPENED, V_CONFIG
// V_IDLE: Waiting for a card to be scanned
// V_AUTH: Waiting for a valid PIN to be entered
// V_ALARM: Activate the buzzer and the LED while asking for a PIN
// V_OPENED: Open the door and wait for the switch to be closed
// V_CONFIG: Configure the system
*/

enum State { V_IDLE, V_AUTH, V_ALARM, V_OPENED, V_CONFIG };
enum ConfigState { V_CHANGE_PIN, V_CHANGE_NUMBER_OF_TRIES, V_EXIT_CONFIG };
State state = V_IDLE; ConfigState config_state = V_CHANGE_PIN; // Set startup states
int auth_destination = 0; // Auth destination global signal 0 for OPENED, 1 for CONFIG

/*
// Functions
*/

void loaded_stored_values(void) {
  // using globals 
  // loaded stored pin code
  for(int f=0; f<EPROM_PIN_CODE_BYTES; f++ ) {
    pin_code[f] = EEPROM.read(EPROM_BASE_PIN_CODE + f);
  }

  // loaded stored pin tries
  number_of_tries = EEPROM.read(EPROM_PIN_CODE_TRIES);

}

void update_stored_values(void) {
  // using globals 
  // loaded stored pin code
  for(int f=0; f<EPROM_PIN_CODE_BYTES; f++ ) {
    EEPROM.update(EPROM_BASE_PIN_CODE + f, pin_code[f]);
  }

  // loaded stored pin tries
  EEPROM.update(EPROM_PIN_CODE_TRIES, number_of_tries);

}

bool ongoing_intrusion(bool no_movement = true) {
  wdt_reset();
  if (digitalRead(DOOR_SWITCH_PIN) == LOW) { return true; }
  if (no_movement) {
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    if (abs(ax - prev_ax) > movement_sensitivity || abs(ay - prev_ay) > movement_sensitivity || abs(az - prev_az) > movement_sensitivity) {
      return true;
    }
    prev_ax = ax; prev_ay = ay; prev_az = az;
  }
  return false;
}

void read_pin_code(char* entered_pin_code, bool in_alarm, bool no_movement = true) {
  /*
  // Reads the pin code and writes it to a given position in memory
  */

  lcd.setBacklight(255);
  lcd.setCursor(0, 1);
  lcd.print("     ------     ");
  
  for(int i = 0; i < 6;)
  {
    if (in_alarm) {
      digitalWrite(RED_LED_PIN, HIGH);
      int potentiometer_value = analogRead(A3);
      tone(BUZZER_PIN,map(potentiometer_value, 0, 1023, 200, 40000),1000);
      }
    char key = keypad.getKey();
    if (key != NO_KEY)
    {
      entered_pin_code[i] = key;
      lcd.setCursor(5 + i, 1);
      lcd.print(key);
      i++;
    }
    if (in_alarm) { delay(50); digitalWrite(RED_LED_PIN, LOW); digitalWrite(BUZZER_PIN, LOW); delay(50);}
    if (ongoing_intrusion(no_movement) && !in_alarm) {state = V_ALARM; break;}
  }
}

String read_rfid_tag() {
  String IDtag = "";
  if ( !rfid_reader.PICC_IsNewCardPresent() || !rfid_reader.PICC_ReadCardSerial() ) { delay(50); return IDtag; }

  // Uses rfid_reader.uid to store the unique identification on the IDtag variable    
  for (byte i = 0; i < rfid_reader.uid.size; i++) {        
      IDtag.concat(String(rfid_reader.uid.uidByte[i], HEX));
  }        
  return IDtag;
}

/*
// Main
*/

void setup() {
  // Define a default pin for when 
  //pin_code[0] = '1';
  //pin_code[1] = '1';
  //pin_code[2] = '1';
  //pin_code[3] = '1';
  //pin_code[4] = '1';
  //pin_code[5] = '1';
  //number_of_tries = 5;
  //update_stored_values();


  // Start serial port and wait for a connection.
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Status: Serial connected");

  wdt_disable(); // Disable watchdog timer
  wdt_enable(WDTO_8S); // Enable watchdog timer with 8 seconds timeout

  // Setup display
  Wire.begin();
  Wire.beginTransmission(0x27);
  Wire.endTransmission();
  lcd.begin(16, 2);

  // SPI and RFID initialization
  SPI.begin();
  rfid_reader.PCD_Init();

  // Others
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

    // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
      Wire.begin();
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
      Fastwire::setup(400, true);
  #endif

  Serial.println("Initializing I2C devices...");
  accelgyro.initialize();

  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  // initialize PCA9685
  Serial.println("Initializing PCA9685");
  pwm.begin();
  //Serial.println("pwm.begin");
  pwm.setOscillatorFrequency(27000000);
  //Serial.println("pwm.setOscillatorFrequency");
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
  //Serial.println("pwm.setPWMFreq");


  Serial.println("Status: Setup complete");

  loaded_stored_values();

  prev_ax = accelgyro.getAccelerationX();
  prev_ay = accelgyro.getAccelerationY();
  prev_az = accelgyro.getAccelerationZ();
  prev_gx = accelgyro.getRotationX();
  prev_gy = accelgyro.getRotationY();
  prev_gz = accelgyro.getRotationZ();
}  // setup()

void loop() {
  switch (state) {
    case V_IDLE: {
      /*
      Wait for a card to be scanned. If a valid card is scanned, go to V_AUTH state and set the auth_destination to 0 (V_OPENED) or 1 (V_CONFIG) depending on the card. Listen for possible movments detected by the accelerometer. If a movement is detected, go to V_ALARM state. Listen if the switch is high (door open). If that appens, go to V_ALARM state.
      
      Note 1: the scanning module is not yet implemented so, for now, we will simulate a card scan by reading the serial port: opn (auth destination 0) or cfg (auth destination 1)
      Note 2: the accelerometer is not yet implemented so, for now, we will simulate a movement by reading the serial port: mov
      */
      lcd.clear();
      lcd.setBacklight(0);
      while (1)
      {
        if (Serial.available() > 0)
        {
          String input = Serial.readStringUntil('\n');
          if (input == "opn")
          {
            auth_destination = 0;
            state = V_AUTH;
            break;
          }
          else if (input == "cfg")
          {
            auth_destination = 1;
            state = V_AUTH;
            break;
          }
          else if (input == "mov")
          {
            state = V_ALARM;
            break;
          }
        }
        if (read_rfid_tag() == open_tag_id) {
          auth_destination = 0;
          state = V_AUTH;
          break;
        }
        if (read_rfid_tag() == config_tag_id) {
          auth_destination = 1;
          state = V_AUTH;
          break;
        }
        if ( ongoing_intrusion() ) {
          state = V_ALARM;
          break;
        }
      }
      break;
    }
    case V_AUTH: {
      // Wait for a valid PIN to be entered
      // If a valid PIN is entered, go to V_OPENED state
      // If an invalid PIN is entered, go to V_ALARM state
      
      unsigned short int i = 0;
      for (; i < number_of_tries; i++)
      {
        lcd.clear();
        lcd.setBacklight(255);
        lcd.setCursor(0, 0);
        lcd.print("   ENTER PIN:   ");
        
        char entered_pin_code[6];
        read_pin_code(entered_pin_code, false);

        if (!strncmp(entered_pin_code, pin_code, 6)) { break; }
      }

      if (i >= number_of_tries) { state = V_ALARM; break;}
      if (auth_destination) {
        lcd.clear();
        lcd.setBacklight(255);
        lcd.setCursor(0, 0);
        lcd.print("    SETTINGS    ");
        lcd.setCursor(0, 1);
        lcd.print("      MENU      ");
        delay(3000);
        state = V_CONFIG;
        config_state = V_CHANGE_PIN;
        break;
      }
      state = V_OPENED; break;
    }
    case V_ALARM: {
      // Activate the buzzer and the LED while asking for a PIN
      // If a valid PIN is entered, go to V_IDLE state
      // If an invalid PIN is entered, go to V_ALARM state

      lcd.clear();
      lcd.setBacklight(255);
      lcd.setCursor(0, 0);
      lcd.print("     ALARM!     ");
      
      while (1) {
        char entered_pin_code[6];
        read_pin_code(entered_pin_code, true);
        if (!strncmp(entered_pin_code, pin_code, 6)) { break; }
      }

      digitalWrite(RED_LED_PIN, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("     ALARM      ");
      lcd.setCursor(0, 1);
      lcd.print("    DISABLED    ");
      delay(5000);
      state = V_IDLE;
      accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
      prev_ax = ax; prev_ay = ay; prev_az = az;
      break;
    }
    case V_OPENED: {
      // Open the door and wait for the switch to be closed
      // If the switch is closed, go to V_IDLE state
      
      lcd.clear();
      lcd.setBacklight(255);
      lcd.setCursor(0, 0);
      lcd.print("      DOOR      ");
      lcd.setCursor(0, 1);
      lcd.print("    UNLOCKED    ");
      digitalWrite(GREEN_LED_PIN, HIGH);
      // ROTATE SERVO
      for (uint16_t pulselen = SERVOMIN; pulselen < SERVOMAX; pulselen++) {
        pwm.setPWM(servonum, 0, pulselen);
      }
      delay(5000);
      while (1)
      {
        wdt_reset();
        if (digitalRead(DOOR_SWITCH_PIN) == HIGH) {
          // ROTATE SERVO
          digitalWrite(GREEN_LED_PIN, LOW);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("      DOOR      ");
          lcd.setCursor(0, 1);
          lcd.print("     LOCKED     ");
          delay(100);
          for (uint16_t pulselen = SERVOMAX; pulselen > SERVOMIN; pulselen--) {
            pwm.setPWM(servonum, 0, pulselen);
          }
          delay(5000);
          state = V_IDLE;
          break;
        }
      }
      break;
    }
    case V_CONFIG: {
      switch(config_state) {
        case V_CHANGE_PIN: {
          // Change the PIN
          lcd.clear();
          lcd.setBacklight(255);
          lcd.setCursor(0, 0);
          lcd.print("   CHANGE PIN   ");
          lcd.setCursor(0, 1);
          lcd.print("* OK      NEXT #");

          while (1) {
            char key = keypad.getKey();
            if (key == '#')
            {
              config_state = V_CHANGE_NUMBER_OF_TRIES;
              break;
            }
            else if (key == '*')
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("   ENTER PIN:   ");
              read_pin_code(pin_code, false, false);
              update_stored_values();
              break;
            }
            if (ongoing_intrusion(false)) { state = V_ALARM; break; }
          }
          break;
        }
        case V_CHANGE_NUMBER_OF_TRIES: {
          // Change the number of tries
          lcd.clear();
          lcd.setBacklight(255);
          lcd.setCursor(0, 0);
          lcd.print("  CHANGE TRIES  ");
          lcd.setCursor(0, 1);
          lcd.print("* OK      NEXT #");

          while (1) {
            char key = keypad.getKey();
            if (key == '#')
            {
              config_state = V_EXIT_CONFIG;
              break;
            }
            else if (key == '*')
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("ENTER N O TRIES:");
              while (1)
              {
                delay(1);
              }
              break;
            }
            if (ongoing_intrusion(false)) { state = V_ALARM; break; }
          }
          break;
        }
        case V_EXIT_CONFIG: {
          // Exit the configuration menu
          lcd.clear();
          lcd.setBacklight(255);
          lcd.setCursor(0, 0);
          lcd.print("   EXIT CONFIG  ");
          lcd.setCursor(0, 1);
          lcd.print("* OK      NEXT #");

          while (1) {
            char key = keypad.getKey();
            if (key == '#')
            {
              config_state = V_CHANGE_PIN;
              break;
            }
            else if (key == '*')
            {
              lcd.clear();
              lcd.setBacklight(255);
              lcd.setCursor(0, 0);
              lcd.print("    EXITING     ");
              lcd.setCursor(0, 1);
              lcd.print("    SETTINGS    ");
              delay(3000);
              state = V_IDLE;
              break;
            }
            if (ongoing_intrusion(false)) { state = V_ALARM; break; }
          }
          break;
        }
        if (ongoing_intrusion(false)) { state = V_ALARM; break; }
      }

      lcd.setCursor(0, 0);
      lcd.print("   CHANGE PIN   ");
      lcd.setCursor(0, 1);
      lcd.print("* OK      NEXT #");

      break;
    }
  }  // switch (state)
}  // loop()