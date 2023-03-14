# Crane
Embedded systems
Code for the Multidisciplinary Project. A file containing the crane program and another file containing the joystick program

The joystick file takes care of the logic. It constantly takes input from the user (which motor to turn, which direction, which speed,
put in automatic or manual mode, etc...) and then it puts all the information in the form of a string which will be sent by WiFi module to the PCB on the crane.
 
The crane PCB receives the string and then it splits the information and then it sends the respective current to the respective pin to activate the motors
