#include <Servo.h> 

// X axys - base
#define STEPPER_PIN_X1 9
#define STEPPER_PIN_X2 10
#define STEPPER_PIN_X3 11
#define STEPPER_PIN_X4 12
int current_x = 0;
int max_x = 12000;

// Z axys - bridge
#define STEPPER_PIN_Z1 5
#define STEPPER_PIN_Z2 6
#define STEPPER_PIN_Z3 7
#define STEPPER_PIN_Z4 8
int current_z = 0;
int max_z = 16000;

// Y axys - pilot
#define SERVO_PIN_Y1 3
int current_y = 0;
Servo myservo;

// Stepper counters
int step_number_x = 0;
int step_number_y = 0;
int step_number_z = 0;

// UART communication
char rxChar = 0;         // RXcHAR holds the received command.
String command;
int pageSize = 0;


// m 0, 1, 2  
//   3, 4, 5 
//   6, 7, 8 
// Coords           {l,     m,     s}
int square_x [27] = { 2000, 2000, 1500,
                      2000, 2000, 1500,
                      2000, 2000, 1500,
                      5500, 4500, 3000,
                      5500, 4500, 3000,
                      5500, 4500, 3000,
                      9500, 6500, 4500,
                      9500, 6500, 4500,
                      9500, 6500, 4500};

int square_z [27] = {10000, 8500, 6000,
                      7000, 6500, 4500,
                      3000, 4000, 3000,
                     10000, 8500, 6000, 
                      7000, 6500, 4500,
                      3000, 4000, 3000,
                     10000, 8500, 6000,
                      7000, 6500, 4500,
                      3000, 4000, 3000};

int get_square_x(int row, int col){
  return square_x[row*3+col];
}

int get_square_z(int row, int col){
  return square_z[row*3+col];
}

// ============================= Forward Declarations =============================
void resetY ();
void OneStepX(bool dir);
void OneStepY();
void OneStepZ(bool dir);

// ============================= Smart Movement Functions =============================

/** To calibrate, set pen on the writting position point (*** test required)
 * 
 */
void calibrate(int pageSize) {
  current_x = 0;
  current_z = 0;
  resetY();
}

void reset () {
  move_x(0);
  move_z(0);
  for (int i = 0; i<2000; i++){ //Extra in case it gets stuck
    OneStepX(true);
    OneStepZ(true);
    delay(2);
  }
  resetY();
}

/** Move in x, checks if within drawing space, updates location of the head 
 * 
 */
void move_x (int target) {
  int dx = target - current_x;
  if (dx < 0) {
    for (int i = 0; i<abs(dx); i++){
        OneStepX(true);
        delay(2);
      }
  } else {
    for (int i = 0; i<abs(dx); i++){
        OneStepX(false);
        delay(2);
      }
  }
  current_x = target;
  //Serial.println(current_x);
}

/** Move in z, checks if within drawing space, updates location of the head 
 * 
 */
void move_z (int target) {
  int dz = target - current_z;
  if (dz < 0) {
    for (int i = 0; i<abs(dz); i++){
        OneStepZ(true);
        delay(2);
      }
  } else {
    for (int i = 0; i<abs(dz); i++){
        OneStepZ(false);
        delay(2);
      }
  }
    current_z = target;
    //Serial.println(current_z);
  }

int getLineSize () {
  //int lineSize = 3000;
  if (pageSize == 1) {
    return 1830; // lineSize*0.61;
  } else  if (pageSize == 2) {
    return 1200; //lineSize*0.35;
  } else {
    return 3000; //lineSize;
  }
}
/** First symbol 
 *  
 */
void drawLine(){
  //Serial.println("Drawing line");
  int lineSize = getLineSize();
  for (int i = 0; i<lineSize-750; i++){  // Center the line
    OneStepZ(false);
    delay(2);
  }
  OneStepY();     // start writting
  for (int i = 0; i<lineSize; i++){
    OneStepX(false);
    delay(2);
  }
  OneStepY();     // stop writting
  for (int i = 0; i<lineSize; i++){
    OneStepX(true);
    delay(2);
  }
    for (int i = 0; i<lineSize-750; i++){
    OneStepZ(true);
    delay(2);
  }
}

