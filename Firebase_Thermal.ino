#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_MLX90614.h>
#include <Wire.h>
#include "Adafruit_Thermal.h"
#include <FirebaseESP32.h>
#include "time.h"
// #define FIREBASE_HOST "https://dht11-with-esp32-31002-default-rtdb.firebaseio.com/"
// #define FIREBASE_AUTH "YuYSbyytavwTFpO0jZ9bMN2EVo5KBJdpK4AeaL3Y"
// Link database
#define FIREBASE_HOST "https://print-suhu-tubuh-default-rtdb.asia-southeast1.firebasedatabase.app/"
// API Key database
#define FIREBASE_AUTH "AIzaSyD6OJTHib5F8mwulbtlb_cIK4DbPipMkas"
FirebaseData firebaseData;
FirebaseJson json;
unsigned long lastResetTimestamp = 0;
int antrianTerakhir = 0;
int tanggalTerakhir = 0;

Adafruit_Thermal printer(&Serial2);  // Or Serial2, Serial3, etc.

// Replace with your network credentials
const char* ssid = "Hotspot";       // nama wifi
const char* password = "11111111";  //pasword

WiFiClientSecure client;

int batassuhu = 38;
bool ledState = LOW;
//============== hc 04 ================
const int trigPinSuhu = 4;
const int echoPinSuhu = 2;
long durationSuhu;
float distanceCmSuhu;
float distanceInchSuhu;
#define SOUND_VELOCITY 0.034
#define CM_TO_INCH 0.393701
//============== suhu ===================
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
float suhuterbaca;
String stringOne;
//============== lcd ====================
LiquidCrystal_I2C lcd(0x27, 16, 2);
uint8_t simbol = 223;  //simbol derajat untuk lcd
//============== button =================
const int buttonPinPasien = 12;
const int buttonPinPengunjung = 13;  // the number of the pushbutton pin

// variables will change:
int buttonStatePasien = 0;  // variable for reading the pushbutton status
int buttonStatePengunjung = 0;
int antrianPasien = 1;
int antrianPengunjung = 1;
bool isPasien = false;
bool isPengunjung = false;
//============== waktu ==================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

struct tm timeinfo;
String tanggalGlobal;  // Variabel global untuk menyimpan data waktu
String tanggalBaru;

//=============== motor ==================
const int EN2 = 26;
const int IN3 = 25;
const int IN4 = 33;

//=============== limit switch ==================
const int limitSwitchPin1 = 14;  // Pin limit switch 1
const int limitSwitchPin2 = 27;  // Pin limit switch 2

