#define BLYNK_TEMPLATE_ID   "TMPL6W8yjc9Cr"
#define BLYNK_TEMPLATE_NAME "gara"
#define BLYNK_AUTH_TOKEN    "PWAM_X0S2Wsr9ll1E3AYsLiiQ8w-mFUU"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "pp";
char pass[] = "20022004";

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

#define TRIG_PIN      5
#define ECHO_PIN      18
#define MQ2_PIN       34
#define SERVO_PIN     19
#define BUZZER_PIN    17
#define BUTTON_PIN    16
#define FAN_RELAY_PIN 26

// Mức logic để BẬT. Nếu relay/còi kích mức thấp thì đổi HIGH <-> LOW ở đây
#define RELAY_ON  HIGH
#define RELAY_OFF LOW
#define BUZZER_ON  HIGH
#define BUZZER_OFF LOW

#define DIST_CAR_PRESENT 180    // cm: nhỏ hơn hoặc bằng = có xe
#define GAS_ON_TH        2000   // bật quạt khi MQ2 lớn hơn
#define GAS_OFF_TH       1499   // tắt quạt khi MQ2 nhỏ hơn
#define GAS_WARMUP_MS    20000UL
#define THEFT_CONFIRM    2      // số lần đo liên tiếp (mỗi 1s) mất xe mới báo
#define GAS_SEND_MS      1000UL // gửi giá trị khí ga lên Blynk mỗi 1s (tiết kiệm message)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo doorServo;
BlynkTimer timer;

bool oledAvailable = false;
bool isAntiTheftActive = false;
bool isAlarmTriggered = false;
bool alertSent = false;          // đã gửi thông báo Blynk cho lần báo trộm này chưa
bool isDoorOpen = false;
bool isFanOn = false;
bool carPresent = false;

int missCount = 0;
int lastFanSent = -1;            // -1 = chưa gửi, buộc gửi lần đầu / sau khi kết nối lại
int lastAlarmSent = -1;
unsigned long lastGasSend = 0;
int mq2Value = 0;
long distanceCm = 0;

long readDistanceOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return 999;
  return duration * 0.0343 / 2;
}

// Lấy giá trị giữa của 3 lần đo để lọc nhiễu
long readDistance() {
  long a = readDistanceOnce(); delay(15);
  long b = readDistanceOnce(); delay(15);
  long c = readDistanceOnce();
  if ((a >= b && a <= c) || (a <= b && a >= c)) return a;
  if ((b >= a && b <= c) || (b <= a && b >= c)) return b;
  return c;
}

void setDoor(bool open) {
  isDoorOpen = open;
  doorServo.write(open ? 90 : 0);
}

// V0: mở/đóng cửa
BLYNK_WRITE(V0) {
  setDoor(param.asInt() == 1);
  Serial.println(isDoorOpen ? "[BLYNK] Mo cua" : "[BLYNK] Dong cua");
}

// V1: bật/tắt chống trộm (chỉ bật được khi có xe)
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    distanceCm = readDistance();           // đo mới ngay lúc bấm
    carPresent = (distanceCm <= DIST_CAR_PRESENT);
    if (carPresent) {
      isAntiTheftActive = true;
      isAlarmTriggered = false;
      alertSent = false;
      missCount = 0;
      Serial.println("[BLYNK] Bat chong trom");
    } else {
      isAntiTheftActive = false;
      Blynk.virtualWrite(V1, 0);           // nhả công tắc về OFF
      Serial.println("[BLYNK] Tu choi: khong co xe");
    }
  } else {
    isAntiTheftActive = false;
    isAlarmTriggered = false;
    alertSent = false;
    missCount = 0;
    lastAlarmSent = -1;                    // đèn báo trộm sẽ được cập nhật ở lần gửi kế tiếp
    Serial.println("[BLYNK] Tat chong trom");
  }
}

BLYNK_CONNECTED() {
  Blynk.virtualWrite(V0, isDoorOpen ? 1 : 0);
  Blynk.virtualWrite(V1, isAntiTheftActive ? 1 : 0);
  lastFanSent = -1;                        // gửi lại V2, V4 ngay sau khi kết nối
  lastAlarmSent = -1;
  lastGasSend = 0;
}

