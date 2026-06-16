#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AccelStepper.h>
#include <GyverStepper.h>
#include <EEPROM.h>

// ================= НАСТРОЙКИ ПИНОВ =================
const int PIN_BTN_LEFT = 4;
const int PIN_BTN_RIGHT = 16;
const int PIN_BTN_OK   = 15;
const int PIN_STEP     = 5;
const int PIN_DIR      = 18;
const int PIN_ENABLE   = 19;
const int PIN_SENS_1   = 13;
const int PIN_SENS_2   = 14;

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= ШАГОВИК =================
AccelStepper stepper(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);
const int STEPS_PER_REV = 400;

GStepper<STEPPER2WIRE> gstepper(STEPS_PER_REV, PIN_STEP, PIN_DIR);                   

// ================= СОСТОЯНИЕ =================
esp_timer_handle_t stepTimer = nullptr;
long currentStep = 0;
int stepDirection = 1;

struct MotorVals1{
  int speedVal;
  int passesVal;
} Vals;

struct MotorVals2{
  int speedVal=150;
  int passesVal=1;
} Test;

int menuCursor = 0;   // 0=Speed, 1=Passes
int passesCount = 0;
unsigned long btn0_timer = 0;
bool setup_flag = false;
bool isRunning = false;
bool displayChange_flag = true;
bool menu_select_flag = false;
bool menu_return_flag = false;
bool options_select_flag = false;
bool options_quit_flag = false;
bool changing_speed_state = false;
bool changing_passes_state = false;
bool sens1_flag = false;
bool sens2_flag = false;
bool direction = false;
long targetSteps = 0;

// Режимы работы кнопок
enum UIMode { MODE_MENU, MODE_OPTIONS };
UIMode uiMode = MODE_MENU;

// ================= АНТИДРЕБЕЗГ =================
struct Button {
    int pin;
    int lastState;
    unsigned long lastDebounce;
} buttons[5] = {
    {PIN_BTN_LEFT, HIGH, 0},
    {PIN_BTN_RIGHT, HIGH, 0},
    {PIN_BTN_OK, HIGH, 0},
    {PIN_SENS_1, HIGH, 0},
    {PIN_SENS_2, HIGH, 0}
};
const unsigned long DEBOUNCE_DELAY = 50;

// ================= ПРОТОТИПЫ =================
void handleButtons();
void handleSensors();
void onButtonPress(int btn);
void handleDisplay();
void drawDisplay();
//160 = 10 мм/c

// ================= SETUP =================
void setup() {
    Serial.begin(9600);
    for (int i = 0; i < 3; i++) pinMode(buttons[i].pin, INPUT_PULLUP);
    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, HIGH);
    pinMode(PIN_SENS_1, INPUT);
    pinMode(PIN_SENS_2, INPUT);


    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("OLED init failed"));
        for (;;);
    }
    
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setRotation(2);
    display.clearDisplay();
    display.display();
    EEPROM.begin(10);

    uiMode = MODE_MENU;
    EEPROM.get(0, Vals);

    gstepper.setRunMode(KEEP_SPEED);
    gstepper.setSpeed(abs(Vals.speedVal));    // в шагах/сек
    gstepper.setAcceleration(16000);
  }

// ================= LOOP =================
void loop() {
  if (isRunning) gstepper.tick();
  handleButtons();
  handleSensors();
  if(displayChange_flag){
    displayChange_flag=false;
    handleDisplay();
    drawDisplay();
  }
}

// ================= КНОПКИ =================
void handleButtons() {
    unsigned long now = millis();
    for (int i = 0; i < 3; i++) {
        int reading = digitalRead(buttons[i].pin);
            if (reading == LOW && buttons[i].lastState == HIGH && (millis()-buttons[i].lastDebounce>=170)) {
                displayChange_flag=true;
                buttons[i].lastDebounce = millis();
                onButtonPress(i);
            }
        buttons[i].lastState = reading;
    }
  if(buttons[0].lastState == 1)btn0_timer=now;
  if((!changing_speed_state)&&(!changing_passes_state)&&(buttons[0].lastState == 0)&&(millis()-btn0_timer >= 1200)) {
    uiMode = MODE_MENU;
    displayChange_flag=true;
  }
}

