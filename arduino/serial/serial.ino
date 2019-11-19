void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);

}

void loop() {
  String s = "";
  
  while(true){
    if(Serial.available()){
      char c = Serial.read();
      if(c == '\n') break;
      s+=c;
    }
  }
  if(s == "9x"){
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
       delay(1000);  
  }
  else if(s == "8y"){
                         // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(1000);    
  }
}