void updateSystem() {
  distanceCm = readDistance();
  carPresent = (distanceCm <= DIST_CAR_PRESENT);
  mq2Value = analogRead(MQ2_PIN);

  // --- Quạt hút: có độ trễ, bỏ qua lúc MQ-2 đang làm nóng ---
  if (millis() > GAS_WARMUP_MS) {
    if (mq2Value > GAS_ON_TH) isFanOn = true;
    else if (mq2Value < GAS_OFF_TH) isFanOn = false;
  } else {
    isFanOn = false;
  }
  digitalWrite(FAN_RELAY_PIN, isFanOn ? RELAY_ON : RELAY_OFF);

  // --- Chống trộm: đang bật mà mất xe liên tiếp thì báo ---
  if (isAntiTheftActive && !isAlarmTriggered) {
    if (!carPresent) missCount++;
    else missCount = 0;
    if (missCount >= THEFT_CONFIRM) isAlarmTriggered = true;
  }

  // --- Gửi cảnh báo trộm lên Blynk (một lần cho mỗi lần báo) ---
  if (isAlarmTriggered && !alertSent && Blynk.connected()) {
    Blynk.logEvent("theft_alert", "Xe da roi khoi gara khi dang bat chong trom!");
    alertSent = true;
    Serial.println("[BLYNK] Da gui canh bao trom");
  }

  // --- Gửi dữ liệu lên Blynk (chỉ khi online, V2/V4 chỉ gửi khi đổi) ---
  if (Blynk.connected()) {
    if (millis() - lastGasSend >= GAS_SEND_MS || lastGasSend == 0) {
      Blynk.virtualWrite(V3, mq2Value);
      lastGasSend = millis();
    }
    int fanNow = isFanOn ? 255 : 0;
    if (fanNow != lastFanSent) {
      Blynk.virtualWrite(V2, fanNow);
      lastFanSent = fanNow;
    }
    int alarmNow = isAlarmTriggered ? 255 : 0;
    if (alarmNow != lastAlarmSent) {
      Blynk.virtualWrite(V4, alarmNow);
      lastAlarmSent = alarmNow;
    }
  }

  // --- OLED ---
  if (oledAvailable) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Khoi MQ2: "); display.println(mq2Value);
    display.print("Xe: "); display.print(carPresent ? "CO" : "KHONG");
    display.print(" ("); display.print(distanceCm); display.println("cm)");
    display.print("Chong trom: "); display.println(isAntiTheftActive ? "ON" : "OFF");
    display.print("Quat: "); display.println(isFanOn ? "ON" : "OFF");
    display.print("Cua: "); display.println(isDoorOpen ? "MO" : "DONG");
    display.print("WiFi: "); display.print(WiFi.status() == WL_CONNECTED ? "OK" : "...");
    display.print(" Blynk: "); display.println(Blynk.connected() ? "OK" : "...");
    if (isAlarmTriggered) display.println("!! CO TROM !!");
    display.display();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[OLED] Khong tim thay man hinh");
  } else {
    oledAvailable = true;
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Dang khoi dong...");
    display.display();
  }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);
  digitalWrite(FAN_RELAY_PIN, RELAY_OFF);

  ESP32PWM::allocateTimer(0);
  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  setDoor(false);

  timer.setInterval(1000L, updateSystem);

  WiFi.begin(ssid, pass);
  Blynk.config(auth);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) Blynk.run();
  timer.run();

  // Còi chỉ kêu khi báo trộm
  digitalWrite(BUZZER_PIN, isAlarmTriggered ? BUZZER_ON : BUZZER_OFF);

  // Nút bấm mở/đóng cửa
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      setDoor(!isDoorOpen);
      Serial.println(isDoorOpen ? "[NUT] Mo cua" : "[NUT] Dong cua");
      if (Blynk.connected()) Blynk.virtualWrite(V0, isDoorOpen ? 1 : 0);
      while (digitalRead(BUTTON_PIN) == LOW) { delay(10); }
    }
  }
}
