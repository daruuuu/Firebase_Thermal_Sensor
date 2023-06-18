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
const int trigPinPintu = 14;
const int echoPinPintu = 27;
long durationPintu;
float distanceCmPintu;
float distanceInchPintu;
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

void setup() {
  pinMode(buttonPinPasien, INPUT_PULLUP);
  pinMode(buttonPinPengunjung, INPUT_PULLUP);
  pinMode(trigPinSuhu, OUTPUT);   // Sets the trigPin as an Output
  pinMode(echoPinSuhu, INPUT);    // Sets the echoPin as an Input
  pinMode(trigPinPintu, OUTPUT);  // Sets the trigPin as an Output
  pinMode(echoPinPintu, INPUT);   // Sets the echoPin as an Input
  pinMode(EN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  Serial.begin(9600);
  Serial.begin(115200);
  Wire.begin(SDA, SCL);
  Serial2.begin(9600);  // Initialize SoftwareSerial
  printer.begin();      // Init printer (same regardless of serial type)
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("sistem aktif");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("tekan salah");
  lcd.setCursor(0, 1);
  lcd.print("satu tombol");
  //  if (!mlx.begin()) {
  //    Serial.println("Error connecting to MLX sensor. Check wiring.");
  //    while (1);
  //  }
  mlx.begin();

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());
  // Connect to database
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

void loop() {
  jarakPintu();
  jarakSuhu();
  suhu();
  monitoring();
  // read the state of the pushbutton value:
  buttonStatePasien = digitalRead(buttonPinPasien);
  buttonStatePengunjung = digitalRead(buttonPinPengunjung);
  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonStatePasien == LOW) {
    isPasien = true;
    Serial.println(antrianPasien);
    Serial.println("pasien");
    antrianPasien++;
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
    antrianPengunjung++;
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
      json.clear();
      json.set("tanggal", getFormattedDate(timeinfo));
      json.set("jumlah_pasien", antrianPasien);
      json.set("pasien", suhuterbaca);

      // Update the Firebase node for the current date
      if (Firebase.updateNode(firebaseData, "/data_pasien/" + String(antrianPasien - 1), json)) {
        Serial.println("Data pasien berhasil diupdate");
      } else {
        Serial.println("Gagal mengupdate data pasien");
      }
      lcd.clear();
      lcd.print(" SUHU : ");
      lcd.print(suhuterbaca, 1);
      lcd.write(simbol);
      lcd.print("C");
      isPasien = false;
      if (distanceCmPintu < 8) {
        stopMotor();  // Berhenti motor
        closeDoor();  // Menutup pintu
      } else {
        // Jika jarak lebih dari atau sama dengan 8 cm, gerakkan motor ke satu sisi
        openDoor();
      }
      delay(5000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("tekan salah");
      lcd.setCursor(0, 1);
      lcd.print("satu tombol");
    }

    if (isPengunjung == true) {
      json.clear();
      json.set("tanggal", getFormattedDate(timeinfo));
      json.set("jumlah_pengunjung", antrianPengunjung);
      json.set("pengunjung", suhuterbaca);

      // Update the Firebase node for the current date
      if (Firebase.updateNode(firebaseData, "/data_pengunjung/" + String(antrianPengunjung - 1), json)) {
        Serial.println("Data pengunjung berhasil diupdate");
      } else {
        Serial.println("Gagal mengupdate data pengunjung");
      }
      // Cek apakah suhu diizinkan
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
        delay(5000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("tekan salah");
        lcd.setCursor(0, 1);
        lcd.print("satu tombol");
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
        // Buka pintu
        if (distanceCmPintu < 8) {
          stopMotor();  // Berhenti motor
          closeDoor();  // Menutup pintu
        } else {
          // Jika jarak lebih dari atau sama dengan 8 cm, gerakkan motor ke satu sisi
          openDoor();
        }
        delay(5000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("tekan salah");
        lcd.setCursor(0, 1);
        lcd.print("satu tombol");
        Serial.println(suhuterbaca);
      }
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

void jarakPintu() {
  digitalWrite(trigPinPintu, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPinPintu, HIGH);  // Sets the trigPin on HIGH state for 10 micro seconds
  delayMicroseconds(10);
  digitalWrite(trigPinPintu, LOW);
  durationPintu = pulseIn(echoPinPintu, HIGH);           // Reads the echoPin, returns the sound wave travel time in microseconds
  distanceCmPintu = durationPintu * SOUND_VELOCITY / 2;  // Calculate the distance
  distanceInchPintu = distanceCmPintu * CM_TO_INCH;      // Convert to inches
  if (distanceCmPintu > 100) {
    distanceCmPintu = 100;
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
  Serial.println("Jarak Pintu");
  Serial.print(distanceCmPintu);
  Serial.print("cm ");
  Serial.println();
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

void printtiket() {
  printer.setLineHeight(10);
  printer.justify('C');
  printer.setSize('M');
  printer.println(F("RS BERKAITAN"));
  printer.println(F("\n"));

  printer.justify('L');
  printer.setSize('S');
  printer.boldOn();
  printer.print(F("No Pengunjung ke : "));
  printer.print(antrianPasien);
  Serial2.println(antrianPasien);

  printer.justify('L');
  printer.setSize('S');
  printer.print(F("suhu terbaca : "));
  printer.print(suhuterbaca, 1);
  printer.println(F(" C"));

  printer.println(F("\n\n\n\n\n\n"));
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
  delay(2000);  // Ubah delay sesuai dengan waktu yang dibutuhkan untuk menutup pintu
  stopMotor();  // Berhenti motor setelah pintu ditutup
}

void pintu() {
  jarakPintu();
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(EN2, 70);

  // Cek jarak selama pintu dibuka
  if (distanceCmPintu < 8) {
    // Pintu mencapai jarak kurang dari 5cm, hentikan motor
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    // analogWrite(EN2, 0);
    digitalWrite(EN2, LOW);

    // Tunggu 5 detik sebelum memutar motor ke arah sebaliknya
    delay(5000);

    // Putar motor ke arah sebaliknya
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    analogWrite(EN2, 70);

    // Cek jarak selama motor berputar
    if (distanceCmPintu >= 10) {
      // Pintu mencapai atau melebihi jarak 5cm, hentikan motor
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      analogWrite(EN2, 0);
    }
    delay(100);  // Tunggu 100ms sebelum memeriksa kembali jarak
  }

  delay(100);  // Tunggu 100ms sebelum memeriksa kembali jarak
}