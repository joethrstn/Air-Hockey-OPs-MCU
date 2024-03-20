#include "Arduino.h"
#include <Wire.h>

#define HSD_MAX_TEMP 100.0 //Double check with datasheet
#define HSD_MAX_CURRENT 50.0 //Double check with Altium configuration
#define LED_FLASH_DELAY 500

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
#define HI_SIDE_COIL_VOLTAGE_AI PA7
#define PRECHARGE_RELAY_COIL_VOLTAGE_AI PB0

HardwareSerial Serial2(PA10,PA9);

bool did_transition = false;
int start_millis;
bool light_state = false;
bool cap_fault = false;
bool hsd_temp_fault = false;
bool hsd_current_fault = false;
bool loop_fault = false;

float HSD_get_current(){
  digitalWrite(HSD_SEL1_DO, 0);
  return analogRead(HSD_SNS_AI)*5000/261; //5000=Ksns, 261=Rsns
}
float HSD_get_temperature(){
  digitalWrite(HSD_SEL1_DO, 1);
  return (((analogRead(HSD_SNS_AI)*3.3/1023)-0.1483)/0.0029); //5000=Ksns, 261=Rsns
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

float get_high_side_relay_coil_voltage(){
  return analogRead(HI_SIDE_COIL_VOLTAGE_AI)*3.3*7.9/1023;
}
float get_precharge_relay_coil_voltage(){
  return analogRead(PRECHARGE_RELAY_COIL_VOLTAGE_AI)*3.3*7.9/1023;
}
float get_capacitor_voltage(){
  return analogRead(CAP_VOLTAGE_AI)*3.3*7.9/1023;
}

enum hw_state{
  SW_PRECHARGE,
  SW_RUN,
  SW_OFF
};

hw_state get_hardware_state(){
  hw_state current_hw_state = SW_PRECHARGE;
  if(get_precharge_relay_coil_voltage()<20 && get_precharge_relay_coil_voltage()<20 && 
    get_high_side_relay_coil_voltage()<20 && get_high_side_relay_coil_voltage()<20){ //2 reads to debounce in transition
    current_hw_state = SW_OFF;
  }
  else if(get_high_side_relay_coil_voltage()>20){
    current_hw_state = SW_RUN;
  }
  else if(get_precharge_relay_coil_voltage()>20){
    current_hw_state = SW_PRECHARGE; 
  }
  return current_hw_state;
}

enum state{
  INIT,
  PRECHARGE,
  WAIT_TO_RUN,
  RUN,
  FAULT
};

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

state state_machine_transition(state local_current_state){
  hsd_temp_fault = HSD_get_temperature()>HSD_MAX_TEMP; //TODO catch HSD self-turnoff
  hsd_current_fault = HSD_get_current()>HSD_MAX_CURRENT;
  loop_fault = get_hardware_state() == SW_OFF;
  cap_fault = get_capacitor_voltage()<22 && (local_current_state == RUN || local_current_state == WAIT_TO_RUN);
  bool is_faulted = hsd_temp_fault || hsd_current_fault || loop_fault || cap_fault;
  state local_new_state = local_current_state;
  switch(local_current_state){ //state machine transition logic
    case INIT:
      if(get_hardware_state() == SW_PRECHARGE){
        local_new_state = PRECHARGE;
      }
    break;
    case PRECHARGE:
      if(is_faulted || get_hardware_state() != SW_PRECHARGE){
        local_new_state = FAULT;
      }
      else if(get_capacitor_voltage()>23.5){
        local_new_state = WAIT_TO_RUN;
      }
    break;
    case WAIT_TO_RUN:
      if(is_faulted){
        local_new_state = FAULT;
      }
      else if(get_hardware_state() == SW_RUN){
        local_new_state = RUN;
      }
    break;
    case RUN:
      if(is_faulted || get_hardware_state() != SW_RUN){
        local_new_state = FAULT;
      }
    break;
    case FAULT:
      if(!is_faulted){
        local_new_state = INIT;
      }
    break;
  }
  return local_new_state;
}

state current_state = INIT;
state new_state = INIT;

void setup()
{
  Serial2.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(nRELAY_EN_DO, OUTPUT);
  pinMode(PAUSE_DISCHARGE_DO, OUTPUT);
  pinMode(LED_RED_DO, OUTPUT);
  pinMode(LED_GREEN_DO, OUTPUT);
  pinMode(LED_BLUE_DO, OUTPUT);
  pinMode(nHIT_LIMIT_DI, INPUT);
  pinMode(PRECHARGE_EN_DO, OUTPUT);
  pinMode(RUN_RELAY_EN_DO, OUTPUT);
  pinMode(TABLE_IS_OPERATIONAL_DO, OUTPUT);
  pinMode(GAME_IS_ON_DO, OUTPUT);
  pinMode(HSD_EN_DO, OUTPUT);
  pinMode(HSD_LATCH_DO, OUTPUT);
  pinMode(HSD_SEL1_DO, OUTPUT);

  digitalWrite(nRELAY_EN_DO, LOW); //Set SW shdn to ok state
  digitalWrite(PAUSE_DISCHARGE_DO, HIGH); //Do not pause discharge during a loop-open event
  digitalWrite(PRECHARGE_EN_DO, LOW); //Open precharge relay
  digitalWrite(RUN_RELAY_EN_DO, LOW); //Open run relay
  HSD_retry_after_faults();
}

void loop()
{
  new_state = state_machine_transition(current_state); //check if state transition is required
  did_transition = (new_state != current_state);
  if(did_transition){ //run on exit blocks
    switch(current_state){
      case INIT:
        //do nothing
      break;
      case PRECHARGE:
        //do nothing
      break;
      case WAIT_TO_RUN:
        digitalWrite(PRECHARGE_EN_DO, LOW); //Open precharge relay
      break;
      case RUN:
        digitalWrite(RUN_RELAY_EN_DO, LOW); //Open run relay
        digitalWrite(TABLE_IS_OPERATIONAL_DO, LOW); //Tell controls processor that table is not ready to go
      break;
      case FAULT:
        //do nothing?
      break;
    }
  }
  current_state = new_state; //state transition
  if(did_transition){ //run on entry blocks
    switch(current_state){
      case INIT:
        digitalWrite(PRECHARGE_EN_DO, LOW); //Open precharge relay
        digitalWrite(RUN_RELAY_EN_DO, LOW); //Open run relay
      break;
      case PRECHARGE:
        digitalWrite(PRECHARGE_EN_DO, HIGH); //Close precharge relay
        HSD_enable();
      break;
      case WAIT_TO_RUN:
        start_millis = millis();
      break;
      case RUN:
        digitalWrite(RUN_RELAY_EN_DO, HIGH); //Close run relay
        digitalWrite(TABLE_IS_OPERATIONAL_DO, HIGH); //Tell controls processor that table is ready to go
      break;
      case FAULT:
        HSD_disable();
      break;
    }
  }
  switch(current_state){ //run on loop blocks
    case INIT:
      setLED(WHITE);
    break;
    case PRECHARGE:
      setLED(YELLOW);
    break;
    case WAIT_TO_RUN:
      if(millis()>start_millis+LED_FLASH_DELAY){ //flash RGB in green, black state
        if(!light_state){
          setLED(GREEN);
          start_millis = millis();
          light_state = true;
        }
        else{
          setLED(BLACK);
          start_millis = millis();
          light_state = false;
        }
      }
    break;
    case RUN:
      setLED(GREEN);
    break;
    case FAULT:
      if(cap_fault){
          setLED(BLUE); //TODO
          digitalWrite(nRELAY_EN_DO, HIGH);
        }
      else if(hsd_temp_fault || hsd_current_fault){
        setLED(PURPLE);
        digitalWrite(nRELAY_EN_DO, HIGH);
      }
      else if(loop_fault){
        setLED(RED);
        digitalWrite(nRELAY_EN_DO, LOW);
      }
    break;
  } 
}