void handleSensors(){
  if (digitalRead(PIN_SENS_1)&&(!sens1_flag)){
    Vals.speedVal= 0 - abs(Vals.speedVal);
    sens1_flag = true;
    sens2_flag = false;
    direction = 1;
    gstepper.setSpeed(Vals.speedVal);
    if(setup_flag){ 
      passesCount++;
    }else{
      setup_flag = true;
      isRunning = false;
    }
    displayChange_flag = true;
    if(passesCount>=Vals.passesVal){
      passesCount=0;
      isRunning=false;
      displayChange_flag = true;
    }
    Serial.printf("Sensor 1, %d, ", Vals.speedVal);
    Serial.println(direction);

  }
    if (digitalRead(PIN_SENS_2)&&(!sens2_flag)){
    Vals.speedVal= abs(Vals.speedVal);
    sens2_flag = true;
    sens1_flag = false;
    direction = 0;
    gstepper.setSpeed(Vals.speedVal);
    Serial.printf("Sensor 2, %d, ", Vals.speedVal);
    Serial.println(direction);
  }
}

void onButtonPress(int btn) {
  if (isRunning) {
    if ((btn==0)||(btn==1)||(btn == 2)) isRunning = false;
  return;
  }
  if ((btn==0)||(btn==1)) {
    if((!changing_speed_state)&&(!changing_passes_state)){
      menuCursor = (menuCursor + 1) % 2;
    } 
    if ((uiMode==MODE_OPTIONS)&&(changing_speed_state)) {
      if((btn==0)&&(abs(Vals.speedVal)>0)) {
        if(direction){
          Vals.speedVal = 0 - (abs(Vals.speedVal)-16*5);
        }else{
          Vals.speedVal = abs(Vals.speedVal)-16*5;
        }
      }
      if((btn==1)&&(abs(Vals.speedVal)<1600)){
        if(direction){
          Vals.speedVal = 0 - (abs(Vals.speedVal)+16*5);
        }else{
          Vals.speedVal = abs(Vals.speedVal)+16*5;
        }
      }
      EEPROM.put(0, Vals);
      EEPROM.commit();
    }
      if ((uiMode==MODE_OPTIONS)&&(changing_passes_state)) {
        if((btn==0)&&(Vals.passesVal>0)) Vals.passesVal-=1;
        if((btn==1)&&(Vals.passesVal<1000)) Vals.passesVal+=1;
        EEPROM.put(0, Vals);
        EEPROM.commit();
    }
  }
  if ((btn==2)){
    if ((uiMode==MODE_MENU)&&(!changing_speed_state)&&(!changing_passes_state)) menu_select_flag = 1;
    if ((uiMode==MODE_OPTIONS)&&(!changing_speed_state)&&(!changing_passes_state)) options_select_flag = 1;
    if ((uiMode==MODE_OPTIONS)&&((changing_speed_state)||(changing_passes_state))) options_quit_flag = 1;
    //Serial.println(options_select_flag);
  } 
}

void handleDisplay(){
  switch(uiMode){
    case MODE_MENU:
      if((menuCursor==0)&&(menu_select_flag)){
        menu_select_flag = false;
        uiMode = MODE_OPTIONS;
        return;
      }
      if((menuCursor==1)&&(menu_select_flag)){
        menu_select_flag = false;
        isRunning = true;
        if(setup_flag) {
          gstepper.setSpeed(Vals.speedVal);
        } else {
          gstepper.setSpeed(abs(Vals.speedVal));
        }
      }
      break;
    case MODE_OPTIONS:
      if((menuCursor==0)&&(options_select_flag)){
          options_select_flag = false;
          changing_speed_state = true;
          changing_passes_state = false;
      }
      if((menuCursor==1)&&(options_select_flag)){
        options_select_flag = false;
        changing_passes_state = true;
        changing_speed_state = false;
      }
      if(options_quit_flag){
        options_quit_flag = false;
        changing_speed_state = false;
        changing_passes_state = false;
      }
      break;
  }
}

void drawDisplay() {
  display.clearDisplay();
  switch(uiMode){
    case MODE_MENU:
      if(isRunning){
        display.setCursor(0, 0);
        display.println("Done");
        display.print(passesCount);
        display.print("/");
        display.print(Vals.passesVal);
        display.display();
        return;
      }
      display.setCursor(0, 0);
      if(menuCursor==0) display.print("> ");
      display.println("Menu");
      if(menuCursor==1) display.print("> ");
      display.print("Start");
      display.display();
      break;
    case MODE_OPTIONS:
      display.setCursor(0,0);
      if(changing_speed_state){
        display.println("Speed");
        display.print(abs(Vals.speedVal)/16);
        display.print(" mm/s");
        display.display();
        return;
      }
      if(changing_passes_state){
        display.println("Passes");
        display.print(Vals.passesVal);
        display.display();
        return;
      }
      if((!changing_speed_state)&&(!changing_speed_state)){
        if(menuCursor==0) display.print("> ");
        display.println("Speed");
        if(menuCursor==1) display.print("> ");
        display.print("Passes");
        display.display();
      }
      break;
  } 
}