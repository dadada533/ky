// DS1307 + TM1637：串口输出完整时间，数码管显示"组号:分钟"
// 绕过 Fred.Chu 库 init() 的 ACK 死锁问题

#include <Wire.h>
#include "RTClib.h"

#define CLK 10
#define DIO 9
#define GROUP 1          // 改成你的组号

// ---------- TM1637 直接驱动（带超时，不阻塞） ----------
static const uint8_t TubeTab[] = {
  0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,
  0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71   // 0~F
};

void tm1637Start() {
  digitalWrite(CLK, HIGH);
  digitalWrite(DIO, HIGH);
  delayMicroseconds(2);
  digitalWrite(DIO, LOW);
}

void tm1637Stop() {
  digitalWrite(CLK, LOW);
  delayMicroseconds(2);
  digitalWrite(DIO, LOW);
  delayMicroseconds(2);
  digitalWrite(CLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(DIO, HIGH);
}

// 写一个字节，ACK 最多等 100μs，超时就放弃（不阻塞）
bool tm1637Write(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(CLK, LOW);
    digitalWrite(DIO, data & 0x01);
    data >>= 1;
    delayMicroseconds(2);
    digitalWrite(CLK, HIGH);
    delayMicroseconds(2);
  }
  // ACK
  digitalWrite(CLK, LOW);
  delayMicroseconds(2);
  digitalWrite(DIO, HIGH);
  delayMicroseconds(2);
  digitalWrite(CLK, HIGH);
  delayMicroseconds(2);
  pinMode(DIO, INPUT);
  uint16_t timeout = 0;
  while (digitalRead(DIO)) {
    delayMicroseconds(1);
    if (++timeout > 200) { pinMode(DIO, OUTPUT); digitalWrite(CLK, LOW); return false; }
  }
  pinMode(DIO, OUTPUT);
  digitalWrite(CLK, LOW);
  return true;
}

void tm1637Show(const int8_t *buf) {
  // 转换段码
  uint8_t seg[4];
  for (uint8_t i = 0; i < 4; i++) {
    if (buf[i] == 0x7F) seg[i] = 0;
    else seg[i] = TubeTab[buf[i] & 0x0F];
    seg[i] |= 0x80;   // 点亮所有位的小数点（作为冒号位置的消隐）
  }
  tm1637Start();
  tm1637Write(0x40);  // 自动地址递增
  tm1637Stop();
  tm1637Start();
  tm1637Write(0xC0);  // 起始地址 0
  for (uint8_t i = 0; i < 4; i++) tm1637Write(seg[i]);
  tm1637Stop();
  tm1637Start();
  tm1637Write(0x8A);  // 亮度 2，显示开（0x88 + brightness）
  tm1637Stop();
}

// ---------- 主程序 ----------
RTC_DS1307 rtc;
bool rtcOk = false;

char daysOfTheWeek[7][12] = {"Sunday","Monday","Tuesday","Wednesday",
                             "Thursday","Friday","Saturday"};

void showGroupMinute(int minute) {
  int8_t buf[4];
  buf[0] = GROUP % 10;
  buf[1] = (minute / 10) % 10;
  buf[2] = minute % 10;
  buf[3] = 0x7F;
  tm1637Show(buf);
}

void setup() {
  Serial.begin(57600);

  pinMode(CLK, OUTPUT);
  pinMode(DIO, OUTPUT);
  digitalWrite(CLK, LOW);
  digitalWrite(DIO, LOW);

  // 显示 00 以防数码管亮
  showGroupMinute(0);

  Serial.println("Program start");

  rtcOk = rtc.begin();
  if (!rtcOk) {
    Serial.println("Couldn't find RTC - check SDA(A4)/SCL(A5)/5V/GND");
  } else {
    Serial.println("RTC found");
    if (!rtc.isrunning()) {
      Serial.println("RTC not running, set time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
}

void loop() {
  Serial.print("Group ");
  Serial.print((int)GROUP);
  Serial.print(" | ");

  if (!rtcOk) {
    rtcOk = rtc.begin();
    if (!rtcOk) {
      Serial.println("RTC not found");
      delay(1000);
      return;
    }
    Serial.println("RTC connected");
    if (!rtc.isrunning())
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
  showGroupMinute(now.minute());

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.print("Group ");
  Serial.print((int)GROUP);
  Serial.print(" | since midnight 1/1/1970 = ");
  Serial.print(now.unixtime());
  Serial.print("s = ");
  Serial.print(now.unixtime() / 86400L);
  Serial.println("d");

  DateTime future(now + TimeSpan(7,12,30,6));
  Serial.print("Group ");
  Serial.print((int)GROUP);
  Serial.print(" | now + 7d + 12h + 30m + 6s: ");
  Serial.print(future.year(), DEC);
  Serial.print('/');
  Serial.print(future.month(), DEC);
  Serial.print('/');
  Serial.print(future.day(), DEC);
  Serial.print(' ');
  Serial.print(future.hour(), DEC);
  Serial.print(':');
  Serial.print(future.minute(), DEC);
  Serial.print(':');
  Serial.print(future.second(), DEC);
  Serial.println();
  Serial.println();

  delay(1000);
}
