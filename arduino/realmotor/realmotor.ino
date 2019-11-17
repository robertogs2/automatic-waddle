#include <Servo.h> 

// X axys - base
#define STEPPER_PIN_X1 9
#define STEPPER_PIN_X2 10
#define STEPPER_PIN_X3 11
#define STEPPER_PIN_X4 12
int current_x = 0;
int max_x = 10000;

// Z axys - bridge
#define STEPPER_PIN_Z1 5
#define STEPPER_PIN_Z2 6
#define STEPPER_PIN_Z3 7
#define STEPPER_PIN_Z4 8
int current_z = 0;
int max_z = 10000;

// Y axys - pilot
#define SERVO_PIN_Y1 3
int current_y = 0;
Servo myservo;

// Stepper counters
int step_number_x = 0;
int step_number_y = 0;
int step_number_z = 0;



// UART communication
char rxChar= 0;         // RXcHAR holds the received command.

//=== function to print the command list:  ===========================
void printHelp(void){
  Serial.println("---- Command list: ----");
  Serial.println(" w -> Move x step_number_yitive");
  Serial.println(" a -> Move z step_number_yitive");
  Serial.println(" d -> Move x negative");
  Serial.println(" s -> Move z negative");  
  Serial.println(" e -> Toggle y");  
  }

void setup() {
  pinMode(STEPPER_PIN_X1, OUTPUT);
  pinMode(STEPPER_PIN_X2, OUTPUT);
  pinMode(STEPPER_PIN_X3, OUTPUT);
  pinMode(STEPPER_PIN_X4, OUTPUT);
  pinMode(STEPPER_PIN_Z1, OUTPUT);
  pinMode(STEPPER_PIN_Z2, OUTPUT);
  pinMode(STEPPER_PIN_Z3, OUTPUT);
  pinMode(STEPPER_PIN_Z4, OUTPUT);
  myservo.attach(SERVO_PIN_Y1);
  
  Serial.begin(9600);   // Open serial port (9600 bauds).
  Serial.flush();       // Clear receive buffer.
  printHelp();          // Print the command list.
}

void loop(){
  serial();
}

void serial () {
  int n = 2000;
  if (Serial.available() >0){          // Check receive buffer.
    rxChar = Serial.read();            // Save character received. 
    Serial.flush();                    // Clear receive buffer.
  
  switch (rxChar) {
    case 'w':
    case 'W':
      for (int i = 0; i<n; i++){
        OneStepX(false);
        delay(2);
      }
      Serial.println("x++");
      break;
      
    case 'a':
    case 'A':
      for (int i = 0; i<n; i++){
        OneStepZ(true);
        delay(2);
      }
      Serial.println("z++");
      break;

    case 's':
    case 'S':                          // If received  's' or 'S':
      for (int i = 0; i<n; i++){
        OneStepX(true);
        delay(2);
      }
      Serial.println("x--");
      break;

    case 'd':
    case 'D':                          // If received 'd' or 'D':
      for (int i = 0; i<n; i++){
        OneStepZ(false);
        delay(2);
      }
      Serial.println("z--");
      break;

    case 'e':
    case 'E':                          // If received 'e' or 'E':
      OneStepY();
      delay(2);
      Serial.println("¬y");
      break;

    case 'q':
    case 'Q':                          // If received 'e' or 'E':
    for (int i = 0; i<n; i++){
      OneStepX(false);
      OneStepZ(false);
      delay(2);
    }
      Serial.println("q--");
      break;

    default:
      Serial.println("Input is not a command!");
    }
  }
}

void move_x (int steps) {
  int dx = current_x-steps;
  bool dir = true;
  if (dx > 0) {
    dir = true;
  } else {
    dir = false;
    false;
  }
  for (int i = 0; i<abs(dx); i++){
    OneStepX(dir);
    delay(2);
  }
}

void demo() {
  for (int i = 0; i<4000; i++){
    OneStepX(true);
    OneStepZ(false);
    delay(2);
  }
  for (int i = 0; i<4000; i++){
    OneStepX(false);
    OneStepZ(true);
    delay(2);
  }
}


