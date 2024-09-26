/* 
 Mini CNC Plotter firmware, based in TinyCNC https://github.com/MakerBlock/TinyCNC-Sketches
 Send GCODE to this Sketch using gctrl.pde https://github.com/damellis/gctrl
 Convert SVG to GCODE with MakerBot Unicorn plugin for Inkscape available here https://github.com/martymcguire/inkscape-unicorn
 
 More information about the Mini CNC Plotter here (german, sorry): http://www.makerblog.at/2015/02/projekt-mini-cnc-plotter-aus-alten-cddvd-laufwerken/
  */

#include <Servo.h>

#define LINE_BUFFER_LENGTH 128  //Đây là độ dài mảng dùng để lưu trữ từng dòng lệnh nhận được qua serial.
#define MIN_STEP_DELAY (200.0)  // us
#define MAX_FEEDRATE (3939)     // mm/minute
#define MIN_FEEDRATE (0.01)
// for arc directions
#define ARC_CW (1)
#define ARC_CCW (-1)
// Arcs are split into many line segments.  How long are the segments?
#define MM_PER_SEGMENT (1)
// Servo position for Up and Down
int penZUp = 30;
int penZDown = 90;
int last_degree; // biến này để lưu giá trị góc tinh chỉnh cho servo
// Servo on PWM pin 10
const int penServoPin = 11;  
const int Xlimit_switch=9;
const int Ylimit_switch=10; 
// Should be right for Nema17 steppers, but is not too important here
const int stepsPerRevolution = 200;  // số bước cần để quay được 1 vòng

// create servo object to control a servo
Servo penServo;

// Initialize steppers for X- and Y-axis using this Arduino pins for the CNC shield V3
#define M1_STEP 2
#define M1_DIR 5

#define M2_STEP 3
#define M2_DIR 6

#define M3_STEP 4
#define M3_DIR 7

#define M123_ENA 8
#define RESUME A4
#define EMERGENCY  A5
#define RESET  13
/* Structures, global variables    */
bool EMERGENCY_FLAG =false;
struct point {
  float x;
  float y;
  float z;
};

// Current position of plothead
struct point currentPos;  // vị trí hiện tại của đầu bút

// Motor steps to go 1 millimeter.
// Use test sketch to go 100 steps. Measure the length of line.
// Calculate steps per mm. Enter here.
#define M_FULLSTEP 1
#define M_HALFSTEP 2
#define M_QUARTERSTEP 4
#define M_ONE16 16
#define M_ONE32 32

#define STEPPER_MODE M_ONE16
float StepsPerMillimeterX = 200 * M_ONE32 / 42;       // chế độ 1/16 step-microstep resolution
float StepsPerMillimeterY = 200 * STEPPER_MODE / 42;  // chế độ 1/16 step-microstep resolution

//  Drawing settings, should be OK
const float StepInc = 1;  // Đây là giá trị gia tăng mỗi bước của motor (stepper motor)
int StepDelay = 100;        //Đây là độ trễ giữa các bước di chuyển của motor, liên quan đến Feedrate
float fr = (int)60 * 1000000.0 / (StepDelay * StepsPerMillimeterX);           //Feedrate- human version
bool mode_abs = true;
bool mode_rel = !mode_abs;

// Drawing robot limits, in mm
// OK to start with. Could go up to 400 mm if calibrated well.
float Xmin =0;
float Xmax =250;  // mm
float Ymin = 0;
float Ymax = 310;
float Zmin = 0;
float Zmax = 1;

float Xpos = Xmin*StepsPerMillimeterX;// đơn vị số bước
float Ypos = Ymin*StepsPerMillimeterY;  
float Zpos = Zmax;
float lastZ=0;

