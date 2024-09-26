import cv2
import numpy as np
from matplotlib import pyplot as plt
import tkinter as tk
from tkinter import filedialog, messagebox
import Gcode_sender as send_gcode

gcode_filename = None
offsetx = 0.0
offsety = 0.0

def douglas_peucker(points, epsilon):
    return cv2.approxPolyDP(points, epsilon, closed=True)

def load_image(image_path):
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise ValueError(f"Cannot open image at: {image_path}")
    return img


def preprocess_image(img, threshold=127):
    _, binary_img = cv2.threshold(img, threshold, 255, cv2.THRESH_BINARY)
    return binary_img


def detect_edges(img):
    edges = cv2.Canny(img, 100, 200)
    return edges


def find_contours_and_simplify(img, epsilon=2.0):
    contours, _ = cv2.findContours(img, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    simplified_contours = [douglas_peucker(contour, epsilon) for contour in contours]
    return simplified_contours


def display_images(original, processed, title1="Ảnh gốc", title2="Ảnh đã xử lý"):
    plt.figure(figsize=(10, 5))
    plt.subplot(1, 2, 1)
    plt.imshow(original)
    plt.title(title1)
    plt.subplot(1, 2, 2)
    plt.imshow(processed, cmap='gray')
    plt.title(title2)
    plt.show()


def draw_contours(img, contours):
    img_with_contours = img.copy()
    cv2.drawContours(img_with_contours, contours, -1, (0, 255, 0), 2)
    return img_with_contours


def convert_to_vector(contours):
    vectors = []
    for contour in contours:
        points = contour.reshape(-1, 2)
        vectors.append(points)
    return vectors

def map_value(value, fromLow, fromHigh, toLow, toHigh):
    # Công thức chuyển đổi giá trị
    return toLow + (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow)

def convert_to_mm(vectors, img_width, img_height, paper_width_mm, paper_height_mm, offsetx, offsety):
    global x_min, x_max, y_min, y_max
    scale_x = paper_width_mm / img_width
    scale_y = paper_height_mm / img_height
    scale_ = min(scale_x, scale_y)

    vectors_mm = []
    x_pre = 0.0
    y_pre = 0.0

    # Khởi tạo giá trị x_min, x_max, y_min, y_max với các giá trị vô cực để đảm bảo cập nhật đúng
    x_min = float('inf')
    x_max = float('-inf')
    y_min = float('inf')
    y_max = float('-inf')

    for vector in vectors:
        # Giả định vector là một danh sách hoặc mảng 2D như [[x1, y1], [x2, y2], ...]
        scaled_vector = []
        for point in vector:
            x, y = point  # unpack từng cặp (x, y) từ vector
            scaled_x = x*scale_ + offsetx
            scaled_y = y*scale_ + offsety
            # scaled_x = map_value(x*scale_, 0,paper_width_mm*scale_, -paper_width_mm/2,
            #                      -paper_width_mm/2 + paper_width_mm*scale_) + offsetx
            # scaled_y = map_value(y*scale_, 0,paper_height_mm*scale_, -paper_height_mm/2,
            #                      -paper_height_mm/2 + paper_height_mm*scale_) + offsetx
            # Cập nhật x_min, x_max, y_min, y_max
            x_min = min(x_min, scaled_x)
            x_max = max(x_max, scaled_x)
            y_min = min(y_min, scaled_y)
            y_max = max(y_max, scaled_y)

            scaled_vector.append([scaled_x, scaled_y])
            x_pre = scaled_x
            y_pre = scaled_y

        vectors_mm.append(np.array(scaled_vector))

    return vectors_mm



def generate_gcode(vectors, filename="output.gcode", feedrate=1500):
    with open(filename, 'w') as gcode_file:
        gcode_file.write(f"(x range :{x_min} to {x_max} mm )\n")
        gcode_file.write(f"(y range :{y_min} to {y_max} mm )\n")
        gcode_file.write("G21 ( Set units to millimeters)\n")
        gcode_file.write("G90 ( Use absolute positioning)\n")
        gcode_file.write("M300 S50  (Pen up)\n")
        gcode_file.write("G4 P150\n")

        def test():
            gcode_file.write(f"G1 F2500\n")
            gcode_file.write("M300 S50  (Pen up)\n")
            gcode_file.write("G4 P150\n")
            gcode_file.write(f"G1 X{x_min} Y{y_min} \n")
            gcode_file.write(f"G1 X{x_min} Y{y_max} \n")
            gcode_file.write(f"G1 X{x_max} Y{y_max} \n")
            gcode_file.write(f"G1 X{x_max} Y{y_min} \n")
            gcode_file.write(f"G1 X{x_min} Y{y_min} \n")
        test()
        for vector in vectors:
            first_point = True
            for x, y in vector:
                if first_point:
                    gcode_file.write(f"G1 X{x:.2f} Y{y:.2f} F{feedrate}\n")
                    gcode_file.write("M300 S30  (Pen down)\n")
                    gcode_file.write("G4 P150\n")
                    first_point = False
                else:
                    gcode_file.write(f"G1 X{x:.2f} Y{y:.2f} F{feedrate}\n")
            gcode_file.write("M300 S50  (Pen up)\n")
            gcode_file.write("G4 P150\n")

        gcode_file.write("G4 P150 (Wait for 150ms)\n")
        gcode_file.write("M300 S50 (Pen up)\n")
        gcode_file.write("G28  (Home all axes)\n")

def update():
    global gcode_filename
    send_gcode.portname = str(port_entry.get())
    send_gcode.file_path = gcode_filename
    label.config(text=str(gcode_filename))
# Hàm khởi tạo giao diện
def setup_gui_():
    global feedrate_entry, paper_width_entry, paper_height_entry, offsetx_entry, offsety_entry, port_entry,gcodepath
    global label

    root = tk.Tk()
    root.title("Chuyển đổi hình ảnh sang G-code")
    n = 0
    # Nhập Feedrate
    tk.Label(root, text="Tốc độ đầu bút (Feedrate):").grid(row=n, column=0)
    feedrate_entry = tk.Entry(root)
    feedrate_entry.grid(row=0, column=1)
    feedrate_entry.insert(0, "2000")  # Giá trị mặc định
    n += 1
    # Nhập Offset x và offset y
    tk.Label(root, text="Độ lệch của trục X và Y so với gốc:").grid(row=n, column=0)
    offsetx_entry = tk.Entry(root)
    offsetx_entry.grid(row=n, column=1)
    offsetx_entry.insert(0, "0")
    offsety_entry = tk.Entry(root)
    offsety_entry.grid(row=n, column=2)
    offsety_entry.insert(0, "0")
    n += 1
    # Nhập Paper Width
    tk.Label(root, text="Chiều rộng của giấy (mm):").grid(row=n, column=0)
    paper_width_entry = tk.Entry(root)
    paper_width_entry.grid(row=n, column=1)
    paper_width_entry.insert(0, "210")  # Giá trị mặc định
    n += 1
    # Nhập Paper Height
    tk.Label(root, text="Chiều cao của giấy (mm):").grid(row=n, column=0)
    paper_height_entry = tk.Entry(root)
    paper_height_entry.grid(row=n, column=1)
    paper_height_entry.insert(0, "297")  # Giá trị mặc định
    n += 1
    # Nhập cổng Serial
    # # Tạo một biến để lưu lựa chọn của người dùng
    # selected_option = tk.StringVar()
    # selected_option.set("COM7")  # Giá trị mặc định hiển thị
    #
    # # Tạo OptionMenu với các tùy chọn
    # options = ["COM2", "COM3", "COM4", "COM5", "COM6", "COM7"]
    # option_menu = tk.OptionMenu(root, selected_option, *options)
    # option_menu.


    tk.Label(root, text=" Nhập cổng Serial: ").grid(row=n, column=0)
    port_entry = tk.Entry(root)
    port_entry.grid(row=n, column=1)
    port_entry.insert(0, "COM7")


    select_button=tk.Button(root,text="Update", command=update)
    select_button.grid(row= n, column = 2)
    n += 1
    # Nút thực hiện chuyển đổi
    process_button = tk.Button(root, text="Xử lý hình ảnh", command=process_image)
    process_button.grid(row=n, columnspan=3)
    n += 1
    # nút chọn file gcode
    tk.Label(root, text="Đường dẫn tệp G-code:").grid(row=n, column=0)
    gcodepath= tk.Button(root, text="Chọn", command=select_file)
    gcodepath.grid(row=n,column=2)
    # Tạo nhãn để hiển thị đường dẫn file (ban đầu để trống)
    label = tk.Label(root, text="")
    label.grid(row=n, column=1)
    n += 1
    # # Nút thực hiện truyền G-code
    send = tk.Button(root, text="Bắt đầu truyền G-code để điều khiển",
                     command=lambda: send_gcode.operate(gcode_filename))
    send.grid(row=n, columnspan=4)
    if(send_gcode.i==len(send_gcode.gcode) and send_gcode.i!=0):
        root.destroy()
    root.mainloop()

def show():
    global gcode_filename
    # Kiểm tra nếu người dùng đã chọn tệp
    if gcode_filename:
        # Tạo và hiển thị nhãn với đường dẫn tệp
        label.config(text=str(gcode_filename))

def select_file():
    global gcode_filename
    # Mở hộp thoại để chọn file và cập nhật biến gcode_filename
    gcode_filename = filedialog.askopenfilename()
    show()  # Gọi hàm show ngay lập tức để hiển thị đường dẫn file

def process_image():
    try:
        # Lấy các giá trị từ giao diện
        feedrate = int(feedrate_entry.get())
        paper_width_mm = float(paper_width_entry.get())
        paper_height_mm = float(paper_height_entry.get())
        offsetx = float(offsetx_entry.get())
        offsety = float(offsety_entry.get())
        image_path = filedialog.askopenfilename(title="Select an Image",
                                                filetypes=[("Image Files", "*.jpg;*.jpeg;*.png")])
        # Nhận lấy port
        send_gcode.portname = str(port_entry.get())
        original= cv2.imread(image_path)
        original=cv2.cvtColor(original, cv2.COLOR_BGR2RGB)
        img = load_image(image_path)
        binary_img = preprocess_image(img)
        edges = detect_edges(binary_img)
        simplified_contours = find_contours_and_simplify(edges)
        vectors = convert_to_vector(simplified_contours)

        img_width, img_height = img.shape[1], img.shape[0]
        vectors_mm = convert_to_mm(vectors, img_width, img_height, paper_width_mm, paper_height_mm, offsetx, offsety)

        # Chọn đường dẫn và tên file để lưu G-code
        global gcode_filename

        gcode_filename = filedialog.asksaveasfilename(title="Lưu tệp G-code", defaultextension=".gcode",
                                                      filetypes=[("G-code Files", "*.gcode")])
        if gcode_filename:
            # Tạo và hiển thị nhãn với đường dẫn tệp
            label.config(text=str(gcode_filename))
        if not gcode_filename:
            messagebox.showwarning("Warning", "No filename provided. G-code not saved.")
            return

        generate_gcode(vectors_mm, filename=gcode_filename, feedrate=feedrate)
        messagebox.showinfo("Success", "G-code generated successfully!")
        display_images(original, edges)

    except Exception as e:
        messagebox.showerror("Error", str(e))


if __name__ == "__main__":
    setup_gui_()
    # send_gcode.setup_gui()
