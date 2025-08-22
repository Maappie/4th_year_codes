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

  pinMode(BUTTONPIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTONPIN), onButtonFall, FALLING);

  pinMode(10, OUTPUT);

  while (!SD.begin(CSPIN)) {
    Serial.println("SD Card initialization failed! Retrying.");
    delay(1000);
  }
  Serial.println("SD Card ready.");
}

void loop() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int   lightRaw = analogRead(LIGHTPIN);  
  bool  motion   = digitalRead(PIRPIN);

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

  if (dumpRequested) {
    dumpRequested = false;
    dumpLogFile("envlog.csv");
  }

  delay(5000); 
}