// Set to true to get debug output. Set it once you upload again
boolean verbose = false;
char line[LINE_BUFFER_LENGTH];      // Đây là mảng dùng để lưu trữ từng dòng lệnh nhận được qua serial.
char c;                             // Biến này lưu trữ từng ký tự được đọc từ serial.
int lineIndex;                      // Đây là chỉ số theo dõi vị trí hiện tại trong mảng line,để biết ký tự tiếp theo sẽ được lưu ở đâu.
bool lineIsComment, lineSemiColon;  //Các biến Boolean này được sử dụng để xác định xem dòng hiện tại có phải là dòng chú thích (comment) hay không.

//  Needs to interpret
//  G1 for moving
//  G92 for changing logical position
//  M18 for turning off power to motors
//  G90 : absolute mode  | G91 : relative mode  // absolute or relative coordinate system mode
//  G4 P300 (wait 150ms)
//  M300 S30 (pen down)
//  M300 S50 (pen up)
//  Discard anything with a (
//  Discard any other command!

//================ FUNCTIONS ========================
void test_cnc(String shape);
void penUp(void);
void penDown(void);
void drawLine(float x1, float y1);
void review_Gcode(void);
void m1step(int dir);
void m23step(int dir);
void disable(void);
void feedrate(float nfr);
void wait(long us);
float atan3(float dy, float dx);
void arc(float cx, float cy, float x, float y, float dir);
void processIncomingLine(char* line, int charNB);
void protection(void);
void setup_coordinate(void);
// void draw_border(void);
void home(void);

//===================================================

/**********************
 * void setup() - Initialisations
 ***********************/
void setup() {
  //  Setup
  Serial.begin(9600);
  pinMode(M123_ENA, OUTPUT);
  pinMode(M1_STEP, OUTPUT);
  pinMode(M2_STEP, OUTPUT);
  pinMode(M3_STEP, OUTPUT);
  pinMode(M1_DIR, OUTPUT);
  pinMode(M2_DIR, OUTPUT);
  pinMode(M3_DIR, OUTPUT);
  pinMode(Xlimit_switch,INPUT_PULLUP);
  pinMode(Ylimit_switch,INPUT_PULLUP);
  pinMode(RESUME,INPUT);    // nút tạm dừng/tiếp tục, led sáng tiếp tục, led tắt tạm dừng
  pinMode(EMERGENCY,INPUT);// nút dừng khẩn cấp
  pinMode(RESET,INPUT_PULLUP);
  penServo.attach(penServoPin);
  penServo.write(penZUp);

  setup_coordinate();
  // home(); // về gốc toạ đô- comeback to original coordinate
  disable();
  // draw_border();
  delay(2000);

  // Set The origin of the coordinate system is the initial point.
  // currentPos.x = 0;
  // currentPos.y = 0;
  //  Notifications!!!
  Serial.println("CNC Plotter alive and kicking!");
  Serial.print("X range is from ");
  Serial.print(Xmin);
  Serial.print(" to ");
  Serial.print(Xmax);
  Serial.println(" mm.");
  Serial.print("Y range is from ");
  Serial.print(Ymin);
  Serial.print(" to ");
  Serial.print(Ymax);
  Serial.println(" mm.");
  Serial.println("ok");
}

/**********************
 * void loop() - Main loop
 ***********************/