void setup() {
  pinMode(buttonPinPasien, INPUT_PULLUP);
  pinMode(buttonPinPengunjung, INPUT_PULLUP);
  pinMode(trigPinSuhu, OUTPUT);  // Sets the trigPin as an Output
  pinMode(echoPinSuhu, INPUT);   // Sets the echoPin as an Input
  pinMode(EN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(limitSwitchPin1, INPUT_PULLUP);
  pinMode(limitSwitchPin2, INPUT_PULLUP);
  Serial.begin(9600);
  Serial.begin(115200);
  Wire.begin(SDA, SCL);
  Serial2.begin(9600);  // Initialize SoftwareSerial
  printer.begin();      // Init printer (same regardless of serial type)
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistem aktif");
  delay(3000);
  mlx.begin();

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    // Serial.println("Connecting to WiFi..");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting to");
    lcd.setCursor(0, 1);
    lcd.print("WiFi");
  }
  tekanTombol();
  Serial.println(WiFi.localIP());
  // Connect to database
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

void loop() {
  jarakSuhu();
  suhu();
  monitoring();
  // read the state of the pushbutton value:
  buttonStatePasien = digitalRead(buttonPinPasien);
  buttonStatePengunjung = digitalRead(buttonPinPengunjung);
  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (digitalRead(limitSwitchPin1) == LOW) {
    Serial.println("Limit Switch 1 ditekan");
    stopMotor();  // Berhenti motor
    delay(1000);
    closeDoor();  // Menutup pintu
  }
  if (digitalRead(limitSwitchPin2) == LOW) {
    Serial.println("Limit Switch 2 ditekan");
    stopMotor();
  }
  if (buttonStatePasien == LOW) {
    isPasien = true;
    Serial.println(antrianPasien);
    Serial.println("pasien");
    // antrianPasien++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pasien");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("letakan tangan");
    lcd.setCursor(0, 1);
    lcd.print("di sensor");
  }
  if (buttonStatePengunjung == LOW) {
    isPengunjung = true;
    Serial.println(antrianPengunjung);
    Serial.println("pengunjung");
    // antrianPengunjung++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pengunjung");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("letakan tangan");
    lcd.setCursor(0, 1);
    lcd.print("di sensor");
  }
  if (distanceCmSuhu < 8 and distanceCmSuhu != 0) {
    stringOne = String(suhuterbaca);
    if (isPasien == true) {
      FirebaseJson pasienData;
      pasienData.set("antrianPasien", antrianPasien);
      pasienData.set("suhu", suhuterbaca);
      String currentDate = getFormattedDate(timeinfo);

      // Check if the currentDate exists in the Firebase node
      if (Firebase.getJSON(firebaseData, "/data_pasien/" + currentDate)) {
        // Parse the existing JSON data as a FirebaseJsonArray
        FirebaseJsonArray* existingData = firebaseData.jsonArrayPtr();

        // Add the new pasienData to the existing array
        existingData->add(pasienData);

        // Update the Firebase node with the updated array
        if (Firebase.setArray(firebaseData, "/data_pasien/" + currentDate, *existingData)) {
          Serial.println("Data pasien berhasil diupdate");
        } else {
          Serial.println("Gagal mengupdate data pasien");
        }
      } else {
        // Create a new FirebaseJsonArray and add the pasienData
        FirebaseJsonArray newData;
        newData.add(pasienData);

        // Set the new array in the Firebase node for the currentDate
        if (Firebase.setArray(firebaseData, "/data_pasien/" + currentDate, newData)) {
          Serial.println("Data pasien berhasil diupdate");
        } else {
          Serial.println("Gagal mengupdate data pasien");
        }
      }
      antrianPasien++;
      printTiket();
      lcd.clear();
      lcd.print(" SUHU : ");
      lcd.print(suhuterbaca, 1);
      lcd.write(simbol);
      lcd.print("C");
      isPasien = false;
      openDoor();
      delay(1500);
      tekanTombol();
    }
    if (isPengunjung == true) {
      FirebaseJson pengunjungData;
      pengunjungData.set("antrianPengunjung", antrianPengunjung);
      pengunjungData.set("suhu", suhuterbaca);
      String currentDate = getFormattedDate(timeinfo);

      if (Firebase.getJSON(firebaseData, "/data_pengunjung/" + currentDate)) {
        // Parse the existing JSON data as a FirebaseJsonArray
        FirebaseJsonArray* existingData = firebaseData.jsonArrayPtr();

        // Add the new pengunjungData to the existing array
        existingData->add(pengunjungData);

        // Update the Firebase node with the updated array
        if (Firebase.setArray(firebaseData, "/data_pengunjung/" + currentDate, *existingData)) {
          Serial.println("Data pengunjung berhasil diupdate");
        } else {
          Serial.println("Gagal mengupdate data pengunjung");
        }
      } else {
        // Create a new FirebaseJsonArray and add the pengunjungData
        FirebaseJsonArray newData;
        newData.add(pengunjungData);

        // Set the new array in the Firebase node for the currentDate
        if (Firebase.setArray(firebaseData, "/data_pengunjung/" + currentDate, newData)) {
          Serial.println("Data pengunjung berhasil diupdate");
        } else {
          Serial.println("Gagal mengupdate data pengunjung");
        }
      }
      antrianPengunjung++;
    }
    if (isPengunjung == true && suhuterbaca > batassuhu) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Tidak boleh masuk");
      lcd.setCursor(0, 1);
      lcd.print(" SUHU : ");
      lcd.print(suhuterbaca, 1);
      lcd.write(simbol);
      lcd.print("C");
      Serial.println(suhuterbaca);
      isPengunjung = false;
      delay(3000);
      tekanTombol();
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Diizinkan masuk");
      lcd.setCursor(0, 1);
      lcd.print(" SUHU : ");
      lcd.print(suhuterbaca, 1);
      lcd.write(simbol);
      lcd.print("C");
      Serial.println(suhuterbaca);
      isPengunjung = false;
      openDoor();
      delay(1500);
      tekanTombol();
      Serial.println(suhuterbaca);
    }
    delay(1000);
  }
  delay(500);
}

