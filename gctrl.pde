import java.awt.event.KeyEvent;
import javax.swing.JOptionPane;
import processing.serial.*;

Serial port = null;
PFont boldFont;
// select and modify the appropriate line for your operating system
// leave as null to use interactive port (press 'p' in the program)
//String portname = null;
//String portname = Serial.list()[0]; // Mac OS X
//String portname = "/dev/ttyUSB0"; // Linux
String portname = "COM3"; // Windows
int k=0,l=0;
boolean streaming = false;
float speed = 5;
int degree=30;
String[] gcode;
int i = 0;
boolean ready=true;
int current_cmd=0; //index dòng lệnh được lưu khi tạm dừng CNC
void openSerialPort()
{
  if (portname == null) return;
  if (port != null) port.stop();
  
  port = new Serial(this, portname, 9600);
  
  port.bufferUntil('\n'); // cho phép chương trình đọc dữ liệu từ cổng serial cho đến khi gặp ký tự xuống dòng (\n)
}

void selectSerialPort()
{
  String[] availablePorts = Serial.list();
  //tạo ra một hộp thoại cho phép người dùng chọn cổng serial tương ứng với bo mạch Arduino.
  // Hộp thoại này yêu cầu người dùng chọn từ danh sách các cổng có sẵn
  String result = (String) JOptionPane.showInputDialog( 
    null,
    "Select the serial port that corresponds to your Arduino board.",
    "Select serial port",
    JOptionPane.PLAIN_MESSAGE,
    null,
    availablePorts,
    availablePorts.length > 0 ? availablePorts[0] : null);
    
  if (result != null) {
    portname = result;
    openSerialPort();
  }
}

void setup()
{
  size(500,520);
  openSerialPort();
}

