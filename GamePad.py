import serial
from serial.tools.list_ports import comports
from tkinter import*
from tkinter.ttk import *

class GamePad:
    def __init__(self, window):
        right_button = Button(window, text="Right", command=self.move_right)
        right_button.grid(row=1, column=2)
        
        up_button = Button(window, text="Up", command=self.move_up)
        up_button.grid(row=0, column=1)

        left_button = Button(window, text="Left", command=self.move_left)
        left_button.grid(row=1, column=0)

        down_button = Button(window, text="Down", command=self.move_down)
        down_button.grid(row=2, column=1)

        window.bind('<Right>', self.move_right)
        window.bind('<Up>', self.move_up)
        window.bind('<Left>', self.move_left)
        window.bind('<Down>', self.move_down)
        

    def move_right(self, *args):
        ser.write(b'6')
        
    def move_up(self, *args):
        ser.write(b'8')

    def move_left(self, *args):
        ser.write(b'4')

    def move_down(self, *args):
        ser.write(b'2')


def main():
    window = Tk()
    window.title("Game Pad")
    GamePad(window)
    window.mainloop()


if __name__ == "__main__":
    available_ports = comports()
    print("Available ports:")
    for available_port in available_ports:
        print(available_port)
    
    PORT_NAME = input("Please enter a valid port name (e.g. \"COM3\"):")
    ser = serial.Serial(PORT_NAME, 9600)
    main()
    ser.close()
