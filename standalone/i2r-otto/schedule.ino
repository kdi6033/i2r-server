// ============================================================
// schedule.ino - Otto 예약: 정해진 시각에 조명, 라디오, 내 음악을 켜고 끈다.
//
// 시각은 clock.ino 의 시계(NTP, 한국 시간)를 쓴다. 시계가 아직 맞지 않으면 예약은 동작하지 않는다.
// 예약은 최대 SCHED_MAX 개이고 같은 대상(예: 조명)을 여러 번 겹쳐서 넣어도 된다. NVS 에 저장한다.
// board-i2r-03 의 TimeSlot(시작/끝 시각 + 매일/요일 반복)과 같은 생각이다.
//
//   예약 하나 = 대상(t) + 켜는 시각(on) + 끄는 시각(off) + 요일(d) + 사용(en) + 값(p)
//     t   0 조명, 1 라디오, 2 내 음악
//     on  켜는 시각, 자정부터 지난 분 (0 ~ 1439). -1 이면 켜지 않는다 (끄기만 한다)
//     off 끄는 시각 (0 ~ 1439). -1 이면 끄지 않는다 (켜기만 한다). off 가 on 보다 작으면 다음 날 끈다.
//     d   요일 비트: 1=일 2=월 4=화 8=수 16=목 32=금 64=토. 127 = 매일
//     en  1 사용 / 0 쓰지 않음
//     p   라디오는 방송국 번호(0부터), 내 음악은 곡 번호(1부터, 0 = 처음 곡), 조명은 쓰지 않는다
//         (조명은 [조명] 패널에서 마지막으로 고른 색과 밝기로 켠다. 소리는 라디오/내 음악 볼륨을 쓴다)
//
//   {"c":"os","m":"..","a":"get"}                       예약 목록 요청
//   {"c":"os","m":"..","a":"set","tx":"0,420,480,127,1,0;1,1080,1200,62,1,3"}   예약 전체를 바꾼다 ("" 이면 모두 지운다)
//        tx 는 예약을 ';' 로, 값은 ',' 로 구분한다: t,on,off,d,en,p
//
// 응답: {"c":"os","m":"..","a":"<요청>","ok":1,"tx":"<예약 목록>","time":"HH:MM"}  실패하면 "ok":0,"err":"..."
//        time 은 보드의 지금 시각이다. MQTT 메시지가 300바이트 이하여야 해서 예약은 8개까지만 둔다.
//
// 켜고 끄는 일은 웹에서 누른 것과 같은 함수를 쓴다. 정해진 분(分)이 되는 순간에만 한 번 실행하므로,
// 그 사이에 손으로 끄거나 켜면 그대로 둔다. 같은 분에 끄기와 켜기가 겹치면 끄기를 먼저 한다.
// 보드가 꺼져 있던 동안 지나간 예약은 하지 않는다.
// ============================================================

const int SCHED_MAX = 8;
const int SCHED_WEEK_MIN = 7 * 1440;  // 한 주의 분
const int SCHED_MAX_CATCHUP = 10;     // 오래 막혀 있었어도 이 분 수만큼만 지난 분을 따라잡는다

struct SchedSlot {
  int8_t type;    // 0 조명, 1 라디오, 2 내 음악
  int16_t on;     // 켜는 분 (-1 = 없음)
  int16_t off;    // 끄는 분 (-1 = 없음)
  uint8_t days;   // 요일 비트
  uint8_t enabled;
  int8_t param;   // 방송국 / 곡 번호
};

SchedSlot schedSlots[SCHED_MAX];
int schedCount = 0;
int schedLastKey = -1; // 마지막으로 처리한 분 (요일*1440 + 분)

// 예약 목록을 "t,on,off,d,en,p;..." 문자열로 만든다.
String schedToString() {
  String s;
  for (int i = 0; i < schedCount; i++) {
    if (i > 0)
      s += ";";
    // int8_t 를 그대로 String 에 넣으면 숫자가 아니라 글자 하나가 되므로 int 로 바꿔서 넣는다
    s += String((int)schedSlots[i].type) + "," + String((int)schedSlots[i].on) + "," + String((int)schedSlots[i].off) + "," +
         String((int)schedSlots[i].days) + "," + String((int)schedSlots[i].enabled) + "," + String((int)schedSlots[i].param);
  }
  return s;
}

// 문자열을 읽어서 예약 목록을 바꾼다. 형식이 틀리면 false 이고 기존 목록은 그대로 둔다.
bool schedParse(const String &s) {
  SchedSlot tmp[SCHED_MAX];
  int n = 0;
  const char *p = s.c_str();

  while (*p != '\0') {
    if (n >= SCHED_MAX)
      return false;
    long v[6];
    for (int f = 0; f < 6; f++) {
      char *end = nullptr;
      v[f] = strtol(p, &end, 10);
      if (end == p)
        return false; // 숫자가 없다
      p = end;
      if (f < 5) {
        if (*p != ',')
          return false;
        p++;
      }
    }
    if (*p == ';')
      p++;
    else if (*p != '\0')
      return false;

    if (v[0] < 0 || v[0] > 2 || v[1] < -1 || v[1] > 1439 || v[2] < -1 || v[2] > 1439 || v[3] < 0 || v[3] > 127 ||
        v[4] < 0 || v[4] > 1 || v[5] < 0 || v[5] > 99)
      return false;
    tmp[n].type = v[0];
    tmp[n].on = v[1];
    tmp[n].off = v[2];
    tmp[n].days = v[3];
    tmp[n].enabled = v[4];
    tmp[n].param = v[5];
    n++;
  }

  for (int i = 0; i < n; i++)
    schedSlots[i] = tmp[i];
  schedCount = n;
  return true;
}