void draw()
{
  background(144, 238, 144); // Màu tím đậm, RGB: (75, 0, 130)
  fill(72, 61, 139); // Màu vàng sáng, RGB: (255, 215, 0)
  int y = 12, dy = 30;
  stroke(255, 0, 0); // 
  // Đặt độ dày viền
  strokeWeight(5);
  // Không tô màu bên trong (chỉ có viền)
  noFill();
  // Vẽ hình chữ nhật
  rect(10, 20, 480, 470+20);
  stroke(255, 165, 0); 
  rect(15, 25, 470, 460+20);
  stroke(255, 255, 153); 
  rect(20, 30, 460, 450+20);
  //text(" ", 12, y); y += 10;
  boldFont = createFont("ProcessingSans-Bold", 32); // Kích thước font là 32
  textFont(boldFont); // Áp dụng font chữ
  text("            ", 5, y); y += 20;
  text("            INSTRUCTIONS", 5, y);y += 30;
  textSize(18); // Cỡ chữ 18
  text("    p: Chọn cổng Serial", 10, y); y += dy;
  text("    1: 5mm / 1 bước", 10, y); y += dy;
  text("    2: 10mm / 1 bước", 10, y); y += dy;
  text("    3: 15mm / 1 bước", 10, y); y += dy;
  text("    Phím          : chạy từng bước trong mặt phẳng OXY", 10, y); y += dy;
  text("    page up & page down: Nâng&hạ trục Z", 10, y); y += dy;
  text("    Tinh chỉnh Z - 'i'-lưu penZup hoặc 'o'-penZdown", 10, y); y += dy;
  text("    k: Hạ dần đầu bút xuống - l: Nâng dần đầu bút lên.", 10, y); y+= dy;
  text("    h: Về vị trí gốc", 10, y); y += dy;
  text("    0: Reset vị trí hiện tại (set home to the current location)", 10, y); y += dy;
  text("    g: Phát trực tuyến tệp G-code", 10, y); y += dy;
  text("    n: Tạm dừng in  ;                     m: tiếp tục in", 10, y); y += dy;
  text("    x: Dừng phát tệp G-code (không dừng ngay tức thì)", 10, y);y=y+15;
  text("    --------------------------------------------------------------------------", 10, y); y += dy;
  y = height - dy;
  text("    Bước nhảy hiện tại: " + speed + " mm per step", 10, y); y -= dy;
  text("    Cổng Serial hiện tại: " + portname, 10, y); y -= dy;
  drawPlayIcon(380, 355+20, 25);       // Vẽ biểu tượng chơi
  drawPauseIcon(160, 355+20, 25, 25); // Vẽ biểu tượng tạm dừng
  drawSaveIcon(420, 200+20,30);
  drawFourDirectionArrow(100, 155+20, 40);
  // Kích thước của mũi tên
  float arrowWidth = 20;
  float arrowHeight = 5;
  float bodyHeight = 5;
  
  // Vẽ mũi tên lên và xuống với kích thước đã điều chỉnh
  drawArrow(370,185 - 15+20, true, arrowWidth, arrowHeight, bodyHeight);  // Mũi tên lên
  drawArrow(370,185 + 15+20, false, arrowWidth, arrowHeight, bodyHeight); // Mũi tên xuống
  drawAxes(180,280+20,20,3);
  drawCancelIcon(455, 415,20);
}
void drawCancelIcon(float x, float y, float size) {
  stroke(255, 0, 0); // Màu đỏ cho dấu X
  strokeWeight(4);
  noFill();
  
  // Vẽ hai đường chéo tạo thành dấu X
  line(x - size / 2, y - size / 2, x + size / 2, y + size / 2); // Đường chéo từ trên trái xuống dưới phải
  line(x + size / 2, y - size / 2, x - size / 2, y + size / 2); // Đường chéo từ trên phải xuống dưới trái
}
void drawAxes(float x, float y, float axisLength, float arrowSize) {
  stroke(0);
  fill(0);
  
  // Vẽ trục X
  line(x - axisLength, y, x + axisLength, y);  // Vẽ trục ngang
  // Vẽ trục Y
  line(x, y - axisLength, x, y + axisLength);  // Vẽ trục dọc
  
  // Vẽ mũi tên cho trục X
  line(x + axisLength - arrowSize, y - arrowSize, x + axisLength, y);
  line(x + axisLength - arrowSize, y + arrowSize, x + axisLength, y);
  
  // Vẽ mũi tên cho trục Y
  line(x - arrowSize, y - axisLength + arrowSize, x, y - axisLength);
  line(x + arrowSize, y - axisLength + arrowSize, x, y - axisLength);
  
  // Ghi chú các trục (tuỳ chọn)
  text("X", x + axisLength + 5, y+10);
  text("Y", x-20, y-10);
}