String getFormattedDate(struct tm timeinfo) {
  char dateBuffer[11];
  sprintf(dateBuffer, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return String(dateBuffer);
}

void jarakSuhu() {
  digitalWrite(trigPinSuhu, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPinSuhu, HIGH);  // Sets the trigPin on HIGH state for 10 micro seconds
  delayMicroseconds(10);
  digitalWrite(trigPinSuhu, LOW);
  durationSuhu = pulseIn(echoPinSuhu, HIGH);           // Reads the echoPin, returns the sound wave travel time in microseconds
  distanceCmSuhu = durationSuhu * SOUND_VELOCITY / 2;  // Calculate the distance
  distanceInchSuhu = distanceCmSuhu * CM_TO_INCH;      // Convert to inches
  if (distanceCmSuhu > 100) {
    distanceCmSuhu = 100;
  }
}

void suhu() {
  suhuterbaca = mlx.readObjectTempC();
  if (suhuterbaca > 100) suhuterbaca = 100;
}

void monitoring() {
  Serial.println("Suhu & Jarak");
  Serial.print(suhuterbaca);
  Serial.print("*C   ");
  Serial.print(distanceCmSuhu);
  Serial.print("cm ");
  Serial.println();
  delay(2000);
}

void printLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  // Simpan data waktu ke variabel global
  int year = timeinfo.tm_year + 1900;  // Tahun
  int month = timeinfo.tm_mon + 1;     // Bulan
  int day = timeinfo.tm_mday;          // Tanggal
  int weekday = timeinfo.tm_wday;      // Hari dalam bentuk angka (0 = Minggu, 1 = Senin, dst.)

  // Lakukan operasi atau tindakan lain dengan data waktu di sini

  String hari;

  switch (weekday) {
    case 0:
      hari = "Minggu";
      break;
    case 1:
      hari = "Senin";
      break;
    case 2:
      hari = "Selasa";
      break;
    case 3:
      hari = "Rabu";
      break;
    case 4:
      hari = "Kamis";
      break;
    case 5:
      hari = "Jumat";
      break;
    case 6:
      hari = "Sabtu";
      break;
    default:
      hari = "Tidak diketahui";
      break;
  }

  const char* namaBulan[] = {
    "Januari", "Februari", "Maret", "April", "Mei", "Juni",
    "Juli", "Agustus", "September", "Oktober", "November", "Desember"
  };

  String bulan = namaBulan[month - 1];
  // Contoh: Menyimpan data waktu dalam variabel global
  tanggalBaru = hari + "," + String(day) + " " + bulan + " " + String(year);
  if (tanggalBaru != tanggalGlobal) {
    tanggalGlobal = tanggalBaru;
    Serial.println(tanggalGlobal);
  }
}

void printTiket() {
  printer.setLineHeight(10);
  printer.justify('C');
  printer.setSize('M');
  printer.println(F("RS BERKAITAN"));
  printer.println(F("\n"));

  printer.justify('L');
  printer.setSize('S');
  printer.print(F("suhu terbaca : "));
  printer.print(suhuterbaca, 1);
  printer.println(F(" C"));

  printer.justify('L');
  printer.setSize('S');
  printer.println(F("No Pengunjung ke : "));
  printer.println(F("\n"));
  printer.justify('C');
  printer.setSize('L');
  printer.doubleHeightOn();
  printer.underlineOn();
  printer.print(antrianPasien);
  printer.underlineOff();
  printer.doubleHeightOn();
  printer.println(F("\n\n\n\n\n\n"));
  Serial2.println(antrianPasien);
}

void openDoor() {
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(EN2, 70);
}

// Fungsi untuk menggerakkan motor ke arah berlawanan (menutup pintu)
void closeDoor() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(EN2, 70);
}

// Fungsi untuk menghentikan motor
void stopMotor() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(EN2, 0);
}

void tekanTombol() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tekan salah");
  lcd.setCursor(0, 1);
  lcd.print("satu tombol");
}