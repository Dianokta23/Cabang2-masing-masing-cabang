#define BLYNK_TEMPLATE_ID "TMPL63elZ23Za"
#define BLYNK_TEMPLATE_NAME "Pengukuran dan analisis debit air"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Konfigurasi Blynk
char auth[] = "unUI6LbkExTDugMbk6eJFCuz94Mh7N_N";
char ssid[] = "Ndk Modal";
char pass[] = "12345678";

// Pin untuk Sensor Aliran Air
#define FLOW_SENSOR_PIN3 D5  // Sensor 3
#define FLOW_SENSOR_PIN4 D6  // Sensor 4

// Inisialisasi LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Alamat I2C LCD mungkin berbeda

volatile int flowPulseCount3 = 0;
volatile int flowPulseCount4 = 0;

// Koefisien kalibrasi sensor aliran
const float calibrationFactor = 7.5;  // Pulsa per liter

unsigned long previousMillis = 0; // Variabel untuk menghitung waktu
float totalVolumeSensor4_m3 = 0.0;    // Variabel untuk menyimpan total volume sensor 4 (m³)
float totalBiaya = 0.0;               // Variabel untuk menyimpan total biaya

// Variabel global untuk status kebocoran dan tingkat kebocoran
String statusKebocoran = "Aman";
String tingkatKebocoran = "Tidak ada";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);  // Waktu UTC+7 (25200 detik offset)

// ISR untuk menangkap pulsa dari sensor flow
void ICACHE_RAM_ATTR flowPulseISR3() { flowPulseCount3++; }
void ICACHE_RAM_ATTR flowPulseISR4() { flowPulseCount4++; }

void setup() {
  Serial.begin(9600);
  delay(100);

  Serial.println("Mulai setup...");
  Blynk.begin(auth, ssid, pass);

  lcd.init();
  lcd.backlight();
  Serial.println("LCD diinisialisasi...");

  pinMode(FLOW_SENSOR_PIN3, INPUT);
  pinMode(FLOW_SENSOR_PIN4, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN3), flowPulseISR3, RISING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN4), flowPulseISR4, RISING);

  Serial.println("Interrupts diinisialisasi...");

  timeClient.begin();  // Mulai client NTP
}

// Fungsi untuk menghitung biaya berdasarkan volume air
float hitungBiaya(float totalVolumeSensor4_m3) {
  float biaya = 0;
  if (totalVolumeSensor4_m3 <= 10) {
    biaya = totalVolumeSensor4_m3 * 210;  // per m³
  } else if (totalVolumeSensor4_m3 <= 20) {
    biaya = (10 * 210) + ((totalVolumeSensor4_m3 - 10) * 310);
  } else if (totalVolumeSensor4_m3 <= 30) {
    biaya = (10 * 210) + (10 * 310) + ((totalVolumeSensor4_m3 - 20) * 450);
  } else {
    biaya = (10 * 210) + (10 * 310) + (10 * 450) + ((totalVolumeSensor4_m3 - 30) * 630);
  }
  return biaya;
}

// Fungsi untuk menghitung dan menampilkan status kebocoran
void tampilkanStatusKebocoran(float flowRate3, float flowRate4) {
  float toleransi = 0.05 * flowRate3;
  float selisihDebit = abs(flowRate3 - flowRate4);

  statusKebocoran = "Aman";
  tingkatKebocoran = "Tidak ada";

  if (selisihDebit > toleransi) {
    if (selisihDebit >= 0.010) {
      tingkatKebocoran = "Besar";
    } else if (selisihDebit >= 0.009) {
      tingkatKebocoran = "Sedang";
    } else if (selisihDebit >= 0.005) {
      tingkatKebocoran = "Kecil";
    }
    statusKebocoran = "Bocor cabang2";
  }

  Serial.print("Status Kebocoran: ");
  Serial.println(statusKebocoran);
  Serial.print("Tingkat Kebocoran: ");
  Serial.println(tingkatKebocoran);

  Blynk.virtualWrite(V3, statusKebocoran);
  Blynk.virtualWrite(V4, tingkatKebocoran);

  // Tampilkan status dan tingkat kebocoran di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sts: ");
  lcd.print(statusKebocoran);
  lcd.setCursor(0, 1);
  lcd.print("Tingkat: ");
  lcd.print(tingkatKebocoran);
}

