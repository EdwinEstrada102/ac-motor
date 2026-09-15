#include ArduinoRS485.h

 ==========================================
 RS485 CONFIGURATION (Opta built-in RS485 port)
 ==========================================
 Both the vibration sensor and the VFD share this single RS485 bus.
#define RS485_BAUD 9600

 --- TACHOMETER CONFIGURATION ---
 NOTE confirm this pin supports attachInterrupt on your Opta variantexpansion.
const int hallPin = 2;
volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;
float smoothedRpm = 0.0;
const float alpha = 0.15;  Smoothing factor (0.0 to 1.0)

 --- VFD MODBUS CONFIGURATION ---
#define VFD_SLAVE_ADDR     0x01      Fd-02 on the VFD, default = 1
#define VFD_COMM_FREQ_REG  0x1000    Communication set value (-10000~10000, 10000=100%)
#define VFD_RUN_CMD_REG    0x2000    Control command word (runstop)
#define VFD_RUNFREQ_REG    0x1001    Running frequency (read-only, U0-00), 0.01Hz units
#define VFD_MAX_FREQ_HZ    50.0      Must match F0-10 on the VFD
#define SLOW_MOTOR_HZ      10.0

String serialCommand = ;

void setup() {
   USB serial for PCPlotterPython commands
  Serial.begin(115200);
  while (!Serial) delay(10);

   Built-in RS485 port; library handles DERE switching automatically
  RS485.begin(RS485_BAUD);

   Initialize tachometer interrupt
  pinMode(hallPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(hallPin), pulseISR, FALLING);
}

void loop() {
   ==========================================
   0. CHECK FOR COMMANDS FROM PYTHON (USB SERIAL)
   ==========================================
  checkSerialCommands();

   ==========================================
   1. CALCULATE RPM
   ==========================================
  noInterrupts();
  unsigned long interval = pulseInterval;
  unsigned long lastPulse = lastPulseTime;
  interrupts();

  float rawRpm = 0.0;
  if (micros() - lastPulse  2000000) {
    rawRpm = 0.0;
  } else if (interval  0) {
    rawRpm = 60000000.0  interval;
  }

  if (rawRpm == 0.0) {
    smoothedRpm = 0.0;
  } else {
    smoothedRpm = (alpha  rawRpm) + ((1.0 - alpha)  smoothedRpm);
  }

   ==========================================
   2. FETCH VIBRATION DATA (Friction & Fatigue)
   ==========================================
  float frictionX = 0, fatigueX = 0;
  float frictionY = 0, fatigueY = 0;
  float frictionZ = 0, fatigueZ = 0;

  bool successX = readSensorBlock(0x004B, frictionX, fatigueX);
  bool successY = readSensorBlock(0x0057, frictionY, fatigueY);
  bool successZ = readSensorBlock(0x0063, frictionZ, fatigueZ);

   ==========================================
   3. OUTPUT TO SERIAL PLOTTER
   ==========================================
  if (successX && successY && successZ) {
    Serial.print(RPM); Serial.print(smoothedRpm);

    Serial.print(, Fric_X(g)); Serial.print(frictionX, 3);
    Serial.print(, Fatg_X(mms)); Serial.print(fatigueX, 3);

    Serial.print(, Fric_Y(g)); Serial.print(frictionY, 3);
    Serial.print(, Fatg_Y(mms)); Serial.print(fatigueY, 3);

    Serial.print(, Fric_Z(g)); Serial.print(frictionZ, 3);
    Serial.print(, Fatg_Z(mms)); Serial.println(fatigueZ, 3);
    Serial.println(, Machine_State0);
  }
}

 ==========================================
 HELPER FUNCTIONS
 ==========================================

 Tachometer Interrupt Service Routine
void pulseISR() {
  unsigned long currentTime = micros();
  unsigned long tempInterval = currentTime - lastPulseTime;

  if (tempInterval  25000) {
    pulseInterval = tempInterval;
    lastPulseTime = currentTime;
  }
}

 Sends a raw Modbus RTU request frame over RS485
void sendModbusRequest(byte request, int len) {
  RS485.beginTransmission();
  RS485.write(request, len);
  RS485.endTransmission();
}

 Reads up to maxLen bytes from RS485 into buffer within timeoutMs.
 Returns the number of bytes actually read.
int readModbusResponse(byte buffer, int maxLen, unsigned long timeoutMs) {
  RS485.receive();

  int count = 0;
  unsigned long start = millis();

  while (millis() - start  timeoutMs && count  maxLen) {
    if (RS485.available()) {
      buffer[count++] = RS485.read();
      start = millis();  reset timeout on each byte received (frame gap detection)
    }
  }

  RS485.noReceive();
  return count;
}

 Function to request 6 contiguous registers starting at the given address
