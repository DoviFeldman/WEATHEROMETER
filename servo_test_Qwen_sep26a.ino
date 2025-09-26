#include <ESP32Servo.h>

// written with Qwen coder on Sep 26 to test out and figure out the placement of all the temp numers and icons on the grandmas version but can also be used for any version and i will need to use this tool for all of them.
// there are 8 temp numbers and 6 icons, so i divided 180 by 7 and by 5 cause that includes 0 so it gives us 8 numbers and 6 numbers. (i rounded the temp numbers, and these are the ones i used)
// here are the numbers to input for the grandmas weatherometer:
// for the Temp Numbers 180, 154, 128, 103, 77, 51, 26, 0
// for the Icons 0 , 36 , 72 , 108 , 144 , 180
// Temp Numbers go on bottom cause theres more room and Icons go on top cause theres less of them. 


Servo tempServo;
Servo iconServo;

void setup() {
  Serial.begin(115200);
  
  // Attach servos to their respective pins
  tempServo.attach(13);
  iconServo.attach(14);
  
  Serial.println("Servo Controller Ready");
  Serial.println("Commands: 'temp <angle>' or 'icon <angle>' (0-180 degrees)");
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    // Find the space to separate command and angle
    int spaceIndex = input.indexOf(' ');
    if (spaceIndex == -1) {
      Serial.println("Invalid format. Use: 'servo_name angle'");
      return;
    }
    
    String servoName = input.substring(0, spaceIndex);
    String angleStr = input.substring(spaceIndex + 1);
    int angle = angleStr.toInt();
    
    // Validate angle range
    if (angle < 0 || angle > 180) {
      Serial.println("Angle must be between 0 and 180");
      return;
    }
    
    // Control the appropriate servo
    if (servoName == "temp") {
      tempServo.write(angle);
      Serial.print("Temp servo set to: ");
      Serial.println(angle);
    }
    else if (servoName == "icon") {
      iconServo.write(angle);
      Serial.print("Icon servo set to: ");
      Serial.println(angle);
    }
    else {
      Serial.println("Unknown servo. Use 'temp' or 'icon'");
    }
  }
}
