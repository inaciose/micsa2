#include <SPI.h>
#include <Wire.h>
#include <Keypad.h>
#include <Arduino.h>
#include <MFRC522.h>
#include <LiquidCrystal_PCF8574.h>

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
// Other pin definitions
*/

#define RED_LED_PIN 13
#define DOOR_SWITCH_PIN A0
#define BUZZER_PIN A1
#define GREEN_LED_PIN A2

char pin_code[6] = {'0', '0', '0', '0', '0', '0'}; // TODO: read value from eeprom at startup
short unsigned int number_of_tries = 3; // TODO: read value from eeprom at startup

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

void read_pin_code(char* entered_pin_code, boolean in_alarm) {
  /*
  // Reads the pin code and writes it to a given position in memory
  */

  lcd.setBacklight(255);
  lcd.setCursor(0, 1);
  lcd.print("     ------     ");
  
  for(int i = 0; i < 6;)
  {
    if (in_alarm) { digitalWrite(RED_LED_PIN, HIGH); digitalWrite(BUZZER_PIN, HIGH); }
    char key = keypad.getKey();
    if (key != NO_KEY)
    {
      entered_pin_code[i] = key;
      lcd.setCursor(5 + i, 1);
      lcd.print(key);
      i++;
    }
    if (in_alarm) { delay(50); digitalWrite(RED_LED_PIN, LOW); digitalWrite(BUZZER_PIN, LOW); delay(50);}
    if (digitalRead(DOOR_SWITCH_PIN) == LOW && !in_alarm) {state = V_ALARM; break;}
  }
}

/*
// Main
*/

void setup() {
  // Start serial port and wait for a connection.
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Status: Serial connected");

  // Setup display
  Wire.begin();
  Wire.beginTransmission(0x27);
  Wire.endTransmission();
  lcd.begin(16, 2);

  // Others
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  Serial.println("Status: Setup complete");
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
        if (digitalRead(DOOR_SWITCH_PIN) == LOW) { state = V_ALARM; break; }
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
      delay(5000);
      while (1)
      {
        if (digitalRead(DOOR_SWITCH_PIN) == HIGH) {
          // ROTATE SERVO
          digitalWrite(GREEN_LED_PIN, LOW);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("      DOOR      ");
          lcd.setCursor(0, 1);
          lcd.print("     LOCKED     ");
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
              read_pin_code(pin_code, false);
              break;
            }
            if (digitalRead(DOOR_SWITCH_PIN) == LOW) { state = V_ALARM; break; }
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
            if (digitalRead(DOOR_SWITCH_PIN) == LOW) { state = V_ALARM; break; }
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
            if (digitalRead(DOOR_SWITCH_PIN) == LOW) { state = V_ALARM; break; }
          }
          break;
        }
        if (digitalRead(DOOR_SWITCH_PIN) == LOW) { state = V_ALARM; break; }
      }

      lcd.setCursor(0, 0);
      lcd.print("   CHANGE PIN   ");
      lcd.setCursor(0, 1);
      lcd.print("* OK      NEXT #");

      break;
    }
  }  // switch (state)
}  // loop()