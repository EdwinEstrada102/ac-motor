#include <SoftwareSerial.h>

// --- VIBRATION SENSOR CONFIGURATION ---
#define RX_PIN 10
#define TX_PIN 11
SoftwareSerial RS485Serial(RX_PIN, TX_PIN);

// --- TACHOMETER CONFIGURATION ---
const int hallPin = 2; // UNO Digital Pin 2 (Hardware Interrupt 0)
volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;
float smoothedRpm = 0.0;
const float alpha = 0.15; // Smoothing factor (0.0 to 1.0)

void setup() {
  // Hardware serial for PC/Plotter (High baud rate for smooth plotting)
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  // Software serial for RS485 Converter
  RS485Serial.begin(9600);
  
  // Initialize tachometer interrupt
  pinMode(hallPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(hallPin), pulseISR, FALLING); 
}

void loop() {
  // ==========================================
  // 1. CALCULATE RPM
  // ==========================================
  noInterrupts();
  unsigned long interval = pulseInterval;
  unsigned long lastPulse = lastPulseTime;
  interrupts();

  float rawRpm = 0.0;
  if (micros() - lastPulse > 2000000) { 
    rawRpm = 0.0;
  } else if (interval > 0) {
    rawRpm = 60000000.0 / interval; 
  }

  if (rawRpm == 0.0) {
    smoothedRpm = 0.0;
  } else {
    smoothedRpm = (alpha * rawRpm) + ((1.0 - alpha) * smoothedRpm);
  }

  // ==========================================
  // 2. FETCH VIBRATION DATA (Friction & Fatigue)
  // ==========================================
  float frictionX = 0, fatigueX = 0;
  float frictionY = 0, fatigueY = 0;
  float frictionZ = 0, fatigueZ = 0;

  // Read blocks for X, Y, and Z axes
  bool successX = readSensorBlock(0x004B, frictionX, fatigueX);
  bool successY = readSensorBlock(0x0057, frictionY, fatigueY);
  bool successZ = readSensorBlock(0x0063, frictionZ, fatigueZ);

  // ==========================================
  // 3. OUTPUT TO SERIAL PLOTTER
  // ==========================================
  // Only print if we successfully read all vibration data
  if (successX && successY && successZ) {
    Serial.print("RPM:"); Serial.print(smoothedRpm);
    
    Serial.print(", Fric_X(g):"); Serial.print(frictionX, 3);
    Serial.print(", Fatg_X(mm/s):"); Serial.print(fatigueX, 3);
    
    Serial.print(", Fric_Y(g):"); Serial.print(frictionY, 3);
    Serial.print(", Fatg_Y(mm/s):"); Serial.print(fatigueY, 3);
    
    Serial.print(", Fric_Z(g):"); Serial.print(frictionZ, 3);
    Serial.print(", Fatg_Z(mm/s):"); Serial.println(fatigueZ, 3);
    Serial.println(", Machine_State:0");;
  }
}

// ==========================================
// HELPER FUNCTIONS
// ==========================================

// Tachometer Interrupt Service Routine
void pulseISR() {
  unsigned long currentTime = micros();
  unsigned long tempInterval = currentTime - lastPulseTime;

  // 25,000 microseconds = 2,400 RPM maximum physical limit.
  // Filters out noise/bounce faster than 25ms.
  if (tempInterval > 25000) { 
    pulseInterval = tempInterval;
    lastPulseTime = currentTime;
  }
}

// Function to request 6 contiguous registers starting at the given address
bool readSensorBlock(uint16_t startReg, float &friction, float &fatigue) {
  byte request[8];
  request[0] = 0x50; // Device ID
  request[1] = 0x03; // Read function
  request[2] = highByte(startReg);
  request[3] = lowByte(startReg);
  request[4] = 0x00;
  request[5] = 0x06; // Read exactly 6 registers
  
  // Calculate and attach the Modbus CRC
  uint16_t crc = calculateCRC(request, 6);
  request[6] = lowByte(crc);
  request[7] = highByte(crc);
  
  // Clear any old data lingering in the buffer
  while(RS485Serial.available()) {
    RS485Serial.read();
  }
  
  // Send request and wait for the sensor to reply
  RS485Serial.write(request, 8);
  
  // Small delay to allow the sensor to reply (replaces the tachometer delay)
  delay(30); 
  
  // 6 registers * 2 bytes/reg = 12 bytes + 5 bytes overhead = 17 bytes expected
  if (RS485Serial.available() >= 17) {
    byte response[17];
    for (int i = 0; i < 17; i++) {
      response[i] = RS485Serial.read();
    }
    
    // Verify it is a valid Modbus response from our sensor (0x50)
    if (response[0] == 0x50 && response[1] == 0x03 && response[2] == 0x0C) {
      
      // Friction (a-RMS) is the 1st register in this block (Bytes 3 & 4)
      int16_t rawFriction = (response[3] << 8) | response[4];
      friction = rawFriction / 1000.0; 
      
      // Fatigue (v-RMS) is the 6th register in this block (Bytes 13 & 14)
      int16_t rawFatigue = (response[13] << 8) | response[14];
      fatigue = rawFatigue / 1000.0;
      
      return true;
    }
  }
  return false;
}

// Standard Modbus RTU CRC-16 calculation
uint16_t calculateCRC(byte *buf, int len) {
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}