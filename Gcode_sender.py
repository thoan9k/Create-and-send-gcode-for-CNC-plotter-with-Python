import serial
import tkinter as tk
from tkinter import simpledialog, filedialog
import os
import time  # Đừng quên import thư viện này
port = None
portname = None  # Chọn COM6 làm cổng mặc định
streaming = False
speed = 5
gcode = []
file_path = None
i = 0
k = 0
l = 0
current_cmd = 0
ready = True
degree = 30


def open_serial_port():
    global port
    if portname is None:
        return
    if port is not None:
        port.close()
    try:
        port = serial.Serial(portname, 9600)
        print(f"Serial port {portname} opened successfully.")
    except serial.SerialException as e:
        print(f"Error opening serial port {portname}: {e}")
        port = None


def select_serial_port():
    global portname
    if portname is not None:
        open_serial_port()  # Nếu đã có portname thì mở cổng ngay lập tức
        return
    portname = simpledialog.askstring("Select serial port", "Enter the serial port name:")
    open_serial_port()


def key_pressed(event):
    global speed, streaming, gcode, i, degree
    if event.char == '1':
        speed = 5
    elif event.char == '2':
        speed = 10
    elif event.char == '3':
        speed = 15
    elif event.char == 'p':
        select_serial_port()  # Thêm dòng này để chọn cổng serial khi nhấn 'p'

    if port is None:
        print("Serial port not opened. Please select a port.")
        return

    if not streaming:

        if event.keysym == 'Left':
            port.write(f"G91\nG20\nG00 X-{speed} Y0.000 Z0.000\n".encode())
        elif event.keysym == 'Right':
            port.write(f"G91\nG20\nG00 X{speed} Y0.000 Z0.000\n".encode())
        elif event.keysym == 'Up':
            port.write(f"G91\nG20\nG00 X0.000 Y{speed} Z0.000\n".encode())
        elif event.keysym == 'Down':
            port.write(f"G91\nG20\nG00 X0.000 Y-{speed} Z0.000\n".encode())
        elif event.keysym == 'Page_Up':
            port.write(f"G91\nG20\nG00 X0.000 Y0.000 Z{speed}\n".encode())
        elif event.keysym == 'Page_Down':
            port.write(f"G91\nG20\nG00 X0.000 Y0.000 Z-{speed}\n".encode())
        elif event.char == 'h':
            port.write("G90\nG20\nG00 X0.000 Y0.000 Z0.000\n".encode())
        elif event.char == '$':
            port.write("$$\n".encode())
        elif event.char == 'h':
            port.write("G28\n".encode())
        elif event.char == 'k':
            degree += 10
            if degree < 100:
                port.write(("K0" + str(degree) + "\n").encode())
            else:
                port.write(("K" + str(degree) + "\n").encode())
        elif event.char == 'l':

            degree -= 10
            if degree < 100:
                port.write(("K0" + str(degree) + "\n").encode())
            else:
                port.write(("K" + str(degree) + "\n").encode())
        elif event.char == 'i':
            port.write("K011\n".encode())
        elif event.char == 'o':
            port.write("K012\n".encode())

    if not streaming and event.char == 'g':
        global file_path
        gcode = None
        i = 0
        file_path = filedialog.askopenfilename(title="Select a file to process:")
        if file_path:
            with open(file_path, 'r') as file:
                gcode = file.readlines()
            streaming = True
            stream()
    if event.char == 'r':
        port.write("R\n".encode())
    if event.char == 'x':
        streaming = False
    if event.char == 'n':
        global current_cmd
        _pause_()
        current_cmd = i - 1  # gcode[i] chính là dòng lệnh Gcode hiện tại

    if event.char == 'm':
        _continue_()


def stream_gcode():
    global gcode, streaming, i, file_path
    if port is None:  # chờ kết nối cổng COM
        select_serial_port()  # chọn cổng serial nếu chưa kết nối tới cổng COM nào
        if port is None:
            print("Incorrect port", portname)
            return

    if file_path is None:
        print("chưa có đường dẫn tệp Gcode!")
        file_part = filedialog.askopenfilename(title="Chọn tệp Gcode:")
    else:

        file_part = file_path  # Nếu đã có file_path, gán file_part từ file_path

    if not os.path.exists(file_part):  # kiểm tra sự tồn tại của file
        print(f"File not found: {file_part}")
        return

    try:
        with open(file_part, 'r') as file:
            gcode = file.readlines()  # đọc tất cả các line trên file
            print(f"Successfully loaded G-code from {file_part}")
            streaming = True
            i = 0
            stream()

    except Exception as e:
        print(f"Error reading file {file_part}: {e}")


def _pause_():
    global ready
    ready = False
    port.write("M112 S0\n".encode())


def _continue_():
    # String s = port.readStringUntil('\n'
    global ready
    port.write((gcode[current_cmd] + "\n").encode())

    if k == 1:
        port.write("M112 S1\n".encode())  # lệnh báo tiếp tục
        ready = True
        # println("successfully")