void OneStepX(bool dir){
  if(dir){
    switch(step_number_x ){
      case 0:
      digitalWrite(STEPPER_PIN_X1, HIGH);
      digitalWrite(STEPPER_PIN_X2, LOW);
      digitalWrite(STEPPER_PIN_X3, LOW);
      digitalWrite(STEPPER_PIN_X4, LOW);
      break;
      case 1:
      digitalWrite(STEPPER_PIN_X1, LOW);
      digitalWrite(STEPPER_PIN_X2, HIGH);
      digitalWrite(STEPPER_PIN_X3, LOW);
      digitalWrite(STEPPER_PIN_X4, LOW);
      break;
      case 2:
      digitalWrite(STEPPER_PIN_X1, LOW);
      digitalWrite(STEPPER_PIN_X2, LOW);
      digitalWrite(STEPPER_PIN_X3, HIGH);
      digitalWrite(STEPPER_PIN_X4, LOW);
      break;
      case 3:
      digitalWrite(STEPPER_PIN_X1, LOW);
      digitalWrite(STEPPER_PIN_X2, LOW);
      digitalWrite(STEPPER_PIN_X3, LOW);
      digitalWrite(STEPPER_PIN_X4, HIGH);
      break;
    } 
  }else{
    switch(step_number_x){
      case 0:
      digitalWrite(STEPPER_PIN_X1, LOW);
      digitalWrite(STEPPER_PIN_X2, LOW);
      digitalWrite(STEPPER_PIN_X3, LOW);
      digitalWrite(STEPPER_PIN_X4, HIGH);
      break;
      case 1:
      digitalWrite(STEPPER_PIN_X1, LOW);
      digitalWrite(STEPPER_PIN_X2, LOW);
      digitalWrite(STEPPER_PIN_X3, HIGH);
      digitalWrite(STEPPER_PIN_X4, LOW);
      break;
      case 2:
      digitalWrite(STEPPER_PIN_X1, LOW);
      digitalWrite(STEPPER_PIN_X2, HIGH);
      digitalWrite(STEPPER_PIN_X3, LOW);
      digitalWrite(STEPPER_PIN_X4, LOW);
      break;
      case 3:
      digitalWrite(STEPPER_PIN_X1, HIGH);
      digitalWrite(STEPPER_PIN_X2, LOW);
      digitalWrite(STEPPER_PIN_X3, LOW);
      digitalWrite(STEPPER_PIN_X4, LOW);
    }
  }
  step_number_x++;
    if(step_number_x > 3){
      step_number_x = 0;
    }
}

void OneStepZ(bool dir){
  if(dir){
    switch(step_number_z){
      case 0:
      digitalWrite(STEPPER_PIN_Z1, HIGH);
      digitalWrite(STEPPER_PIN_Z2, LOW);
      digitalWrite(STEPPER_PIN_Z3, LOW);
      digitalWrite(STEPPER_PIN_Z4, LOW);
      break;
      case 1:
      digitalWrite(STEPPER_PIN_Z1, LOW);
      digitalWrite(STEPPER_PIN_Z2, HIGH);
      digitalWrite(STEPPER_PIN_Z3, LOW);
      digitalWrite(STEPPER_PIN_Z4, LOW);
      break;
      case 2:
      digitalWrite(STEPPER_PIN_Z1, LOW);
      digitalWrite(STEPPER_PIN_Z2, LOW);
      digitalWrite(STEPPER_PIN_Z3, HIGH);
      digitalWrite(STEPPER_PIN_Z4, LOW);
      break;
      case 3:
      digitalWrite(STEPPER_PIN_Z1, LOW);
      digitalWrite(STEPPER_PIN_Z2, LOW);
      digitalWrite(STEPPER_PIN_Z3, LOW);
      digitalWrite(STEPPER_PIN_Z4, HIGH);
      break;
    } 
  }else{
    switch(step_number_z){
      case 0:
      digitalWrite(STEPPER_PIN_Z1, LOW);
      digitalWrite(STEPPER_PIN_Z2, LOW);
      digitalWrite(STEPPER_PIN_Z3, LOW);
      digitalWrite(STEPPER_PIN_Z4, HIGH);
      break;
      case 1:
      digitalWrite(STEPPER_PIN_Z1, LOW);
      digitalWrite(STEPPER_PIN_Z2, LOW);
      digitalWrite(STEPPER_PIN_Z3, HIGH);
      digitalWrite(STEPPER_PIN_Z4, LOW);
      break;
      case 2:
      digitalWrite(STEPPER_PIN_Z1, LOW);
      digitalWrite(STEPPER_PIN_Z2, HIGH);
      digitalWrite(STEPPER_PIN_Z3, LOW);
      digitalWrite(STEPPER_PIN_Z4, LOW);
      break;
      case 3:
      digitalWrite(STEPPER_PIN_Z1, HIGH);
      digitalWrite(STEPPER_PIN_Z2, LOW);
      digitalWrite(STEPPER_PIN_Z3, LOW);
      digitalWrite(STEPPER_PIN_Z4, LOW);
    } 
  }
  step_number_z++;
    if(step_number_z > 3){
      step_number_z = 0;
    }
}

void OneStepY () {
  if (current_y){
      myservo.write(0);              // tell servo to go to step_number_yition in variable 'step_number_y'
      delay(15);                       // waits 15ms for the servo to reach the step_number_yition
      current_y = 0;
  } else {
    myservo.write(180);              // tell servo to go to step_number_yition in variable 'step_number_y'
    delay(15);                       // waits 15ms for the servo to reach the step_number_yition
    current_y = 1;
    }
  }
