import serial
import time

# Open the Arduino Opta USB serial port (Adjust COM port for your system)
ser = serial.Serial('COM3', 115200, timeout=1) 
time.sleep(2) # Wait for connection to stabilize

# Send a command to the Opta
ser.write(b'SLOW_MOTOR\n')