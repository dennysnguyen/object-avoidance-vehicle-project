#include <NewPing.h>
#include <Servo.h>

//motor definitons
#define SPEED 70 //PWM value 0 to 255
#define TURN_SPEED 85 //PWM value 0 to 255

//right side motors
#define Speed_BK1_Socket 11  //BK1 (front right PWM)
#define Chnl_B_IN1 5 //BK1 direction IN1
#define Chnl_B_IN2 6 //BK1 direction IN2

#define Speed_AK1_Socket 9 //AK1 (rear right PWM)
#define Chnl_A_IN1 22 //AK1 direction IN1
#define Chnl_A_IN2 24 //AK1 direction IN2

//left side motors
#define Speed_BK3_Socket 10  //BK3 (front left PWM)
#define Chnl_B_IN3 26 //BK3 direction IN1
#define Chnl_B_IN4 28 //BK3 direction IN2

#define Speed_AK3_Socket 12 //AK3 (rear left PWM)
#define Chnl_A_IN3 7 //AK3 direction IN3
#define Chnl_A_IN4 8 //AK3 direction IN4

//sensor definitions
#define SERVO_PIN 13
#define TRIG_PIN 30
#define ECHO_PIN 31
#define MAX_DISTANCE 200 //cm
#define STOP_DISTANCE 20 //cm
#define CENTER_ANGLE 100

Servo ultrasonicServo;
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

int current_angle = CENTER_ANGLE; //tracks the servo's current position
int sweep_direction = 1; //1 for sweeping right -1 for sweeping left
const int SWEEP_STEP = 9; //sweep speed
const int ANGLE_MIN = 10; //start range
const int ANGLE_MAX = 170; //end range

//state variables
long best_clearance = 0;
int best_path_angle = CENTER_ANGLE; 

//function protoypes
void BK1_fwd(int speed);
void BK1_bck(int speed);
void BK3_fwd(int speed);
void BK3_bck(int speed);
void AK1_fwd(int speed);
void AK1_bck(int speed);
void AK3_fwd(int speed);
void AK3_bck(int speed);
void stop_motor();
void move(char direction, int speed);
void goForward(int speed);
void goBackward(int speed);
void pivotLeft(int speed);
void pivotRight(int speed);
long measureDistance();
void handleAvoidance();
void init_GPIO();


//movement primitives

//Front Right Wheel (BK1)
void BK1_fwd(int speed) { digitalWrite(Chnl_B_IN1, HIGH); digitalWrite(Chnl_B_IN2, LOW); analogWrite(Speed_BK1_Socket, speed); }
void BK1_bck(int speed) { digitalWrite(Chnl_B_IN1, LOW); digitalWrite(Chnl_B_IN2, HIGH); analogWrite(Speed_BK1_Socket, speed); }

//Front Left Wheel (BK3)
void BK3_fwd(int speed) { digitalWrite(Chnl_B_IN3, HIGH); digitalWrite(Chnl_B_IN4, LOW); analogWrite(Speed_BK3_Socket, speed); }
void BK3_bck(int speed) { digitalWrite(Chnl_B_IN3, LOW); digitalWrite(Chnl_B_IN4, HIGH); analogWrite(Speed_BK3_Socket, speed); }

//Rear Right Wheel (AK1)
void AK1_fwd(int speed) { digitalWrite(Chnl_A_IN1, HIGH); digitalWrite(Chnl_A_IN2, LOW); analogWrite(Speed_AK1_Socket, speed); }
void AK1_bck(int speed) { digitalWrite(Chnl_A_IN1, LOW); digitalWrite(Chnl_A_IN2, HIGH); analogWrite(Speed_AK1_Socket, speed); }

//Rear Left Wheel (AK3)
void AK3_fwd(int speed) { digitalWrite(Chnl_A_IN3, HIGH); digitalWrite(Chnl_A_IN4, LOW); analogWrite(Speed_AK3_Socket, speed); }
void AK3_bck(int speed) { digitalWrite(Chnl_A_IN3, LOW); digitalWrite(Chnl_A_IN4, HIGH); analogWrite(Speed_AK3_Socket, speed); }

//motor control functions

void stop_motor() 
{ //stop all motors immediately
  analogWrite(Speed_AK3_Socket, 0);
  analogWrite(Speed_AK1_Socket, 0);
  analogWrite(Speed_BK3_Socket, 0);
  analogWrite(Speed_BK1_Socket, 0);
}

//moves all 4 wheels
void move(char direction, int speed) 
{
  if (direction == 'F') 
  {
    BK1_fwd(speed); AK1_fwd(speed); //right side Forward
    BK3_fwd(speed); AK3_fwd(speed); //left side Forward
  } 
  else if (direction == 'B') 
  {
    BK1_bck(speed); AK1_bck(speed); //right side Backward
    BK3_bck(speed); AK3_bck(speed); //left side Backward
  }
}

void goForward(int speed = SPEED) { move('F', speed); }
void goBackward(int speed = SPEED) { move('B', speed); }