void loop() {
  // char line[ LINE_BUFFER_LENGTH ];// Đây là mảng dùng để lưu trữ từng dòng lệnh nhận được qua serial.
  // char c;// Biến này lưu trữ từng ký tự được đọc từ serial.
  // int lineIndex;// Đây là chỉ số theo dõi vị trí hiện tại trong mảng line,để biết ký tự tiếp theo sẽ được lưu ở đâu.
  // bool lineIsComment, lineSemiColon;//Các biến Boolean này được sử dụng để xác định xem dòng hiện tại có phải là dòng chú thích (comment) hay không.

  lineIndex = 0;
  lineSemiColon = false;
  lineIsComment = false;
  //Vòng lặp này thực hiện việc nhận và xử lý lệnh từ serial một cách liên tục.
  while (1) {
    // Vòng lặp này kiểm tra xem có dữ liệu nào đang chờ trong bộ đệm serial hay không.
    // Nếu có, chương trình sẽ đọc từng ký tự một từ serial và xử lý.
    // Serial reception - Mostly from Grbl, added semicolon support
    if(digitalRead(RESET)==0) {
      penUp();
      setup_coordinate();
      disable();
    }
    while (Serial.available() > 0 ) {
      if(digitalRead(RESET)==0) {
        penUp();
        setup_coordinate();
        disable();
      }
      // if(digitalRead(EMERGENCY)==0) EMERGENCY_FLAG=true;
      // if(EMERGENCY_FLAG) break; 
      c = Serial.read();
      // nếu gặp các ký tự xuống dòng thì lưu và xử lý câu lệnh Gcode đó
      if ((c == '\n') || (c == '\r')) {  // End of line reached
        if (lineIndex > 0) {             // Line is complete. Then execute!
          line[lineIndex] = '\0';        // Terminate string
          if (verbose) {
            Serial.print("Received : ");
            Serial.println(line);
          }
          //================================================================
          
          if(!digitalRead(RESUME))
          { 
            static int firstcall_=0;
            while(!digitalRead(RESUME)) {
              
              if(firstcall_==0){
                lastZ=Zpos;
                penUp();
                home();
                Serial.println("pause by button");
                firstcall_=1;
              }
              
            }
            firstcall_=0;
            // nếu led xanh sáng là vẫn đang tiếp tục
          }
          //================================================================
          processIncomingLine(line, lineIndex);
          
          lineIndex = 0;  // reset lại index để tiếp tục xử lý dòng line mới
        } else {
          // Empty or comment line. Skip block.
        }
        lineIsComment = false;
        lineSemiColon = false;
        
        Serial.println("ok");
        
        // ngược lại không in gì lên cổng Serial - điều này sẽ làm tạm dừng chuyển Gcode sang

      }
      // ngược lại, nhận dạng comment và bỏ qua
      else {
        if ((lineIsComment) || (lineSemiColon)) {  // Throw away all comment characters
          if (c == ')') lineIsComment = false;     // End of comment. Resume line.
        } else {
          if (c <= ' ') {         // Throw away whitepace and control characters
          } else if (c == '/') {  // Block delete not supported. Ignore character.
          } else if (c == '(') {  // Enable comments flag and ignore all characters until ')' or EOL.
            lineIsComment = true;
          } else if (c == ';') {
            lineSemiColon = true;
          } else if (lineIndex >= LINE_BUFFER_LENGTH - 1) {
            Serial.println("ERROR - lineBuffer overflow");
            lineIsComment = false;
            lineSemiColon = false;
          }
          // Chuyển hết ký tự nhận được thành CHỮ IN HOA
          else if (c >= 'a' && c <= 'z') {  // Upcase lowercase
            line[lineIndex++] = c - 'a' + 'A';
          } else {
            line[lineIndex++] = c;
          }
        }
      }
    }
  }
}

void penUp() {
  penServo.write(penZUp);
  delay(200);
  Zpos = Zmax;
  if (verbose) {
    Serial.println("Pen up!");
  }
}
//  Lowers pen
void penDown(){
  penServo.write(penZDown);
  Zpos = Zmin;
  if (verbose) {
    Serial.println("Pen down.");
  }
}
/*********************************
 * Draw a line from (x0;y0) to (x1;y1). 
 * Bresenham algo from https://www.marginallyclever.com/blog/2013/08/how-to-build-an-2-axis-arduino-cnc-gcode-interpreter/
 * int (x1;y1) : Starting coordinates
 * int (x2;y2) : Ending coordinates
 **********************************/
