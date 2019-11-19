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
  printHelp();          // Print the command list.
}

void loop(){
  serial();
}

// ============================= Serial Communication =============================

/** Help function that lists the commands available through serial().
 *  Set the line ending to "No line ending"
 * 
 */
void printHelp(void){
  Serial.println("---- Command list: ----");
  Serial.println(" w -> Move x step_number_yitive");
  Serial.println(" a -> Move z step_number_yitive");
  Serial.println(" d -> Move x negative");
  Serial.println(" s -> Move z negative");  
  Serial.println(" e -> Toggle y"); 
  Serial.println(" q -> Diagonal away from origin"); 
  }

/** Serial communication with the cnc machine for manual control.
 *  Set the line ending to "No line ending"
 * 
 */
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

/** Serial communication with the cnc machine for manual control.
 *  Set the line ending to "No line ending"
 *  Prints "P" when done
 */
void driver () {
  int n = 2000;
  if (Serial.available() >0){          // Check receive buffer.
    rxChar = Serial.read();            // Save character received. 
    Serial.flush();                    // Clear receive buffer.
  
  switch (rxChar) {
    case 'x':
    case 'X':
      move_x(5000);
      Serial.println("P");
      break;
      
    case 'z':
    case 'Z':
      move_z(5000);
      Serial.println("P");
      break;

    case 'd':
    case 'D':                          // If received  's' or 'S':
      drawFig(0);
      Serial.println("P");
      break;

    default:
      Serial.println("Input is not a command!");
      Serial.println("P");
    }
  }
}

// ============================= Smart Movement Functions =============================

/** To calibrate, set pen on the writting position point (*** test required)
 * 
 */
void calibrate(int pageSize) {
  OneStepY();
  current_x = 0;
  current_y = 0;
  current_z = 0;

  max_x = pageSize;
  max_z = pageSize;
}

/** Move in x, checks if within drawing space, updates location of the head 
 * 
 */
void move_x (int target) {
  int dx = current_x-target;
  bool dir = false;
  if (dx < 0) {
    dir = true;
  }
  int target = current_x+dx;
  if (target<0 & target>max_x) {
    Serial.println("Ilegal movement");
  } else {
    current_x += dx;
    for (int i = 0; i<abs(dx); i++){
      OneStepX(dir);
      delay(2);
    }
    Serial.println(current_x);
  }
}

/** Move in z, checks if within drawing space, updates location of the head 
 * 
 */
void move_z (int target) {
  int dz = current_z-target;
  bool dir = false;
  if (dz < 0) {
    dir = true;
  }
  int target = current_z+dz;
  if (target<0 & target>max_z) {
    Serial.println("Ilegal movement");
  } else {
    current_z += dz;
    for (int i = 0; i<abs(dz); i++){
      OneStepZ(dir);
      delay(2);
    }
    Serial.println(current_z);
  }
}

/** First symbol 
 *  
 */
void drawLine(int pageSize){
  for (int i = 0; i<pageSize*1000; i++){
    OneStepX(false);
    delay(2);
  }
  OneStepY();     // stop writting
  for (int i = 0; i<pageSize*1000; i++){
    OneStepX(true);
    delay(2);
  }
}

/** Second symbol 
 *  
 */
void drawAngle(int pageSize){
  for (int i = 0; i<pageSize*1000; i++){
    OneStepX(false);
    delay(2);
  }
  for (int i = 0; i<pageSize*1000; i++){
    OneStepZ(false);
    delay(2);
  }
  OneStepY();     // stop writting
  for (int i = 0; i<pageSize*1000; i++){
    OneStepX(true);
    delay(2);
  }
  for (int i = 0; i<pageSize*1000; i++){
    OneStepZ(true);
    delay(2);
  }
}

/** Third symbol 
 *  
 */
void drawTriangle(int pageSize){
  for (int i = 0; i<pageSize*1000; i++){
    OneStepX(false);
    OneStepZ(false);
    delay(2);
  }
  for (int i = 0; i<pageSize*1000; i++){
    OneStepX(false);
    OneStepZ(true);
    delay(2);
  }
  for (int i = 0; i<pageSize*2000; i++){
    OneStepX(true);
    delay(2);
  }
  OneStepY();     // stop writting
}

/** Choose a figure to draw
 * 
 */
void drawFig(int fig, int pageSize) {
  OneStepY();     // start writting
  switch(fig):
    case 0:
      drawLine(pageSize);
      break;
    case 1:
      drawAngle(pageSize);
      break;
    case 2:
      drawTriangle(pageSize);
      break;
    default:
      OneStepY();     // stop writting (dot)
      break; 
}

// ============================= Engine Functions =============================

/** Demo function that moves both X and Z axys at the same time
*/
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