/** Second symbol 
 *  
 */
void drawAngle(){
  OneStepY();     // start writting
  //Serial.println("Drawing angle");
  int lineSize = getLineSize();
  for (int i = 0; i<lineSize; i++){
    OneStepX(false);
    delay(2);
  }
  for (int i = 0; i<lineSize-500; i++){
    OneStepZ(false);
    delay(2);
  }
  OneStepY();     // stop writting
  for (int i = 0; i<lineSize; i++){
    OneStepX(true);
    delay(2);
  }
  for (int i = 0; i<lineSize-500; i++){
    OneStepZ(true);
    delay(2);
  }
}

/** Third symbol 
 *  
 */
void drawTriangle(){
  OneStepY();     // start writting
  //Serial.println("Drawing triangle");
  int lineSize = getLineSize();
  for (int i = 0; i<lineSize; i++){
    OneStepX(false);
    OneStepZ(false);
    delay(2);
  }
  for (int i = 0; i<lineSize; i++){
    OneStepZ(true);
    delay(2);
  }
  for (int i = 0; i<lineSize; i++){
    OneStepX(true);
    delay(2);
  }
  OneStepY();     // stop writting
}

void drawStraight (bool dir, int lineSize) {
  if (dir) {  // vertical (x)
    for (int i = 0; i<lineSize-750; i++){  // Center the line
      OneStepZ(false);
      delay(2);
    }
    OneStepY();     // start writting
    for (int i = 0; i<lineSize*4; i++){
      OneStepX(false);
      delay(2);
    }
    OneStepY();     // stop writting
  } else { // horizontal (dir == 0, z)
    for (int i = 0; i<lineSize-750; i++){  // Center the line
      OneStepX(false);
      delay(2);
    }
    OneStepY();     // start writting
    for (int i = 0; i<lineSize*4; i++){
      OneStepZ(false);
      delay(2);
    }
    OneStepY();     // stop writting
  }
}

void drawWinner(int winLine){
  int target = getLineSize();
  int new_x = 0;
  int new_z = 0;
  if (winLine == 6) {    // diagonal
    new_x = get_square_x(0, pageSize);
    new_z = get_square_z(0, pageSize);
    move_x(new_x);
    move_z(new_z);
    current_x = new_x;
    current_z = new_z;
    for (int i = 0; i<target; i++){
      OneStepZ(false);
      delay(2);
    }
    OneStepY();     // start writting
    for (int i = 0; i<target*4; i++){
      OneStepX(false);
      OneStepZ(true);
      current_x += target;
      current_z += target;
      delay(2);
    }
    OneStepY();     // stop writting
  } else if (winLine == 7) {    // diagonal
      new_x = get_square_x(2, pageSize);
      new_z = get_square_z(2, pageSize);
      move_x(new_x);
      move_z(new_z);    
      current_x = new_x;
      current_z = new_z;
      OneStepY();     // start writting
      for (int i = 0; i<target*4; i++){
        OneStepX(false);
        OneStepZ(false);
        current_x += target;
        current_z += target;
        delay(2);
      }
      OneStepY();     // stop writting
    } else {    // straights
      switch (winLine) {
        case 0: 
          new_x = get_square_x(2, pageSize);
          new_z = get_square_z(2, pageSize);
          move_x(new_x);
          move_z(new_z);
          current_x = new_x;
          current_z = new_z;
          drawStraight(0, target);
          break;
        case 1: 
          new_x = get_square_x(5, pageSize);
          new_z = get_square_z(5, pageSize);
          move_x(new_x);
          move_z(new_z);
          current_x = new_x;
          current_z = new_z;          
          drawStraight(0, target);
          break;
        case 2: 
          new_x = get_square_x(8, pageSize);
          new_z = get_square_z(8, pageSize);
          move_x(new_x);
          move_z(new_z);
          current_x = new_x;
          current_z = new_z;          
          drawStraight(0, target);
          break;
        case 3: 
          new_x = get_square_x(0, pageSize);
          new_z = get_square_z(0, pageSize);
          move_x(new_x);
          move_z(new_z);
          current_x = new_x;
          current_z = new_z;          
          drawStraight(1, target);
          break;
        case 4: 
          new_x = get_square_x(1, pageSize);
          new_z = get_square_z(1, pageSize);
          move_x(new_x);
          move_z(new_z);
          current_x = new_x;
          current_z = new_z;          
          drawStraight(1, target);
          break;
        case 5: 
          new_x = get_square_x(2, pageSize);
          new_z = get_square_z(2, pageSize);
          move_x(new_x);
          move_z(new_z);
          current_x = new_x;
          current_z = new_z;          
          drawStraight(1, target);
          break;
        default:
          break;
      }
    }
  }


