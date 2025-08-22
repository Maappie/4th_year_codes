#include <SPI.h>
#include <SD.h>
#include "DHT.h"

// -------- Pins --------
#define DHTPIN    4       // DHT22 data (moved off D2 to free interrupt)
#define DHTTYPE   DHT11
#define PIRPIN    3       // PIR digital output
#define CSPIN     10      // SD chip select
#define BUTTONPIN 2       // Interrupt button (falling -> dump logs)
#define LIGHTPIN  A0      // Analog light sensor AO

DHT dht(DHTPIN, DHTTYPE);
File dataFile;

volatile bool dumpRequested = false;

void onButtonFall() {
  dumpRequested = true;   // ISR: just set a flag
}

static void dumpLogFile(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.println("Log file not found or open failed.");
    return;
  }
  Serial.println("---- BEGIN envlog.csv ----");
  const size_t BUF = 64;
  char buf[BUF + 1];
  while (true) {
    int n = f.readBytes(buf, BUF);
    if (n <= 0) break;
    buf[n] = '\0';
    Serial.print(buf);
  }
  Serial.println("\n---- END envlog.csv ----");
  f.close();
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(LIGHTPIN, INPUT);

  // Button: internal pull-up; press connects to GND (falling edge)
  pinMode(BUTTONPIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTONPIN), onButtonFall, FALLING);

  // Keep SPI in master mode & init SD
  pinMode(10, OUTPUT);

  while (!SD.begin(CSPIN)) {
    Serial.println("SD Card initialization failed! Retrying.");
    delay(1000);
  }
  Serial.println("SD Card ready.");
}

void loop() {
  // Read sensors
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int   lightRaw = analogRead(LIGHTPIN);   // 0..1023 raw ADC
  bool  motion   = digitalRead(PIRPIN);

  // Log CSV line: temp,humidity,lightRaw,motion
  dataFile = SD.open("envlog.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.print(temp);     dataFile.print(",");
    dataFile.print(humidity); dataFile.print(",");
    dataFile.print(lightRaw); dataFile.print(",");
    dataFile.println(motion ? "1" : "0");
    dataFile.close();
    Serial.println("Data logged.");
  } else {
    Serial.println("Error opening file.");
  }

  // If button asked for a dump, do it after closing the file
  if (dumpRequested) {
    dumpRequested = false;
    dumpLogFile("envlog.csv");
  }

  delay(5000); // Log every 5 seconds
}
