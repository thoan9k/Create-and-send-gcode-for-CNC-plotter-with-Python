String[] labels = {
  "Paper Size X", 
  "Paper Size Y", 
  "Image Size X", 
  "Image Size Y", 
  "Paper Top to Origin", 
  "Pen Width", 
  "Pen Count", 
  "Feetrate", 
  "Pen Zup", 
  "Pen Zdown", 
  "Port Name"
};  // Các tiêu đề biểu thị biến

String[] inputs = {
  "150", 
  "180", 
  "28 * 25.4", 
  "35 * 25.4", 
  "0", 
  "0.65", 
  "1", 
  "2000", 
  "M300 S50", 
  "M300 S30", 
  "COM3"
};  // Dữ liệu đầu vào cho từng biến

int focusedBox = -1;  // Biến để kiểm tra ô nào đang được chọn

void enter_input() {
  

  // Vẽ các ô nhập liệu với tiêu đề
  for (int i = 0; i < labels.length; i++) {
    // Hiển thị tiêu đề
    fill(0);
    text(labels[i], 50, 30 + i * 50); 
    
    // Vẽ ô nhập liệu
    if (focusedBox == i) {
      fill(230);  // Nếu ô này đang được chọn
    } else {
      fill(255);  // Nếu ô này không được chọn
    }
    rect(150, 10 + i * 50, 180, 30);  // Vị trí và kích thước ô nhập liệu (đã chỉnh kích thước)

    // Hiển thị nội dung nhập vào
    fill(0);
    text(inputs[i], 160, 30 + i * 50); 
  }

  // Vẽ nút xác nhận ngay dưới ô nhập liệu cuối cùng
  fill(100, 200, 100);  // Màu xanh lá
  rect(150, 10 + labels.length * 50 + 10, 100, 40);  // Vị trí và kích thước nút
  fill(255);
  text("Confirm", 175, 15+labels.length * 50 + 30);  // Chữ "Confirm" trên nút
}