/** Choose a figure to draw
 * 
 */
void drawFig(int fig) {
  switch(fig) {
    case 0:
      drawLine();
      break;
    case 1:
      drawAngle();
      break;
    case 2:
      drawTriangle();
      break;
    default:
      OneStepY();     // stop writting (dot)
      break;
  }
   resetY();
   //Serial.println("A");
}

// ============================= Engine Functions =============================
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

/** One step for the X-axys. Add to a for loop ~500 iterations
 *  
 *  bool dir: direction of movement of the motor
 */
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

/** One step for the Z-axys. Add to a for loop ~500 iterations
 *  
 *  bool dir: direction of movement of the motor
 */
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

/** One step for the Y-axys. Toggles the position of the writer head
 */
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

void resetY () {
    myservo.write(0);              // tell servo to go to step_number_yition in variable 'step_number_y'
    delay(15);                       // waits 15ms for the servo to reach the step_number_yition
    current_y = 0;
  }

// ============================= Serial Communication =============================
void split(String input, char delimiter, String* results){
 int t = 0;
 int r = 0;
 int i = 0;
 for (i=0; i < input.length(); i++){
   if(input.charAt(i) == delimiter && input.charAt(i) != '\n') {
     results[t] = input.substring(r, i);
     r=(i+1);
     t++;
   }
 } 
 results[t] = input.substring(r, i);
}

void analize_command(String command){
  if (command=="s0") {
    max_x = 12000;
    max_z = 16000;
    pageSize = 0;
    //Serial.println("A");
  } else if (command=="s1") {
    max_x = 12000;
    max_z = 8000;
    pageSize = 1;
    //Serial.println("A");
  }
  else if (command=="s2") {
    max_x = 6000;
    max_z = 8000;
    pageSize = 2;
    //Serial.println("A");
  } else if (command=="c") {
    OneStepY();
    //Serial.println("A");
  } else if (command=="r") {
    reset();
    //Serial.println("A");
  } else if (command.charAt(0)=='w'){
    //Serial.println("A");
    int winLine = command.charAt(1) - '0';
    if (winLine >= 0 && winLine <= 7) {
      drawWinner(winLine);
      //Serial.println("A");
    }
  } else {
    int symbol = command.charAt(0) - '0';
    int square = command.charAt(1) - '0';

    if (symbol >= 0 && symbol <= 2) {
      if (square >= 0 && square <= 8) {
      int new_x = get_square_x(square, pageSize);
      int new_z = get_square_z(square, pageSize);
      //Serial.println(symbol);
      //Serial.println(square);
      move_x(new_x);
      move_z(new_z);
      drawFig(symbol);
      Serial.println("A");
      }
    }
  }
}

  // ============================= Default Arduino Functions =============================
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
  //printHelp();        // Print the command list.

  resetY();
}

void loop() {

 // put your main code here, to run repeatedly:
  while(true){
    if(Serial.available()){
      char c = Serial.read();
   if(c == '\n' || c == '\r') break;
   command += c;
    }
 }
 if(command.length() > 0){
  Serial.flush();
   analize_command(command);
   //Serial.println(command);
 }
 command = "";
}
