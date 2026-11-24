// ============================================================
// clock.ino - Otto 시계: I2R_Time(itime) 으로 시간을 읽는다.
//             (웹의 [시계] 버튼을 누르면 눈 OLED 에 큰 글씨로 보여 준다. 평소 눈 화면에는 시간을 그리지 않는다)
//
// 시간을 맞추는 일은 I2R_IoT 의 I2R_Time 이 한다 (otto_iot.ino 의 itime).
//  - Wi-Fi 가 연결되면 NTP(pool.ntp.org 등)로 맞추고, 그 뒤로는 ESP32 가 한 시간마다 다시 맞춘다.
//  - 인터넷이 없어도 웹, 시뮬레이터가 보낸 st 명령(현지 epoch, 시간대)으로 맞출 수 있다.
//  - 시간대는 NVS "i2r" 의 tz_offset (기본 한국 UTC+9). 공식 펌웨어에서 맞춘 값을 그대로 이어받는다.
//  - 껐다 켜면 마지막으로 저장한 시각에서 이어 간다 (restored). 이 시각은 꺼져 있던 시간만큼 틀릴 수 있어서
//    화면에는 보여 주지만, 예약(schedule.ino)은 NTP 나 st 로 맞춘 뒤에만 돈다 (clockSynced).
//
// 시스템 시계는 UTC 다. 현지 시각은 반드시 itime.local() 로 읽는다 (localtime_r 를 쓰지 않는다).
// 화면 그리기는 eyes.ino 의 eyesDrawBigClock() 이 한다. (clockText() 로 문자열을 받는다)
// ============================================================

// 시간이 있는지 (NTP, st, 또는 저장한 시각에서 이어 가는 중)
bool clockValid() { return itime.valid(); }

// 시간을 믿을 수 있는지 (NTP 나 st 로 맞춤). 예약은 이것만 본다.
bool clockSynced() { return itime.valid() && !itime.restored(); }

// loop() 에서 계속 부른다. 시간 상태가 바뀌면 시리얼에 한 줄 남긴다.
void clockLoop() {
  static int lastState = -1; // 0 없음, 1 이어 가는 중, 2 맞춤
  int state = clockSynced() ? 2 : (clockValid() ? 1 : 0);
  if (state == lastState)
    return;
  lastState = state;
  if (state == 2)
    Serial.printf("[시계] 시간을 맞췄습니다: %s (시간대 UTC%+ld)\n", itime.text().c_str(), itime.tz() / 3600);
  else if (state == 1)
    Serial.printf("[시계] 저장한 시각에서 이어 갑니다: %s (NTP 나 st 로 맞추기 전까지 예약은 쉽니다)\n", itime.text().c_str());
  else
    Serial.println("[시계] 시간이 아직 없습니다. Wi-Fi(NTP) 나 st 명령을 기다립니다.");
}

// 지금 시각을 "HH:MM" 으로 buf(6바이트 이상)에 쓴다. 시간이 없으면 false.
bool clockText(char *buf) {
  if (!clockValid())
    return false;
  struct tm tmv;
  itime.local(tmv);
  snprintf(buf, 6, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
  return true;
}

// 화면을 다시 그려야 하는지 보려고 쓴다. 분이 바뀌면 값이 바뀐다. 시간이 없으면 -1.
int clockMinuteKey() {
  if (!clockValid())
    return -1;
  struct tm tmv;
  itime.local(tmv);
  return tmv.tm_hour * 60 + tmv.tm_min;
}