bool readSensorBlock(uint16_t startReg, float &friction, float &fatigue) {
  byte request[8];
  request[0] = 0x50;  Device ID
  request[1] = 0x03;  Read function
  request[2] = highByte(startReg);
  request[3] = lowByte(startReg);
  request[4] = 0x00;
  request[5] = 0x06;  Read exactly 6 registers

  uint16_t crc = calculateCRC(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);

  sendModbusRequest(request, 8);

   6 registers  2 bytesreg = 12 bytes + 5 bytes overhead = 17 bytes expected
  byte response[17];
  int received = readModbusResponse(response, 17, 50);  50ms timeout

  if (received = 17) {
    if (response[0] == 0x50 && response[1] == 0x03 && response[2] == 0x0C) {
      int16_t rawFriction = (response[3]  8)  response[4];
      friction = rawFriction  1000.0;

      int16_t rawFatigue = (response[13]  8)  response[14];
      fatigue = rawFatigue  1000.0;

      return true;
    }
  }
  return false;
}

 Standard Modbus RTU CRC-16 calculation
uint16_t calculateCRC(byte buf, int len) {
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos  len; pos++) {
    crc ^= (uint16_t)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc = 1;
        crc ^= 0xA001;
      } else {
        crc = 1;
      }
    }
  }
  return crc;
}

 ==========================================
 SERIAL COMMAND HANDLING (from Python over USB)
 ==========================================
void checkSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'n'  c == 'r') {
      if (serialCommand.length()  0) {
        serialCommand.trim();
        if (serialCommand == SLOW_MOTOR) {
          setVFDFrequency(SLOW_MOTOR_HZ);
          delay(20);
          setVFDRunCommand(0x0001);  0001 = Forward running
        }
        serialCommand = ;
      }
    } else {
      serialCommand += c;
    }
  }
}

 Writes the communication set-value register (0x1000) on the VFD.
 This requires F0-03 = 9 (Communication setting) on the VFD keypad,
 otherwise this register is ignored.
 freqHz is converted to a percentage of VFD_MAX_FREQ_HZ (F0-10),
 scaled to -10000..10000 as required by the register.
void setVFDFrequency(float freqHz) {
  int16_t pct = (int16_t)((freqHz  VFD_MAX_FREQ_HZ)  10000.0);

  byte request[8];
  request[0] = VFD_SLAVE_ADDR;
  request[1] = 0x06;  Write single register
  request[2] = highByte(VFD_COMM_FREQ_REG);
  request[3] = lowByte(VFD_COMM_FREQ_REG);
  request[4] = highByte(pct);
  request[5] = lowByte(pct);

  uint16_t crc = calculateCRC(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);

  sendModbusRequest(request, 8);

  byte response[8];
  readModbusResponse(response, 8, 50);
}

 Writes control word 0x2000 to command the VFD to runstop.
 Requires F0-02 = 2 (Communication control) on the VFD keypad,
 otherwise this register is ignored.
 command 0x0001 = Forward run, 0x0002 = Reverse run, 0x00050x0006 = stop variants
void setVFDRunCommand(uint16_t command) {
  byte request[8];
  request[0] = VFD_SLAVE_ADDR;
  request[1] = 0x06;
  request[2] = highByte(VFD_RUN_CMD_REG);
  request[3] = lowByte(VFD_RUN_CMD_REG);
  request[4] = highByte(command);
  request[5] = lowByte(command);

  uint16_t crc = calculateCRC(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);

  sendModbusRequest(request, 8);

  byte response[8];
  readModbusResponse(response, 8, 50);
}

 Reads U0-00 Running frequency via register 0x1001. Returns Hz as float,
 or NAN if the read failedtimed out.
float readVFDFrequency() {
  byte request[8];
  request[0] = VFD_SLAVE_ADDR;
  request[1] = 0x03;
  request[2] = highByte(VFD_RUNFREQ_REG);
  request[3] = lowByte(VFD_RUNFREQ_REG);
  request[4] = 0x00;
  request[5] = 0x01;  read 1 register

  uint16_t crc = calculateCRC(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);

  sendModbusRequest(request, 8);

   Expected response addr, func(0x03), byteCount(0x02), dataHi, dataLo, crcLo, crcHi = 7 bytes
  byte response[7];
  int received = readModbusResponse(response, 7, 50);

  if (received = 7 && response[0] == VFD_SLAVE_ADDR && response[1] == 0x03) {
    int16_t raw = (response[3]  8)  response[4];
    return raw  100.0;
  }
  return NAN;
}