def stream():
    global i, streaming
    if not streaming or gcode is None or port is None:  # ngắt port là dừng
        return

    while i < len(gcode):
        if len(gcode[i].strip()) == 0:  # bỏ qua câu lệnh nếu trống
            i += 1
            continue
        print(gcode[i].strip() + '\n')  # in câu lệnh vừa nhận được
        port.write((gcode[i].strip() + '\n').encode())
        i += 1
        if i == len(gcode) - 1:
            port.write("M18\n".encode())  # về home và tắt động cơ
            streaming= False
        break


def serial_event():
    global l, i, k, streaming
    if port is not None and port.in_waiting > 0:
        s = port.readline().decode().strip()
        print(s)
        if ready:
            if s.startswith("ok") or s.startswith("error"):
                if l == 0:
                    stream()
            if s.startswith("pause by button"):
                i -= 2
                l = i
            if l == i and l != 0 and s.startswith("ok"):
                port.write("M222\n".encode())
                l = 0

            if s.startswith("emergency!"):
                streaming = False
        else:
            if s.startswith("ok"):
                k = 1
            else:
                k = 0


def setup_gui():
    global file_path, i, streaming
    root = tk.Tk()

    s=0
    if not streaming:
        canvas = tk.Canvas(root, width=500, height=500, bg="black")
        canvas.pack()
        root.title("Serial Port Controller")
        instructions = [
            "INSTRUCTIONS",
            "         ",
            "===========================================================",
            "         ",
            "   p: Chọn cổng Serial",
            "   1: 5mm / 1 bước",
            "   2: 10mm / 1 bước",
            "   3: 15mm / 1 bước",
            "   Phím lên/xuống/trái/phải: chạy từng bước trong mặt phẳng OXY",
            "   page up & page down:  Nâng & hạ trục Z",
            "   Tinh chỉnh Z - 'i'-lưu penZup hoặc 'o'-penZdown",
            "   k: Hạ dần đầu bút xuống - l: Nâng dần đầu bút lên",
            "   h: Về vị trí gốc",
            "   g: Truyền tệp G-code để điều khiển ",
            "   n: Tạm dừng in  ;              m: tiếp tục in",
            "   x: Dừng truyền Gcode (không dừng ngay)",
            "",
            "===========================================================",
            "",
            "Bước nhảy hiện tại: " + str(speed) + " mm per step",
            "Cổng Serial hiện tại: " + str(portname)
        ]

        y = 24
        for instruction in instructions:
            canvas.create_text(24, y, anchor=tk.W, fill="white", text=instruction)
            y += 20
    else:
        global file_path
        canvas = tk.Canvas(root, width=400, height=250, bg="black")
        canvas.pack()
        root.title("Đang truyền G-code...")
        canvas.create_text(130, 200, anchor=tk.W, fill="blue", text="ĐANG TRUYỀN G-CODE...")
        stream_gcode()
        if file_path:
            canvas.create_text(10, 10, anchor=tk.W, fill="green", text="File path:" + str(file_path))
        s=1
    root.bind("<KeyPress>", key_pressed)
    text_ids = []

    def check_serial():

        serial_event()
        root.after(100, check_serial)
        if streaming and s==1:

            current_y = i + 180 - i
            for text_id in text_ids:
                canvas.move(text_id, 0, -20)  # Di chuyển lên 2 pixel mỗi lần lặp
            # Kiểm tra xem text đã ra khỏi canvas chưa, nếu có thì xóa
            for text_id in text_ids:
                x, y, _, _ = canvas.bbox(text_id)  # Lấy tọa độ của text
                if y < 20:  # Nếu dòng text di chuyển ra khỏi canvas
                    canvas.delete(text_id)
                    text_ids.remove(text_id)
            # Nếu còn khoảng trống ở cuối canvas, thêm dòng text mới
            new_text_id = canvas.create_text(30, current_y, anchor=tk.W, fill="white", text=gcode[i])
            text_ids.append(new_text_id)
            current_y += 20  # Tăng vị trí Y để dòng text mới tạo ở phía dưới cùng

    root.after(50, check_serial)

    if i== len(gcode) :
        root.destroy()
    root.mainloop()


def operate(gcode_file):
    global streaming, file_path

    # Đầu tiên mở cổng serial
    open_serial_port()  # Mở cổng serial
    file_path=gcode_file
    streaming =True
    setup_gui()

if __name__ == "__main__":
    # open_serial_port()  # Mở cổng serial khi chương trình bắt đầu, nếu portname đã được chỉ định
    # file_path=None
    # stream_gcode(file_path)
    # file_path=r"C:\Users\Admin\Desktop\gcode\logo.gcode"
    portname = "COM3"
    file_path=r"C:\Users\Admin\Desktop\gcode\logo.gcode"
    operate(file_path)