void drawLine(float x1, float y1) {

  if (verbose) {
    Serial.print("fx1, fy1: ");
    Serial.print(x1);
    Serial.print(",");
    Serial.print(y1);
    Serial.println("");
  }

  //  Bring instructions within limits
  if (x1 >= Xmax) {
    x1 = Xmax;
  }
  if (x1 <= Xmin) {
    x1 = Xmin;
  }
  if (y1 >= Ymax) {
    y1 = Ymax;
  }
  if (y1 <= Ymin) {
    y1 = Ymin;
  }

  if (verbose) {
    Serial.print("Xpos, Ypos: ");
    Serial.print(Xpos);
    Serial.print(",");
    Serial.print(Ypos);
    Serial.println("");
  }

  if (verbose) {
    Serial.print("x1, y1: ");
    Serial.print(x1);
    Serial.print(",");
    Serial.print(y1);
    Serial.println("");
  }

  //  Convert coordinates to steps
  x1 = (long)(x1 * StepsPerMillimeterX);
  y1 = (long)(y1 * StepsPerMillimeterY);
  float x0 = Xpos;  // Xpos có đơn vị steps
  float y0 = Ypos;  // Ypos có đơn vị steps

  //  Let's find out the change for the coordinates
  long dx = abs(x1 - x0);
  long dy = abs(y1 - y0);
  int sx = x0 < x1 ? StepInc : -StepInc;  // Hướng tiến hoặc lùi từng bước của trục x
  int sy = y0 < y1 ? StepInc : -StepInc;  // Hướng tiến hoặc lùi từng bước của trục y

  long i;
  long over = 0;  // biến điều chỉnh giúp quyết định khi nào cần di chuyển động cơ trên trục còn lại (X hoặc Y).
  int state;
  float posX,posY;
  if (dx > dy) {
    for (i = 0; i < dx; ++i) {
      if(digitalRead(EMERGENCY)) 
      { Serial.println("emergency!");
        break;
      }
      // if(digitalRead(Xlimit_switch)==0||digitalRead(Ylimit_switch)==0){
      //   state=Zpos;
      //   posX=currentPos.x;
      //   posY=currentPos.y;
      //   penUp();
      //   protection();
      //   setup_coordinate();
      //   drawLine(posX,posY);
      //   delay(200);
      //   if(state) penDown();
      //   return;
      // }
      m1step(sx);
      over += dy;
      if (over >= dx) {
        over -= dx;
        m23step(sy);
      }
      wait((int)StepDelay/2);
    }
  } else {
    
    for (i = 0; i < dy; ++i) {
      if(digitalRead(EMERGENCY)) 
      { Serial.println("emergency!");
        break;
      }
      // if(digitalRead(Xlimit_switch)==0||digitalRead(Ylimit_switch)==0){
      //   state=Zpos;
      //   posX=currentPos.x;
      //   posY=currentPos.y;
      //   penUp();
      //   protection();
      //   setup_coordinate();
      //   drawLine(posX,posY);
      //   delay(200);
      //   if(state) penDown();
      //   return;
      // }
      m23step(sy);
      over += dx;
      if (over >= dy) {
        over -= dy;
        m1step(sx);
      }
      wait(StepDelay);
    }
  }

  if (verbose) {
    Serial.print("dx, dy:");
    Serial.print(dx);
    Serial.print(",");
    Serial.print(dy);
    Serial.println("");
  }

  if (verbose) {
    Serial.print("Going to (");
    Serial.print(x0);
    Serial.print(",");
    Serial.print(y0);
    Serial.println(")");
  }
  //  Delay before any next lines are submitted

  //  Update the positions
  Xpos = x1;  // đơn vị số bước - steps
  Ypos = y1;  // đơn vị số bước - steps
}


