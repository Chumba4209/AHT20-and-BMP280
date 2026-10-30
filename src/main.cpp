#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>

#define SDA_PIN 8
#define SCL_PIN 10
#define OLED_ADDR 0x3C
#define AHT_ADDR 0x38

Adafruit_BMP280 bmp(&Wire);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

bool ahtOk = false;
bool bmpOk = false;
bool oledOk = false;

float ahtTemp = NAN, ahtHum = NAN;
float bmpTemp = NAN, bmpPres = NAN, bmpAlt = NAN;

void scan(const char *label) {
  Serial.printf("[%s] scan: ", label);
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("0x%02X ", a); n++; }
  }
  if (!n) Serial.print("nothing");
  Serial.println();
}

// ---- AHT20, raw reads (no library) ----
bool initAHT() {
  Wire.beginTransmission(AHT_ADDR);
  Wire.write(0xBA);                        // soft reset
  if (Wire.endTransmission() != 0) return false;
  delay(50);

  Wire.beginTransmission(AHT_ADDR);
  Wire.write(0xBE);                        // init / calibrate
  Wire.write(0x08);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(50);

  if (Wire.requestFrom((uint8_t)AHT_ADDR, (uint8_t)1) != 1) return false;
  uint8_t s = Wire.read();
  Serial.printf("AHT status byte: 0x%02X (calibrated bit %s)\n",
                s, (s & 0x08) ? "set" : "NOT set");
  return true;
}

bool readAHT() {
  Wire.beginTransmission(AHT_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) return false;

  delay(100);                              // measurement takes ~80 ms

  if (Wire.requestFrom((uint8_t)AHT_ADDR, (uint8_t)7) != 7) return false;
  uint8_t d[7];
  for (int i = 0; i < 7; i++) d[i] = Wire.read();
  if (d[0] & 0x80) return false;           // still busy

  uint32_t rawH = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t rawT = ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
  ahtHum  = rawH * 100.0f / 1048576.0f;
  ahtTemp = rawT * 200.0f / 1048576.0f - 50.0f;
  return true;
}

// ---- BMP280: try both possible addresses ----
bool initBMP() {
  const uint8_t addrs[] = {0x76, 0x77};
  for (uint8_t a : addrs) {
    if (bmp.begin(a)) {
      Serial.printf("BMP280 found at 0x%02X\n", a);
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                      Adafruit_BMP280::SAMPLING_X2,    // temperature
                      Adafruit_BMP280::SAMPLING_X16,   // pressure
                      Adafruit_BMP280::FILTER_X16,
                      Adafruit_BMP280::STANDBY_MS_500);
      return true;
    }
  }
  Serial.println("BMP280 not found at 0x76 or 0x77");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(3000);                             // lets the USB serial monitor connect

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(200);

  scan("start");

  ahtOk = initAHT();
  Serial.println(ahtOk ? "AHT20 OK" : "AHT20 FAILED");

  bmpOk = initBMP();
  Serial.println(bmpOk ? "BMP280 OK" : "BMP280 FAILED");

  scan("after sensor init");

  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.println(oledOk ? "OLED OK" : "OLED FAILED");
  if (oledOk) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(ahtOk ? "AHT20 : OK" : "AHT20 : FAIL");
    display.println(bmpOk ? "BMP280: OK" : "BMP280: FAIL");
    display.display();
  }
  scan("after OLED init");
  delay(1500);
}

void loop() {
  delay(2000);

  bool ahtRead = ahtOk && readAHT();
  if (bmpOk) {
    bmpTemp = bmp.readTemperature();
    bmpPres = bmp.readPressure() / 100.0f;     // hPa
    bmpAlt  = bmp.readAltitude(1013.25);       // rough, assumes sea-level 1013.25 hPa
  }

  if (ahtRead) Serial.printf("AHT20  T: %.1f C | H: %.1f %%\n", ahtTemp, ahtHum);
  else         Serial.println("AHT20 read failed");

  if (bmpOk)   Serial.printf("BMP280 T: %.1f C | P: %.1f hPa | Alt: %.0f m\n",
                             bmpTemp, bmpPres, bmpAlt);
  else         Serial.println("BMP280 not available");

  if (oledOk) {
    display.clearDisplay();
    display.setCursor(0, 0);
    if (ahtRead) {
      display.printf("AHT T: %.1f C", ahtTemp);
      display.setCursor(0, 16);  display.printf("Humid: %.1f %%", ahtHum);
    } else {
      display.println("AHT20 read failed");
    }
    display.setCursor(0, 30);
    if (bmpOk) {
      display.printf("BMP T: %.1f C", bmpTemp);
      display.setCursor(0, 45);  display.printf("Pres : %.1f hPa", bmpPres);
    } else {
      display.println("BMP280 not found");
    }
    display.display();
  }
}