//pivot turn left
void pivotLeft(int speed = TURN_SPEED) 
{
  BK1_fwd(speed); AK1_fwd(speed);
  BK3_bck(speed); AK3_bck(speed);
}

//pivot turn right
void pivotRight(int speed = TURN_SPEED) 
{
  BK1_bck(speed); AK1_bck(speed);
  BK3_fwd(speed); AK3_fwd(speed);
}

//ultrasonic sensor and servo functions

//reads the distance from the ultrasonic sensor in cm
long measureDistance() 
{
  long distance = sonar.ping_cm();
  if (distance == 0) return MAX_DISTANCE;
  return distance;
}

//decision and turning logic based on the continuous sweep data
void handleAvoidance() 
{
  //back up for room for turning
  stop_motor();
  goBackward(SPEED);
  delay(300);
  stop_motor();

  const int LEFT_THRESHOLD = 85;   //angle < 85 means path is clearly left of center
  const int RIGHT_THRESHOLD = 115; //angle > 115 means path is clearly right of center

  if (best_clearance <= STOP_DISTANCE) 
  {
    //everything is blocked
    pivotRight(TURN_SPEED);
    delay(1000);
    stop_motor();
  } 
  else 
  {
    //does turn based on the best path found during the continuous sweep
    //if best path is to the left (angle < 85)
    if (best_path_angle < LEFT_THRESHOLD) 
    { 
      pivotLeft(TURN_SPEED);
      delay(500);
      stop_motor();
    //if best path is to the right (angle > 115)
    } 
    else if (best_path_angle > RIGHT_THRESHOLD) 
    { 
      Serial.println("Turning Right (Path to the Right)");
      pivotRight(TURN_SPEED);
      delay(500); 
      stop_motor();
    //if best path is straight ahead (between 85 to 115)
    } 
    else 
    {
      pivotRight(TURN_SPEED); //does a small turn to break any symmetry
      delay(200);
      stop_motor();
    }
  }

  //scan one more time before moving
  ultrasonicServo.write(CENTER_ANGLE); //center the sensor
  delay(100);
  
  long post_turn_distance = measureDistance();
  
  //if the new path is still blocked we just re-evaluation
  if (post_turn_distance > 0 && post_turn_distance <= STOP_DISTANCE) 
  {
      goBackward(SPEED);
      delay(300);
      stop_motor();
  }

  //reset best path variables and flag
  best_clearance = 0;
  best_path_angle = CENTER_ANGLE;
}

//pins initialize
void init_GPIO() 
{
  //motor direction pins
  pinMode(Chnl_A_IN1, OUTPUT); pinMode(Chnl_A_IN2, OUTPUT);
  pinMode(Chnl_A_IN3, OUTPUT); pinMode(Chnl_A_IN4, OUTPUT);
  pinMode(Chnl_B_IN1, OUTPUT); pinMode(Chnl_B_IN2, OUTPUT);
  pinMode(Chnl_B_IN3, OUTPUT); pinMode(Chnl_B_IN4, OUTPUT);
  //motor speed (PWM) pins
  pinMode(Speed_AK1_Socket, OUTPUT); pinMode(Speed_AK3_Socket, OUTPUT);
  pinMode(Speed_BK1_Socket, OUTPUT); pinMode(Speed_BK3_Socket, OUTPUT);
  stop_motor();

  //servo setup
  ultrasonicServo.attach(SERVO_PIN);
  ultrasonicServo.write(CENTER_ANGLE);

  current_angle = CENTER_ANGLE;
  sweep_direction = 1;
  best_clearance = MAX_DISTANCE;
  best_path_angle = CENTER_ANGLE;

  //debugging
  //Serial.begin(9600);
}

void setup() 
{
  init_GPIO();
  //Serial.println("autonomous car initialized");
}

void loop() 
{
  //have continuous servo sweep and distance measuring
  
  //update servo angle
  current_angle += (sweep_direction * SWEEP_STEP);
  
  // check boundary conditions and reverse direction if needed
  if (current_angle >= ANGLE_MAX) 
  {
    current_angle = ANGLE_MAX;
    sweep_direction = -1; 
    
  } 
  else if (current_angle <= ANGLE_MIN) 
  {
    current_angle = ANGLE_MIN;
    sweep_direction = 1; 
    
    // Reset best path variables flags
    best_clearance = 0;
    best_path_angle = CENTER_ANGLE;
  }

  //move the servo
  ultrasonicServo.write(current_angle);

  //take distance at the current angle
  long distance_cm = measureDistance();

  //update bet path if the current spot is clearer
  if (distance_cm > best_clearance) {
    best_clearance = distance_cm;
    best_path_angle = current_angle;
  }
  
  //check for obstacle threshold
  if (distance_cm > 0 && distance_cm <= STOP_DISTANCE) {
    // obstaclee detected and stop and turn immediately.
    handleAvoidance();
    
  } 
  else 
  {
    //roam forward
    goForward(SPEED);
  }
  
  delay(50); 
}