void review_Gcode() {
  Serial.print(F("GcodeCNC2Axis Project"));
  Serial.println("CNC shield V3");
  Serial.println(F("Commands:"));
  Serial.println(F("G00 [X(steps)] [Y(steps)] [F(feedrate)]; - line"));
  Serial.println(F("G01 [X(steps)] [Y(steps)] [F(feedrate)]; - line"));
  Serial.println(F("G02 [X(steps)] [Y(steps)] [I(steps)] [J(steps)] [F(feedrate)]; - clockwise arc"));
  Serial.println(F("G03 [X(steps)] [Y(steps)] [I(steps)] [J(steps)] [F(feedrate)]; - counter-clockwise arc"));
  Serial.println(F("G04 P[seconds]; - delay"));
  Serial.println(F("G90; - absolute mode"));
  Serial.println(F("G91; - relative mode"));
  Serial.println(F("G92 [X(steps)] [Y(steps)]; - change logical position"));
  Serial.println(F("M18; - disable motors"));
  Serial.println(F("M100; - this help message"));
  Serial.println(F("M114; - report position and feedrate"));
  Serial.println(F("T1; - try drawing a square"));
  Serial.println(F("U; - try testing servo up"));
  Serial.println(F("D; - try testing servo down"));
  Serial.println(F("All commands must end with a newline."));
}
void m1step(int dir) {
  digitalWrite(M123_ENA, LOW);
  digitalWrite(M1_DIR, dir == 1 ? LOW : HIGH);
  digitalWrite(M1_STEP, HIGH);
  digitalWrite(M1_STEP, LOW);
}

void m23step(int dir) {
  digitalWrite(M123_ENA, LOW);
  digitalWrite(M2_DIR, dir == 1 ? HIGH : LOW);
  digitalWrite(M3_DIR, dir == 1 ? HIGH : LOW);
  digitalWrite(M2_STEP, HIGH);
  digitalWrite(M3_STEP, HIGH);
  digitalWrite(M2_STEP, LOW);
  digitalWrite(M3_STEP, LOW);
}

void disable() {
  digitalWrite(M123_ENA, HIGH);
}
void feedrate(float nfr) {
  if (fr == nfr||nfr==0) return;  // same as last time?  quit now.
  if (nfr > MAX_FEEDRATE || nfr < MIN_FEEDRATE) {  // don't allow crazy feed rates
    Serial.print(F("New feedrate must be greater than "));
    Serial.print(MIN_FEEDRATE);
    Serial.print(F("steps/s and less than "));
    Serial.print(MAX_FEEDRATE);
    Serial.println(F("steps/s."));
    Serial.print(F("Changed to FEEDRATE:"));
    if (nfr > MAX_FEEDRATE) nfr = MAX_FEEDRATE;
    else if (nfr < MIN_FEEDRATE) nfr = MIN_FEEDRATE;
    Serial.println(nfr);
  }
  StepDelay = (int)60 * 1000000.0 / (nfr * StepsPerMillimeterY);
  fr = nfr;
}

/**
 * delay for the appropriate number of microseconds
 * @input ms how many milliseconds to wait
 */
