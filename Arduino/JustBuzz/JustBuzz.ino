/* Arduino tutorial - Buzzer / Piezo Speaker
   More info and circuit: http://www.ardumotive.com/how-to-use-a-buzzer-en.html
   Dev: Michalis Vasilakis // Date: 9/6/2015 // www.ardumotive.com */


const int buzzer = 4; // which pin to buzz


void setup(){
 
  pinMode(buzzer, OUTPUT); // Set buzzer pin an output

}

void loop(){
 
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(500);
  tone(buzzer, 4000);
  delay(1000);
  noTone(buzzer);     // Stop sound...
  delay(1000);        // ...for 1sec
  
}