void loop() {
  Blynk.run();

  unsigned long currentMillis = millis();
  unsigned long interval = 1000;  // 1 detik

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Hentikan interrupt saat membaca pulsa
    noInterrupts();
    int pulseCount3 = flowPulseCount3;
    int pulseCount4 = flowPulseCount4;
    flowPulseCount3 = 0;
    flowPulseCount4 = 0;
    interrupts();

    // Hitung volume air dalam liter, kemudian konversi ke m³
    float volume3_m3 = (pulseCount3 / calibrationFactor) / 1000.0;  // Konversi ke m³
    float volume4_m3 = (pulseCount4 / calibrationFactor) / 1000.0;  // Konversi ke m³

    // Tambahkan volume ke total volume sensor 4
    totalVolumeSensor4_m3 += volume4_m3;

    // Hitung biaya berdasarkan total volume air yang terpakai
    totalBiaya = hitungBiaya(totalVolumeSensor4_m3);

    // **Perhitungan Debit Air** dalam m³/jam
    float debitAir3 = ((pulseCount3 / calibrationFactor) * 0.001) * 60;  // Debit dalam m³/jam
    float debitAir4 = ((pulseCount4 / calibrationFactor) * 0.001) * 60;  // Debit dalam m³/jam

    // Kirim data debit air ke Blynk
    Blynk.virtualWrite(V0, String(debitAir3, 3));  // Debit air sensor 3 (m³/jam)
    Blynk.virtualWrite(V1, String(debitAir4, 3));  // Debit air sensor 4 (m³/jam)
    
    // Kirim data biaya ke Blynk
    Blynk.virtualWrite(V2, String(totalBiaya, 3));  // Biaya total

    // Tampilkan volume dalam m³ di Serial Monitor dengan 3 angka di belakang koma
    Serial.print("Sensor 3: Volume = ");
    Serial.print(String(volume3_m3, 3));
    Serial.println(" m³");

    Serial.print("Sensor 4: Volume = ");
    Serial.print(String(volume4_m3, 3));
    Serial.println(" m³");

    Serial.print("Biaya Total: ");
    Serial.print(String(totalBiaya, 3));
    Serial.println(" Rupiah");

    // **Tampilkan Debit Air** di Serial Monitor dengan 3 angka di belakang koma
    Serial.print("Sensor 3: Debit = ");
    Serial.print(String(debitAir3, 3));
    Serial.println(" m³/jam");

    Serial.print("Sensor 4: Debit = ");
    Serial.print(String(debitAir4, 3));
    Serial.println(" m³/jam");

    // Tampilkan status kebocoran
    tampilkanStatusKebocoran(debitAir3, debitAir4);

    // Tampilkan waktu (hanya jam) di Serial Monitor
    timeClient.update();
    Serial.print("Waktu (jam): ");
    Serial.println(timeClient.getFormattedTime());

    // **Tampilkan data pada LCD**
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("D3:");
    lcd.print(String(debitAir3, 3));
    lcd.print("m3/j");

    lcd.setCursor(0, 1);
    lcd.print("D4:");
    lcd.print(String(debitAir4, 3));
    lcd.print("m3/j");

    delay(2000); // Tunda 2 detik untuk menampilkan debit

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Biaya: ");
    lcd.print(String(totalBiaya, 3));

    lcd.setCursor(0, 1);
    lcd.print("Sts: ");
    lcd.print(statusKebocoran);

    lcd.setCursor(0, 1);
    lcd.print("Tkgt: ");
    lcd.print(tingkatKebocoran);
  }
}