// 부팅할 때 NVS 에서 예약을 읽는다.
void schedLoad() {
  Preferences prefs;
  prefs.begin("ottosched", true);
  String s = prefs.getString("slots", "");
  prefs.end();
  if (!schedParse(s))
    schedCount = 0;
  Serial.printf("[예약] %d개 불러왔습니다.\n", schedCount);
}

void schedSave() {
  Preferences prefs;
  prefs.begin("ottosched", false);
  prefs.putString("slots", schedToString());
  prefs.end();
}

void schedAck(const char *action, bool ok, const char *err = nullptr) {
  DynamicJsonDocument res(512);
  res["c"] = "os";
  res["m"] = wifi.mac;
  res["a"] = action;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  res["tx"] = schedToString();
  char hm[6];
  res["time"] = clockText(hm) ? hm : "";
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// 예약 하나를 실행한다. on 이 true 면 켜고 false 면 끈다. 결과는 웹에도 알린다.
void schedRun(int i, bool on) {
  int type = schedSlots[i].type;
  int param = schedSlots[i].param;
  const char *err = nullptr;
  Serial.printf("[예약] %d번 %s %s\n", i + 1, type == 0 ? "조명" : (type == 1 ? "라디오" : "내 음악"), on ? "켜기" : "끄기");

  if (type == 0) { // 조명
    lightDemoOn = false;
    lightOn = on;
    lightApply();
    lightAck(true);
  } else if (type == 1) { // 라디오
    if (on) {
      if (radioStart(param, err)) {
        radioAck("play", true);
      } else {
        Serial.printf("[예약] 라디오 시작 실패: %s\n", err);
        radioOn = false;
        radioAck("play", false, err);
      }
    } else if (radioOn) { // 다른 소리가 나오고 있으면 건드리지 않는다
      radioStopPlaying(true);
    }
  } else { // 내 음악
    if (on) {
      int n = (param > 0 && musicExists(param)) ? param : musicNext(0);
      if (n > 0 && musicPlay(n, err)) {
        musicAck("play", true);
      } else {
        if (n == 0)
          err = "no_song";
        Serial.printf("[예약] 내 음악 시작 실패: %s\n", err ? err : "?");
        musicOn = false;
        musicAck("play", false, err);
      }
    } else if (musicOn) {
      musicStop(true);
    }
  }
}

// 한 분(요일*1440 + 분)에 해당하는 예약을 실행한다. 끄기를 먼저, 켜기를 나중에 한다.
void schedFire(int key) {
  int wday = key / 1440;
  int minute = key % 1440;
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < schedCount; i++) {
      if (!schedSlots[i].enabled)
        continue;
      if (pass == 0) { // 끄기
        if (schedSlots[i].off != minute)
          continue;
        // 자정을 넘기는 예약(끄는 시각이 켜는 시각보다 이르다)은 켠 날의 다음 날에 끈다
        int day = (schedSlots[i].on >= 0 && schedSlots[i].off < schedSlots[i].on) ? (wday + 6) % 7 : wday;
        if (schedSlots[i].days & (1 << day))
          schedRun(i, false);
      } else { // 켜기
        if (schedSlots[i].on != minute)
          continue;
        if (schedSlots[i].days & (1 << wday))
          schedRun(i, true);
      }
    }
  }
}

// os 명령 처리 (set / get)
void schedCommand(JsonDocument &doc) {
  String action = doc["a"] | "";
  if (action == "set") {
    String list = doc["tx"] | "";
    if (!schedParse(list)) {
      schedAck("set", false, "bad_param");
      return;
    }
    schedSave();
    Serial.printf("[예약] %d개 저장했습니다.\n", schedCount);
    schedAck("set", true);
  } else if (action == "get") {
    schedAck("get", true);
  } else {
    schedAck(action.c_str(), false, "bad_action");
  }
}

// loop() 에서 계속 불린다. 1초에 한 번 보고, 분이 바뀌었을 때 그 분의 예약을 실행한다.
void scheduleLoop() {
  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck < 1000)
    return;
  lastCheck = now;

  if (!clockSynced()) // NTP 나 st 로 맞춘 시간에서만 (껐다 켠 뒤 이어 가는 시각은 틀릴 수 있다)
    return;

  struct tm tmv;
  itime.local(tmv); // 현지 시각 (I2R_Time)
  int key = tmv.tm_wday * 1440 + tmv.tm_hour * 60 + tmv.tm_min;
  if (key == schedLastKey)
    return;

  // 오래 막혀 있다가 돌아왔어도 지난 몇 분의 예약은 놓치지 않는다. (너무 오래됐으면 지금 분만)
  int steps = 1;
  if (schedLastKey >= 0) {
    steps = (key - schedLastKey + SCHED_WEEK_MIN) % SCHED_WEEK_MIN;
    if (steps > SCHED_MAX_CATCHUP)
      steps = 1;
  }
  schedLastKey = key;
  for (int s = steps - 1; s >= 0; s--)
    schedFire((key - s + SCHED_WEEK_MIN) % SCHED_WEEK_MIN);
}