void wait(long us) {
  delay(us / 1000);
  delayMicroseconds(us % 1000);  // delayMicroseconds doesn't work for values > ~16k.
}
float atan3(float dy, float dx) {
  float a = atan2(dy, dx);
  if (a < 0) a = (PI * 2.0) + a;
  return a;
}
// This method assumes the limits have already been checked.
// This method assumes the start and end radius match (both points are same distance from center)
// This method assumes arcs are not >180 degrees (PI radians)
// This method assumes all movement is at a constant Z height
// cx/cy - center of circle
// x/y - end position
// dir - ARC_CW or ARC_CCW to control direction of arc
// (posx,posy,posz) is the starting position
// line() is our existing command to draw a straight line using Bresenham's algorithm.
void arc(float cx, float cy, float x, float y, float dir) {
  // đầu vào gồm tâm đường tròn, điểm đầu đã biết(currentPos) vị trí điểm cuối và hướng
  // get radius
  float dx = currentPos.x - cx;
  float dy = currentPos.y - cy;
  float radius = sqrt(dx * dx + dy * dy);

  // find the sweep of the arc
  float angle1 = atan3(dy, dx);
  float angle2 = atan3(y - cy, x - cx);
  float sweep = angle2 - angle1;

  if (dir > 0 && sweep < 0) angle2 += 2 * PI;
  else if (dir < 0) angle1 += 2 * PI;

  sweep = angle2 - angle1;

  // get length of arc
  // float circumference=PI*2.0*radius;
  // float len=sweep*circumference/(PI*2.0);
  // simplifies to
  float len = abs(sweep) * radius;

  int i, num_segments = floor(len / MM_PER_SEGMENT);

  // declare variables outside of loops because compilers can be really dumb and inefficient some times.
  float nx, ny, nz, angle3, fraction;

  for (i = 0; i < num_segments; ++i) {
    // interpolate around the arc
    fraction = ((float)i) / ((float)num_segments);
    angle3 = (sweep * fraction) + angle1;

    // find the intermediate position
    nx = cx + cos(angle3) * radius;
    ny = cy + sin(angle3) * radius;
    // make a line to that intermediate position
    drawLine(nx, ny);
    currentPos.x = nx;
    currentPos.y = ny;
  }

  // one last line hit the end
  drawLine(x, y);
  if (mode_abs) {
    currentPos.x = x;
    currentPos.y = y;
  } else {
    currentPos.x = 0;
    currentPos.y = 0;
  }
}
// float parseNumber(char code, float val) {                                  // val là giá trị mặc định
//   char* ptr = line;                                                        // start at the beginning of buffer
//   while ((long)ptr > 1 && (*ptr) && (long)ptr < (long)line + lineIndex) {  // walk to the end
//     if (*ptr == code) {                                                    // if you find code on your walk,
//       return atof(ptr + 1);                                                // convert the digits that follow into a float and return it
//     }
//     ptr = strchr(ptr, ' ') + 1;  // take a step from here to the letter after the next space
//   }
//   return val;  // end reached, nothing found, return default val.
// }


