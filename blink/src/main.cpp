#include "Arduino.h"
#include <Wire.h>

#define LED_BUILTIN_DO PC13

#define nRELAY_EN_DO PB7
#define nHIT_LIMIT_DI PA4
#define PRECHARGE_EN_DO PB4
#define PAUSE_DISCHARGE_DO PB5
#define RUN_RELAY_EN_DO PB6

#define TABLE_IS_OPERATIONAL_DO PB15
#define GAME_IS_ON_DO PA8

#define LED_RED_DO PA12
#define LED_GREEN_DO PA15
#define LED_BLUE_DO PA11

#define HSD_EN_DO PA1
#define HSD_LATCH_DO PA2
#define HSD_SEL1_DO PA0
#define HSD_SNS_AI PA3

#define CAP_VOLTAGE_AI PA6
#define TABLE_RELAY_COIL_VOLTAGE_AI PA7
#define PRECHARGE_RELAY_COIL_VOLTAGE_AI PB0

HardwareSerial Serial2(PA10,PA9);

float HSD_get_current(){
  digitalWrite(HSD_SEL1_DO, 0);
  return analogRead(HSD_SNS_AI)*5000/300; //5000=Ksns, 300=Rsns
}
float HSD_get_temperature(){
  digitalWrite(HSD_SEL1_DO, 1);
  //return (analogRead(HSD_SNS_AI)*294.12)-50.147;
  return (analogRead(HSD_SNS_AI)-0.5683)/0.0112;
}
void HSD_enable(){
  digitalWrite(HSD_EN_DO, 1);
}
void HSD_disable(){
  digitalWrite(HSD_EN_DO, 0);
}
void HSD_latch_faults(){
  digitalWrite(HSD_LATCH_DO, 1);
}
void HSD_retry_after_faults(){
  digitalWrite(HSD_LATCH_DO, 0);
}

float get_table_relay_coil_voltage(){
  return analogRead(TABLE_RELAY_COIL_VOLTAGE_AI)*3.3*7.9/1023;
}
float get_precharge_relay_coil_voltage(){
  return analogRead(PRECHARGE_RELAY_COIL_VOLTAGE_AI)*3.3*7.9/1023;
}

enum color{
  BLACK,
  RED,
  BLUE,
  GREEN,
  PURPLE,
  CYAN,
  YELLOW,
  WHITE
};

void setLED(color LEDcolor){
  int red_state = 0;
  int green_state = 0;
  int blue_state = 0;
  
  switch(LEDcolor){
    case BLACK:
      red_state = 0;
      green_state = 0;
      blue_state = 0;
    break;
    case RED:
      red_state = 1;
      green_state = 0;
      blue_state = 0;
    break;
    case GREEN:
      red_state = 0;
      green_state = 1;
      blue_state = 0;
    break;
    case BLUE:
      red_state = 0;
      green_state = 0;
      blue_state = 1;
    break;
    case PURPLE:
      red_state = 1;
      green_state = 0;
      blue_state = 1;
    break;
    case CYAN:
      red_state = 0;
      green_state = 1;
      blue_state = 1;
    break;
    case YELLOW:
      red_state = 1;
      green_state = 1;
      blue_state = 0;
    break;
    case WHITE:
      red_state = 1;
      green_state = 1;
      blue_state = 1;
    break;
  }
  digitalWrite(LED_RED_DO, red_state);
  digitalWrite(LED_GREEN_DO, green_state);
  digitalWrite(LED_BLUE_DO, blue_state);
}

void setup()
{
  Serial2.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(nRELAY_EN_DO, OUTPUT);
  pinMode(PAUSE_DISCHARGE_DO, OUTPUT);

  pinMode(LED_RED_DO, OUTPUT);
  pinMode(LED_GREEN_DO, OUTPUT);
  pinMode(LED_BLUE_DO, OUTPUT);

  digitalWrite(nRELAY_EN_DO, LOW);
  digitalWrite(PAUSE_DISCHARGE_DO, HIGH);
  digitalWrite(PRECHARGE_EN_DO, 1);
  digitalWrite(RUN_RELAY_EN_DO, 1);
  HSD_enable();
}

void loop()
{
  setLED(WHITE);
  Serial2.println(HSD_get_current());
  delay(1000);
  setLED(RED);
  Serial2.println(HSD_get_temperature());
  delay(1000);
  setLED(BLACK);
  Serial2.println("Black");
  delay(1000);

}