void drawArrow(float x, float y, boolean up, float arrowWidth, float arrowHeight, float bodyHeight) {
  pushMatrix();
  translate(x, y);
  if (!up) {
    rotate(PI); // Xoay mũi tên xuống
  }
  
  // Vẽ đầu mũi tên
  stroke(0);
  fill(0);
  
  // Đầu mũi tên (tam giác)
  beginShape();
  vertex(-arrowWidth / 2, bodyHeight); // Điểm dưới bên trái của đầu mũi tên
  vertex(arrowWidth / 2, bodyHeight);  // Điểm dưới bên phải của đầu mũi tên
  vertex(0, -arrowHeight);            // Đỉnh của đầu mũi tên
  endShape(CLOSE);

  // Thân mũi tên (hình chữ nhật)
  beginShape();
  vertex(-bodyHeight, bodyHeight); // Điểm dưới bên trái của thân
  vertex(bodyHeight, bodyHeight);  // Điểm dưới bên phải của thân
  vertex(bodyHeight, bodyHeight + bodyHeight); // Điểm trên bên phải của thân
  vertex(-bodyHeight, bodyHeight + bodyHeight); // Điểm trên bên trái của thân
  endShape(CLOSE);

  popMatrix();
}
void keyPressed()
{
  if (key == '1') speed =5.0;
  if (key == '2') speed = 10.0;
  if (key == '3') speed = 15.0;
  
  if (!streaming) {
    if (keyCode == LEFT) port.write("G91\nG20\nG1 X" + -speed + " Y0.000 Z0.000 F2500.0\nG90\n");
    if (keyCode == RIGHT) port.write("G91\nG20\nG1 X" + speed + " Y0.000 Z0.000 F2500.0\nG90\n");
    if (keyCode == UP) port.write("G91\nG20\nG1 X0.000 Y" + speed + " Z0.000 F2500.0\nG90\n");
    if (keyCode == DOWN) port.write("G91\nG20\nG1 X0.000 Y" + -speed + " Z0.000 F2500.0\nG90\n");
    if (keyCode == KeyEvent.VK_PAGE_UP) port.write("M300 S50.00\n");
    if (keyCode == KeyEvent.VK_PAGE_DOWN) port.write("M300 S30.00\n");
    if (key == 'h') port.write("G28\n");
    if (key == 'v') port.write("$0=75\n$1=74\n$2=75\n");
    //if (key == 'v') port.write("$0=100\n$1=74\n$2=75\n");
    if (key == 's') port.write("$3=10\n");
    if (key == 'e') port.write("$16=1\n");
    if (key == 'd') port.write("$16=0\n");
    if (key == '0') openSerialPort();
    if (key == 'p') selectSerialPort();
    if (key == '$') port.write("$$\n");

    if (key == 'k') {
      degree=degree +10;
      if(degree<100){
        port.write("K0" + degree+"\n");
        }
       else port.write("K" + degree+"\n");
      }
    if (key == 'l') {
      degree=degree -10;
      if(degree<100){
        port.write("K0" + degree+"\n");
        }
      else port.write("K" + degree+"\n");
      }
      if (key == 'i') {
        port.write("K011\n");
      }
      if (key == 'o') {
        port.write("K012\n");
      }
      
    }
  
  if (!streaming && key == 'g') {
    gcode = null; i = 0;
    File file = null; 
    println("Loading file...");
    // selectInput(String prompt, String callback, File defaultLocation);
    //      String prompt:văn bản thông báo
    //      String callback: Tên của hàm callback sẽ được gọi 
    //      File defaultLocation: vị trí tệp mặc định mà hộp thoại chọn tệp sẽ mở khi bắt đầu. nếu là null hộp thoại sẽ mở ở vị trí mặc định của hệ điều hành hoặc thư mục hiện tại.
    selectInput("Select a file to process:", "fileSelected", file);
  }
  if (key == 'r') port.write("R\n");
  if (key == 'x') streaming = false;
  if (key == 'n') {
      _pause_();
      current_cmd=i-1; // gcode[i] chính là dòng lệnh Gcode hiện tại
    }
  if (key == 'm') {
      _continue_();
    }
}

void fileSelected(File selection) {
  if (selection == null) {
    println("Window was closed or the user hit cancel.");
  } else {
    println("User selected " + selection.getAbsolutePath());
    gcode = loadStrings(selection.getAbsolutePath()); // bắt đầu nhận Gcode, nhận hết tất cả Gcode
    if (gcode == null) return;
    streaming = true;
    stream();
  }
}

void stream()// mỗi lần nếu nhận được phản hồi từ vxl thì gửi 1 câu lệnh G-code tiếp theo
{
  if (!streaming) return;
  
  while (true) {
    if (i == gcode.length) { // Kiểm tra xem chỉ số i có vượt qua hoặc bằng chiều dài của mảng gcode không.
      streaming = false;
      return;
    }
    
    if (gcode[i].trim().length() == 0) i++;  // dòng trống thì bỏ qua
    else break;  // nếu không phải dòng trống thoát khỏi vòng lặp, cho phép xử lý và gửi dòng G-code hiện tại.
  }
  
  println(gcode[i]); //  In dòng G-code hiện tại ra bảng điều khiển. Đây là một cách để kiểm tra xem dòng nào đang được gửi.
  port.write(gcode[i] + '\n'); // Gửi dòng G-code hiện tại đến cổng serial, kèm theo ký tự xuống dòng (\n)
  i++;
  if(i==gcode.length - 1) port.write("M18\n");// về home và tắt động cơ
}

