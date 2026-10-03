import serial
import time
import traceback

print("Starting serial test...")

try:
    print("Opening COM5...")

    ser = serial.Serial(
        port="COM5",
        baudrate=115200,
        timeout=1
    )

    print("COM5 opened successfully!")
    print("Waiting for serial data...")
    print("Press Ctrl+C to stop.\n")

    # ESP8266 may reset when serial port opens
    time.sleep(2)

    while True:
        if ser.in_waiting:
            data = ser.readline()

            if data:
                print(data.decode("utf-8", errors="replace").rstrip())

except KeyboardInterrupt:
    print("\nStopped by user.")

except Exception as e:
    print("\nERROR:")
    print(e)
    traceback.print_exc()

finally:
    try:
        ser.close()
    except:
        pass

    input("\nPress ENTER to close...")