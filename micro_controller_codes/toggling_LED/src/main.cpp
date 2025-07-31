// connect one LED for each pin 5, 18, 19, 21 and add resistors connecting to GND
// add push button connected to pin 34 and the other side is to connected to GND
#include <Arduino.h>

const int led_pins[4] = {5, 18, 19, 21};
const int button_pin = 34;

bool led_state = HIGH;         
bool last_button_state = HIGH;   

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    pinMode(led_pins[i], OUTPUT);
    digitalWrite(led_pins[i], LOW); 
  }

  pinMode(button_pin, INPUT_PULLUP);  
}

void loop() {
  bool current_button_state = digitalRead(button_pin);

  if(last_button_state == HIGH && current_button_state == LOW){
    led_state = !led_state;
    Serial.println(led_state ? "ON" : "OFF");
  }
  
  for(int i = 0; i < 4; i++){
      digitalWrite(led_pins[i], led_state);
    }

  last_button_state = current_button_state;
  delay(10);
}
