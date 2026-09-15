"""
Reads live RPM/vibration data from the Arduino Uno over serial, samples it
once per second, and sends each sample to the Vertex AI endpoint for a
prediction.

If the model predicts -1.0 five times in a row, prints:
    "Error with the machine"

Expects lines from the Arduino like:
RPM:1234.56, Fric_X(g):0.123, Fatg_X(mm/s):0.456, Fric_Y(g):0.123,
Fatg_Y(mm/s):0.456, Fric_Z(g):0.123, Fatg_Z(mm/s):0.456
"""

import re
import time
import serial

from google.cloud import aiplatform

# ---- CONFIG ----
SERIAL_PORT = "/dev/ttyACM0"   # Windows: e.g. "COM3"
BAUD_RATE = 115200             # must match Serial.begin() in the sketch
SAMPLE_INTERVAL_SEC = 1.0      # only feed the model once per second

PROJECT = "ai4i-dataset"
LOCATION = "us-west2"
ENDPOINT_ID = "projects/ai4i-dataset/locations/us-west2/endpoints/2776550534134366208"

CONSECUTIVE_ERROR_THRESHOLD = 5

# Sensor fields the Arduino sends, in the order the model expects them.
SENSOR_COLUMNS = [
    "RPM",
    "Fric_X(g)",
    "Fatg_X(mm/s)",
    "Fric_Y(g)",
    "Fatg_Y(mm/s)",
    "Fric_Z(g)",
    "Fatg_Z(mm/s)",
]

# Matches "Label:value" pairs, e.g. "Fric_X(g):0.123"
PATTERN = re.compile(r"([A-Za-z_]+(?:\([^)]*\))?):(-?\d+\.?\d*)")


def parse_line(line):
    """Parse a serial line into a dict of sensor values, or None if invalid."""
    matches = PATTERN.findall(line)
    if len(matches) != len(SENSOR_COLUMNS):
        return None

    data = {}
    for key, value in matches:
        if key not in SENSOR_COLUMNS:
            return None
        try:
            data[key] = float(value)
        except ValueError:
            return None

    return data


def predict(endpoint, data):
    """Send one sensor reading to the model and return the prediction."""
    instance = [data[col] for col in SENSOR_COLUMNS]
    result = endpoint.predict(instances=[instance])
    return result.predictions[0]


def main():
    aiplatform.init(project=PROJECT, location=LOCATION)
    endpoint = aiplatform.Endpoint(ENDPOINT_ID)

    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)  # allow Arduino to reset after opening the port

    consecutive_errors = 0
    last_sample_time = 0.0

    print("Monitoring machine. Press Ctrl+C to stop.")
    try:
        while True:
            raw = ser.readline().decode("utf-8", errors="ignore").strip()
            if not raw:
                continue

            data = parse_line(raw)
            if data is None:
                # Incomplete/garbled line (e.g. mid-transmission) - skip it
                continue

            now = time.time()
            if now - last_sample_time < SAMPLE_INTERVAL_SEC:
                # Throttle: only predict once per second
                continue
            last_sample_time = now

            prediction = predict(endpoint, data)
            print(data, "->", prediction)

            if prediction == -1.0:
                consecutive_errors += 1
            else:
                consecutive_errors = 0

            if consecutive_errors >= CONSECUTIVE_ERROR_THRESHOLD:
                print("Error with the machine")
                consecutive_errors = 0  # reset so it doesn't spam every sample

    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