void processIncomingLine(char* line, int charNB) {  // charNB là số ký tự hay số phần tử trong mảng line
  int currentIndex = 0;
  char buffer[64];  // Hope that 64 is enough for 1 parameter
  struct point newPos;
  newPos.x = Xmax;
  newPos.y = Ymin;
 
  while (currentIndex < charNB) {
    switch (line[currentIndex++]) {  // Select command, if any
      case 'K':
      {  
        buffer[0] = line[1];
        buffer[1] = line[2];
        buffer[2] = line[3];
        buffer[3] = '\0';
        if(atoi(buffer)==11){
          penZUp=last_degree;
          Serial.println("saved");
        }
        else if(atoi(buffer)==12){
          penZDown=last_degree;
          Serial.println("saved");
        }
        else penServo.write(atoi(buffer));
        last_degree=atoi(buffer);
        Serial.println(atoi(buffer));
        break;}
      case 'U':
        penUp();
        break;
      case 'D':
        penDown();
        break;
      case 'H':
        home();
        break;
      case 'T':
        // draw_border();
        break;
      case 'R':
        {setup_coordinate();
        disable();
        break;}
      case 'G':
        {
          buffer[0] = line[1];  // /!\ Dirty - Only works with 2 digit commands
          buffer[1] = line[2];
          buffer[2] = '\0';
          // buffer[1] = '\0';
          int gCode = atoi(buffer);  // Chuyển chuỗi thành số nguyên để so sánh
          // Serial.println(atoi(buffer));
          switch (gCode) {  // Select G command
            // chuyển chuỗi String trong buffer thành số nguyên Int
            case 0:  // G00 & G01 - Movement or fast movement. Same here
            case 1:
              {
                // /!\ Dirty - Suppose that X is before Y
                
                //strchr() được sử dụng để tìm kiếm vị trí đầu tiên của một ký tự cụ thể trong một chuỗi
                char* indexX = strchr(line + currentIndex, 'X');  // Get X/Y position in the string (if any)
                char* indexY = strchr(line + currentIndex, 'Y');
                char* indexF = strchr(line + currentIndex, 'F');  // get feedrate index
                // Serial.println(String(atof(indexX + 1))+" -  "+String(atof(indexY + 1)));
                feedrate(atof(indexF + 1));                       // cập nhập Stepdelay
                if(line[2]!='X') break;
                if (indexY <= 0) {
                  newPos.x = atof(indexX + 1);
                  newPos.y = currentPos.y;
                } else if (indexX <= 0) {
                  newPos.y = atof(indexY + 1);
                  newPos.x = currentPos.x;
                } else {
                  newPos.y = atof(indexY + 1);
                  indexY = '\0';
                  newPos.x = atof(indexX + 1);
                }
                // vẽ đường thẳng và cập nhập vị trí mới
                drawLine(newPos.x, newPos.y);
                //        Serial.println("ok");
                if (mode_abs) {
                  currentPos.x = newPos.x;
                  currentPos.y = newPos.y;
                } else {
                  currentPos.x = 0;
                  currentPos.y = 0;
                }
                break;
              }
            case 2:
            case 3: {  // arc G02 [X(steps)] [Y(steps)] [I(steps)] [J(steps)] [F(feedrate)]
                char *indexX=strchr(line + currentIndex,'X');
                char *indexY=strchr(line + currentIndex,'Y');
                char *indexI=strchr(line + currentIndex,'I');
                char *indexJ=strchr(line + currentIndex,'J');
                char* indexF = strchr(line + currentIndex,'F');
                // Serial.println(String(atof(indexX + 1))+" -  "+String(atof(indexY + 1))+" -  "+String(atof(indexI + 1))+" -  "+String(atof(indexJ + 1)) );
                feedrate(atof(indexF+1));
                arc(atof(indexI + 1) + (mode_abs?0:currentPos.x),
                    atof(indexJ + 1) + (mode_abs?0:currentPos.y),
                    atof(indexX + 1) + (mode_abs?0:currentPos.x),
                    atof(indexY + 1) + (mode_abs?0:currentPos.y),
                    -1);
                
                break;
              }
            case 4:
              {char* indexP = strchr(line + currentIndex, 'P');
              delay((int)atof(indexP + 1));
              break;  // dwell wait(parseNumber('P',0)*1000)
              }

            case 28:  // về vị trí gốc toạ độ X0 Y0
              {
              home();
              Serial.println("G28");
              break;}
            case 92:  // cài đặt vị trí hiện tại là gốc toạ độ
              {currentPos.x = 0;
              currentPos.y = 0;
              currentPos.z = Zmax;
              Serial.println("Yes, Current position is the original coordinate.");
              break;}
            case 90:
              {mode_abs = true;
              Serial.println("Absolute mode selected");
              break;}
            case 91:
              {mode_abs = false;
              currentPos.x = 0;
              currentPos.y = 0;
              Xpos = 0;
              Ypos = 0;
              Serial.println("Relative mode selected");
              break;}
            default: break;
          }
        }
      case 'M':
        {
          buffer[0] = line[currentIndex++];  // /!\ Dirty - Only works with 3 digit commands
          buffer[1] = line[currentIndex++];
          buffer[2] = line[currentIndex++];
          buffer[3] = '\0';
          switch (atoi(buffer)) {
            case 112: {
              static int firstCall = 0;
              if (firstCall %2 == 0) {
                // Chỉ thực hiện lần đầu tiên
                lastZ=Zpos;
                penUp();
                home();
                  // Đặt cờ để lần sau không vào đây nữa
              }
              firstCall++;
              char* indexS = strchr(line + currentIndex, 'S');
              float S = atof(indexS + 1);
              if (S==0){
                disable();
              }
              if (S==1){
                 // chạy tới điểm trc khi dừng
                 // sau đó trả lại vị trí cho servo
                if(lastZ==0) penDown();
                else penUp();
                delay(150);
              }
              break;
            }
            case 222:
              {
                if(lastZ==0) penDown();
                else penUp();
                delay(150);
              }
            case 300:
              {
                
                char* indexS = strchr(line + currentIndex, 'S');
                float Spos = atof(indexS + 1);
                //          Serial.println("ok");
                if (Spos == 30) {
                  penDown();
                }
                else if (Spos == 50) {
                  penUp();
                }
                break;
              }
            case 114:  // M114 - Repport position
              {Serial.print("Absolute position : X = ");
              Serial.print(currentPos.x);
              Serial.print("  -  Y = ");
              Serial.print(currentPos.y);
              Serial.print(" Feedrate= ");
              Serial.println(fr);
              break;}
            case 100: {review_Gcode(); break; } // M100 - review G-code instructions
            case 18:
              {home();
                disable();
              break;}
             
            default:
              // Serial.print("Command not recognized : M");
              // Serial.println(buffer);
              break;
          }
        }
    }
  }
}
void protection(){
  if(digitalRead(Xlimit_switch)==0){
    for(int i=0;i<(int)5*StepsPerMillimeterX;i++){// chạy ngược lại 5mm để không bị chạm công tắc hành trình
      m1step(-1);
      wait(200);// mỗi bước chờ 200us
    }
    while(digitalRead(Ylimit_switch)){
      m23step(-1);
      wait(200);
    }
    for(int i=0;i<(int)5*StepsPerMillimeterY;i++){// chạy ngược lại 5mm để không bị chạm công tắc hành trình
      m23step(1);
      wait(200);// mỗi bước chờ 200us
    }
  }
  else if(digitalRead(Ylimit_switch)==0){
    for(int i=0;i<(int)5*StepsPerMillimeterY;i++){// chạy ngược lại 5mm để không bị chạm công tắc hành trình
      m23step(1);
      wait(200);// mỗi bước chờ 200us
    }
    while(digitalRead(Xlimit_switch)){
      m1step(1);
      wait(200);
    }
    for(int i=0;i<(int)5*StepsPerMillimeterX;i++){// chạy ngược lại 5mm để không bị chạm công tắc hành trình
      m1step(-1);
      wait(200);// mỗi bước chờ 200us
    }
  }
}
void setup_coordinate(){
  if(Zpos=Zmin) penUp();
  while(digitalRead(Xlimit_switch)&&digitalRead(Ylimit_switch)){// chờ cho đến khi chạm công tắc hành trình của 2 trục 
    m1step(1);
    wait(100);
    m23step(-1);
    wait(100);
    // Serial.print("Xswitch"+String(digitalRead(Xlimit_switch)));
    // Serial.println(" <-> Yswitch"+String(digitalRead(Ylimit_switch)));
  }
  protection();
  currentPos.x=Xmax;
  currentPos.y=Ymin;
  Xpos=Xmax*StepsPerMillimeterX;
  Ypos=Ymin*StepsPerMillimeterY;
}
// void draw_border(){
//   String k="G1 X"+String(Xmin)+" Y"+String(Ymin)+" F1500.0";
//   char* c = k.c_str();  // Lấy con trỏ tới ký tự đầu tiên của chuỗi
//   processIncomingLine(c,64);
//   penDown();
//   k="G1 X"+String(Xmax)+" Y"+String(Ymin)+" F1500.0";
//   c = k.c_str();  // Lấy con trỏ tới ký tự đầu tiên của chuỗi
//   processIncomingLine(c,64);
//   k="G1 X"+String(Xmax)+" Y"+String(Ymax)+" F1500.0";
//   c = k.c_str();  // Lấy con trỏ tới ký tự đầu tiên của chuỗi
//   processIncomingLine(c,64);
//   k="G1 X"+String(Xmin)+" Y"+String(Ymax)+" F1500.0";
//   c = k.c_str();  // Lấy con trỏ tới ký tự đầu tiên của chuỗi
//   processIncomingLine(c,64);
//   k="G1 X"+String(Xmin)+" Y"+String(Ymin)+" F1500.0";
//   c = k.c_str();  // Lấy con trỏ tới ký tự đầu tiên của chuỗi
//   processIncomingLine(c,64);
//   penUp();
//   home();
// }
void home(){
  if (Zpos == Zmin) penUp();
  drawLine(Xmax, Ymin);
  currentPos.x = Xmax;
  currentPos.y = Ymin;
}