// Trong Processing, khi bạn mở cổng serial và có dữ liệu được gửi từ thiết bị qua cổng đó, Processing tự động gọi hàm serialEvent(Serial p) mỗi khi có dữ liệu mới.
// Hàm serialEvent(Serial p) được gọi tự động khi có dữ liệu mới từ cổng serial.
void _pause_(){
  ready=false;
  port.write("M112 S0\n");
  
}
void _continue_() {
  
  port.write(gcode[current_cmd]+'\n');
  
  //String s = port.readStringUntil('\n');
  if (k==1) {    
    port.write("M112 S1\n");  // lệnh báo tiếp tục
    ready = true;
    println("successfully");
  }
}
void drawPauseIcon(float x, float y, float width, float height) {
  fill(255, 0, 0); // Màu trắng
  rect(x, y, width / 3, height);       // Vạch trái
  rect(x + (width / 2), y, width / 3, height);  // Vạch phải
}
void drawPlayIcon(float x, float y, float size) {
  fill(0, 255, 0); // Màu xanh lá
  beginShape();
  vertex(x, y);
  vertex(x, y + size);
  vertex(x + size, y + size / 2);
  endShape(CLOSE);
}
void drawSaveIcon(float x, float y, float size) {
  // Vẽ viền màu vàng sáng
  stroke(0); // Màu vàng sáng
  strokeWeight(1);     // Độ dày viền

  // Vẽ hình vuông chính làm thân đĩa mềm
  fill(192); // Màu xám
  rect(x, y, size, size);

  // Reset lại viền
  noStroke();
  // Các phần khác không có viền như phần nhãn và lỗ đĩa...
  fill(128); 
  rect(x + size * 0.15, y + size * 0.05, size * 0.7, size * 0.3);

  fill(255); 
  rect(x + size * 0.25, y + size * 0.45, size * 0.5, size * 0.35);

  fill(0); 
  ellipse(x + size * 0.5, y + size * 0.9, size * 0.2, size * 0.2);
}
void drawFourDirectionArrow(float x, float y, float size) {
  float arrowSize = size * 0.2;

  // Mũi tên hướng lên
  triangle(x, y - size/2, x - arrowSize, y - size/2 + arrowSize, x + arrowSize, y - size/2 + arrowSize);

  // Mũi tên hướng xuống
  triangle(x, y + size/2, x - arrowSize, y + size/2 - arrowSize, x + arrowSize, y + size/2 - arrowSize);

  // Mũi tên hướng trái
  triangle(x - size/2, y, x - size/2 + arrowSize, y - arrowSize, x - size/2 + arrowSize, y + arrowSize);

  // Mũi tên hướng phải
  triangle(x + size/2, y, x + size/2 - arrowSize, y - arrowSize, x + size/2 - arrowSize, y + arrowSize);
}
void serialEvent(Serial p) // hàm này đọc phản hồi của Board VXL 
{
  String s = p.readStringUntil('\n');
  println(s.trim());
  
  if(ready){// dừng stream code nếu nhấn 'n'- pause
    
    //if(l==i-1 && s.trim().startsWith("ok") ) port.write("M112 S1\n");
    if(l==0&& s.trim().startsWith("ok")) stream();
    else if (l==0&& s.trim().startsWith("error")) stream(); // XXX: really?
    else if(s.trim().startsWith("pause by button")) {
      i=i-2;// lùi câu lệnh gcode về trước đó
      l=i;
      //println("l="+l);
    }
    if(l==i&&l!=0&&s.trim().startsWith("ok")){
      port.write("M222\n");
      l=0;
      println("i=="+i);
    }
    if(l==0 && s.trim().startsWith("emergency!")){
      i=gcode.length;
    }
  }
  else{
    if(s.trim().startsWith("ok")) k=1;
    else k=0;
    //println("k="+k);
  }
}
