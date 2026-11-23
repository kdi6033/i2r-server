// standalone: i2r PLC(i2r-02, 03, 04) 혼자 동작하는 펌웨어 (i2r-server 그룹 ①)
//
// i2r-server 펌웨어는 세 그룹으로 나뉜다. 이 파일은 ①이다.
//   ① standalone  보드 혼자 동작. 보드가 웹서버가 되어 같은 공유기의 휴대폰, PC에서 제어
//   ② local       현장 서버: PLC 여러 대 + i2r-05(MQTT 서버)
//   ③ cloud       클라우드: 아마존 서버와 연결
// 배포 위치: https://github.com/kdi6033/download 의 i2r-server/standalone/i2r-0X.ino.merged.bin
//
// 보드가 직접 웹페이지를 띄운다. 같은 공유기에 연결된 PC나 휴대폰 브라우저에서
// http://i2r.local 또는 시리얼 모니터에 나온 주소(http://192.168.x.x)를 연다.
//
// [Wi-Fi 설정] 두 가지 중 하나
//   1) i2r.link 웹 → 연결 → COM 포트(USB)에서 Wi-Fi 이름, 비밀번호를 보낸다 (배포 펌웨어는 이 방법).
//      보드가 {"c":"si",...}를 받아 저장하고 다시 시작한다. 이메일, 서버 칸은 쓰지 않는다.
//   2) 개발용: secrets.example.h를 secrets.h로 복사해 WIFI_SSID, WIFI_PASSWORD를 적는다. 1)로 저장한 값이 있으면 그것을 먼저 쓴다.
//   저장 자리는 I2R_IoT와 같다 (NVS "i2riot"). 그래서 ② local, ③ cloud 펌웨어로 바꿔도 Wi-Fi가 이어진다.
//
// [보드 바꾸기] 아래 I2R_BOARD를 I2R_02, I2R_03, I2R_04 중 하나로.
//   배포용은 arduino-cli로 -DI2R_BOARD=I2R_02 -DI2R_RELEASE 를 넣어 만든다 (Wi-Fi 값을 비움).
//
// [화면에서 하는 것] (i2r.link 장치 화면과 같음)
//   입력 상태 (INPUTS)    IN1~IN4 켜짐/꺼짐 표시. 이름을 누르면 "입력 규칙" (bio)
//                         입력이 켜질 때/꺼질 때 → 지연 뒤 출력을 켜고/끄고, 동작 시간 뒤 끄기
//   출력 제어 (OUTPUTS)   OUT1~OUT4 스위치. 이름을 누르면 "스케줄" (sch)
//                         기기 현재 시간, 시간대 고르기, 동기화, 매일/매주 시작~종료 시간에 켜기
//   온도, 습도             누르면 "센서 트리거" (bs): 올라갈 때/내려갈 때 출력 바꾸기, 보정
//   ⚙ (이름 옆)            기기 이름과 입력, 출력 이름 바꾸기
//
// [나눠 맡기]
//   I2R_PLC   입력 읽기(떨림 제거), 릴레이, 자동 끄기, AHT21 온습도
//   I2R_IoT   I2R_Time: NTP 시간, 시간대, 동기화(st)  /  I2R_Link: 값 검사 함수
//   이 스케치  Wi-Fi 연결, 웹페이지(page.h), 입력 규칙, 스케줄, 센서 트리거, 이름 저장
//
// [명령 모양] i2r 프로토콜(https://github.com/kdi6033/i2r-server/blob/main/protocol.md)의
//   so, bio, sch, bs, st와 같은 JSON을 웹에서 POST /cmd 로 보낸다. 나중에 MQTT로 옮기기 쉽다.
//   다른 점: 웹이 한 번에 받도록 목록(list)은 "items" 배열 하나로 답하고,
//   insert, delete 뒤에도 목록을 돌려준다. 규칙, 스케줄, 보정, 이름은 보드에 저장되어 껐다 켜도 남는다.
//   시리얼 모니터에 같은 JSON 한 줄을 넣어도 된다. 예: {"c":"so","n":0,"v":1}
//
// [필요한 라이브러리] I2R_PLC, I2R_IoT, ArduinoJson, PubSubClient (I2R_IoT가 함께 씀)
// [보드 설정] ESP32 Dev Module, Flash 8MB, Partition: Huge APP
//
// [주의]
// - 같은 공유기에 붙은 사람은 누구나 이 화면으로 릴레이를 켤 수 있다. 공용 Wi-Fi에서 쓰지 않는다.
// - 비밀번호는 secrets.h에만 적는다 (.gitignore로 GitHub에 올라가지 않음). 이 파일에 직접 적지 않는다.
//   배포 펌웨어(I2R_RELEASE)는 secrets.h를 읽지 않는다.
// - 2.4GHz Wi-Fi만 된다.
// - 업로드하기 전에 릴레이에 연결된 장치가 켜져도 괜찮은지 확인한다 (저장된 스케줄이 바로 켤 수 있다).
//
// [번호] 사람은 1부터 (OUT1, plc.output(1)), JSON은 0부터 ("n":0 = OUT1)

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <I2R_IoT.h>
#include <I2R_PLC.h>
#include "page.h"
#include "logo.h"

// ===== 개발용 Wi-Fi (선택): secrets.example.h를 secrets.h로 복사해 내 값을 넣는다 =====
// secrets.h는 GitHub에 올라가지 않는다 (.gitignore). COM 포트로 설정을 보냈다면 그 값을 먼저 쓴다.
// 배포 펌웨어(I2R_RELEASE)는 secrets.h를 읽지 않는다.
#if !defined(I2R_RELEASE) && __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""      // 비워 두면 i2r.link → 연결 → COM 포트로 넣는다
#define WIFI_PASSWORD ""
#endif
// =====================================================================================

#ifndef I2R_BOARD
#define I2R_BOARD I2R_03  // 내 보드: I2R_02, I2R_03, I2R_04
#endif
I2R_PLC plc(I2R_BOARD);
I2R_Time itime;
WebServer server(80);

typedef I2R_Link L;   // 값 검사 함수(isInt, isNum, isOnOff ...)만 쓴다

const char *GROUP = "standalone";            // i2r-server 그룹 (standalone, local, cloud)
const char *VERSION = "standalone-0.1.0";    // 버전에 그룹을 붙여, 같은 이름의 파일이어도 구별한다
const char *WIFI_NS = "i2riot";              // Wi-Fi 저장 자리 (I2R_IoT의 I2R_Net과 같음)
String wifiSsid, wifiPw;                     // 실제로 쓰는 Wi-Fi (저장된 값 → 없으면 위 값)
const char *NVS_NS = "i2rweb";      // 이 스케치의 저장 자리 (I2R_IoT, 공식 펌웨어와 겹치지 않음)
const uint8_t MAX_RULES = 40;       // 입력 규칙, 스케줄, 센서 트리거 각각 최대 40개
const uint32_t MAX_SEC = 86400;     // 지연, 동작 시간 최대 (초)
const uint8_t NAME_MAX_BYTES = 60;  // 이름 하나 최대 길이 (한글 약 20자)

String mac;       // "A0:B7:65:CD:4D:40"
// 웹 주소 이름 → http://i2r.local
// 같은 공유기에 i2r 보드가 여러 대면 이름이 겹치므로 "i2r-kitchen"처럼 보드마다 다르게 바꾼다
const char *HOST_NAME = "i2r";
String hostName = HOST_NAME;

// ---- 이름: [0] 기기, [1..IN] 입력, [IN+1..] 출력 ----
String names[1 + I2R_PLC::MAX_CH * 2];
uint8_t nameCount() { return 1 + plc.inputCount() + plc.outputCount(); }

// ---- 입력 규칙 (bio): 입력 in이 tr이 되는 순간 → d초 뒤 출력 n을 v로, 켰으면 du초 뒤 끄기 ----
struct BioRule {
  uint16_t sI;      // 규칙 번호 (1부터, 보드가 정함)
  uint8_t in, tr;   // 입력 번호 (0부터), 트리거 (1 켜질 때, 0 꺼질 때)
  uint8_t n, v;     // 바꿀 출력 (0부터), 값
  uint32_t d, du;   // 지연, 동작 시간 (초)
  bool wait;        // 지연 시간을 세는 중 (저장하지 않는 값)
  uint32_t at;      // 지연을 시작한 millis()
};
BioRule bio[MAX_RULES];
uint8_t nBio = 0;
uint16_t nextBio = 1;

// ---- 스케줄 (sch): 출력 pi를 start~end(0시부터 센 초)에 켜기. 매주는 요일마다 한 줄 ----
struct SchSlot {
  uint8_t pi;       // 출력 번호 (0부터)
  uint8_t weekly;   // 0 매일, 1 매주
  int8_t dw;        // 요일 (0 일요일 ~ 6 토요일), 매일은 -1
  uint8_t pad;
  int32_t start, end;
};
SchSlot sch[MAX_RULES];
uint8_t nSch = 0;
bool schPrev[I2R_PLC::MAX_CH];  // 지난번에 스케줄 시간 안이었는지 (들어가고 나가는 순간만 출력을 바꿈)

// ---- 센서 트리거 (bs): 값이 tv를 넘어 올라가거나(tr 1) 내려가는(tr 0) 순간 → 출력 n을 v로 ----
struct BsRule {
  uint16_t sI;      // 규칙 번호 (0부터, 보드가 정함)
  uint8_t ts, tr;   // 센서 (0 온도, 1 습도), 트리거
  uint8_t n, v;     // 바꿀 출력 (0부터), 값
  float tv;         // 기준 값
  uint32_t d, du;   // 지연, 동작 시간 (초)
  float last;       // 지난번 값 (처음은 -1, 저장하지 않는 값)
  bool wait;
  uint32_t at;
};
BsRule bs[MAX_RULES];
uint8_t nBs = 0;
uint16_t nextBs = 0;
float cal[2] = {0, 0};  // 보정값: 보이는 값 = 측정값 + 보정값 (온도, 습도)

const char *TS_NAME[2] = {"temp", "humi"};

// ======================================================================
// 저장 (NVS)
// ======================================================================
template <typename T>
void saveBlob(const char *key, const T *arr, uint8_t count) {
  Preferences p;
  if (!p.begin(NVS_NS, false)) return;
  if (count) p.putBytes(key, arr, sizeof(T) * count);
  else p.remove(key);
  p.end();
}

template <typename T>
uint8_t loadBlob(Preferences &p, const char *key, T *arr) {
  size_t len = p.getBytesLength(key);
  if (len == 0 || len % sizeof(T) != 0 || len > sizeof(T) * MAX_RULES) return 0;  // 모양이 다르면 버림
  p.getBytes(key, arr, len);
  return len / sizeof(T);
}

void saveBio() {
  saveBlob("bio", bio, nBio);
  Preferences p;
  if (p.begin(NVS_NS, false)) { p.putUShort("bioNext", nextBio); p.end(); }
}
void saveSch() { saveBlob("sch", sch, nSch); }
void saveBs() {
  saveBlob("bs", bs, nBs);
  Preferences p;
  if (p.begin(NVS_NS, false)) { p.putUShort("bsNext", nextBs); p.end(); }
}
void saveCal() {
  Preferences p;
  if (p.begin(NVS_NS, false)) { p.putBytes("cal", cal, sizeof(cal)); p.end(); }
}

String defaultName(uint8_t i) {
  if (i == 0) return plc.type();
  if (i <= plc.inputCount()) return "입력 " + String(i);
  return "출력 " + String(i - plc.inputCount());
}

void saveNames() {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (uint8_t i = 0; i < nameCount(); i++) a.add(names[i]);
  String s;
  serializeJson(doc, s);
  Preferences p;
  if (p.begin(NVS_NS, false)) { p.putString("names", s); p.end(); }
}

void loadAll() {
  for (uint8_t i = 0; i < nameCount(); i++) names[i] = defaultName(i);
  Preferences p;
  if (!p.begin(NVS_NS, true)) return;  // 처음 켤 때는 저장 자리가 없다
  String s = p.getString("names", "");
  JsonDocument doc;
  if (s.length() && !deserializeJson(doc, s) && doc.is<JsonArray>()) {
    JsonArray a = doc.as<JsonArray>();
    for (uint8_t i = 0; i < nameCount() && i < a.size(); i++) {
      String v = a[i] | "";
      if (v.length()) names[i] = v;
    }
  }
  nBio = loadBlob(p, "bio", bio);
  nextBio = p.getUShort("bioNext", 1);
  nSch = loadBlob(p, "sch", sch);
  nBs = loadBlob(p, "bs", bs);
  nextBs = p.getUShort("bsNext", 0);
  if (p.getBytesLength("cal") == sizeof(cal)) p.getBytes("cal", cal, sizeof(cal));
  p.end();

  // 저장된 값이 이 보드와 맞지 않으면 버린다 (다른 보드에서 쓰던 저장 자리 등)
  uint8_t k = 0;
  for (uint8_t i = 0; i < nBio; i++)
    if (bio[i].in < plc.inputCount() && bio[i].n < plc.outputCount()) { bio[k] = bio[i]; bio[k].wait = false; k++; }
  nBio = k;
  k = 0;
  for (uint8_t i = 0; i < nSch; i++)
    if (sch[i].pi < plc.outputCount() && sch[i].start >= 0 && sch[i].start < 86400 && sch[i].end >= 0 && sch[i].end < 86400) sch[k++] = sch[i];
  nSch = k;
  k = 0;
  for (uint8_t i = 0; i < nBs; i++)
    if (bs[i].ts < 2 && bs[i].n < plc.outputCount()) { bs[k] = bs[i]; bs[k].last = -1; bs[k].wait = false; k++; }
  nBs = k;
  for (uint8_t i = 0; i < 2; i++) if (isnan(cal[i]) || fabsf(cal[i]) > 100) cal[i] = 0;
}

template <typename T>
void removeAt(T *arr, uint8_t &count, uint8_t idx) {
  for (uint8_t i = idx; i + 1 < count; i++) arr[i] = arr[i + 1];
  count--;
}

// ======================================================================
// 센서 값 (보정 포함)
// ======================================================================
bool hasSensor() { return plc.board() == I2R_03; }

float sensorRaw(uint8_t ts) { return ts == 0 ? plc.temperature() : plc.humidity(); }
float sensorValue(uint8_t ts) { return L::round1(sensorRaw(ts) + cal[ts]); }

// ======================================================================
// 규칙 실행
// ======================================================================
// 출력 n(0부터)을 v로. 켜면서 du초가 있으면 du초 뒤 저절로 끈다
void runTarget(uint8_t n, uint8_t v, uint32_t du) {
  plc.output(n + 1, v, v ? du : 0);
}

void bioUpdate() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < plc.inputCount(); i++) {
    bool r = plc.rising(i + 1), f = plc.falling(i + 1);
    if (!r && !f) continue;
    uint8_t val = r ? 1 : 0;
    for (uint8_t k = 0; k < nBio; k++) {
      // 입력이 트리거와 같아지면 지연을 처음부터 센다 (기다리는 중 다시 바뀌어도 처음부터)
      if (bio[k].in == i && bio[k].tr == val) { bio[k].wait = true; bio[k].at = now; }
    }
  }
  for (uint8_t k = 0; k < nBio; k++) {
    if (!bio[k].wait || now - bio[k].at < bio[k].d * 1000UL) continue;
    bio[k].wait = false;
    Serial.printf("[입력 규칙 %u] IN%u %s → OUT%u %s%s\n", bio[k].sI, bio[k].in + 1, bio[k].tr ? "켜짐" : "꺼짐",
                  bio[k].n + 1, bio[k].v ? "켜기" : "끄기", (bio[k].v && bio[k].du) ? (" (" + String(bio[k].du) + "초)").c_str() : "");
    runTarget(bio[k].n, bio[k].v, bio[k].du);
  }
}

void schUpdate() {
  static unsigned long last = 0;
  if (millis() - last < 500) return;
  last = millis();
  if (!itime.valid()) return;  // 시간을 아직 모르면 스케줄은 쉰다
  struct tm t;
  itime.local(t);
  int32_t sec = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  for (uint8_t pi = 0; pi < plc.outputCount(); pi++) {
    bool on = false;
    for (uint8_t k = 0; k < nSch && !on; k++) {
      const SchSlot &s = sch[k];
      if (s.pi != pi) continue;
      if (s.weekly && s.dw != t.tm_wday) continue;
      if (s.start < s.end) on = s.start <= sec && sec <= s.end;
      else on = sec >= s.start || sec <= s.end;  // 자정을 넘는 시간대 (22시 ~ 6시)
    }
    if (on != schPrev[pi]) {  // 시간대에 들어가는 순간 켜고, 벗어나는 순간 끈다
      schPrev[pi] = on;
      Serial.printf("[스케줄] OUT%u %s (%s)\n", pi + 1, on ? "켜기" : "끄기", itime.text().c_str());
      plc.output(pi + 1, on);
    }
  }
}

void bsUpdate() {
  if (!hasSensor() || !plc.sensorOk()) return;
  unsigned long now = millis();
  for (uint8_t k = 0; k < nBs; k++) {
    BsRule &r = bs[k];
    if (r.wait && now - r.at >= r.d * 1000UL) {
      r.wait = false;
      Serial.printf("[센서 트리거 %u] %s %s %.1f → OUT%u %s\n", r.sI, r.ts ? "습도" : "온도",
                    r.tr ? "≥" : "≤", r.tv, r.n + 1, r.v ? "켜기" : "끄기");
      runTarget(r.n, r.v, r.du);
    }
    float cur = sensorValue(r.ts);
    if (isnan(cur)) continue;
    bool up = r.tr && r.last < r.tv && r.tv <= cur;     // tv 아래에서 tv 이상이 되는 순간
    bool down = !r.tr && r.last > r.tv && r.tv >= cur;  // tv 위에서 tv 이하가 되는 순간
    if (up || down) { r.wait = true; r.at = now; }       // 지연이 0이면 바로 위에서 다음 차례에 실행
    r.last = cur;
  }
}

// ======================================================================
// 명령 처리 (웹 POST /cmd, 시리얼 한 줄)
// ======================================================================
void setErr(JsonDocument &res, const char *re, const char *code) {
  res.clear();
  res["c"] = "err";
  res["m"] = mac;
  res["re"] = re;
  res["code"] = code;
}

// 상태: {"m","t","name":[..],"in":[..],"out":[..],"remain":[..],"temp","humi","time","tz"...}
void fillState(JsonDocument &s) {
  s["m"] = mac;
  s["t"] = plc.type();
  s["ver"] = VERSION;
  JsonArray nm = s["name"].to<JsonArray>();
  for (uint8_t i = 0; i < nameCount(); i++) nm.add(names[i]);
  JsonArray in = s["in"].to<JsonArray>();
  for (uint8_t n = 1; n <= plc.inputCount(); n++) in.add(plc.input(n) ? 1 : 0);
  JsonArray out = s["out"].to<JsonArray>();
  JsonArray rem = s["remain"].to<JsonArray>();
  for (uint8_t n = 1; n <= plc.outputCount(); n++) {
    out.add(plc.outputState(n) ? 1 : 0);
    rem.add(plc.remain(n));
  }
  s["sensor"] = hasSensor();
  if (hasSensor() && plc.sensorOk()) {
    s["temp"] = sensorValue(0);
    s["humi"] = sensorValue(1);
  }
  s["time"] = itime.valid() ? itime.text() : String("");
  s["tz"] = itime.tz();
  s["tsrc"] = timeSrcName();  // 시간을 어디서: ntp 인터넷, saved 저장된 시각, web 화면 동기화, none 모름
  s["ntpAgo"] = ntpAgoSec();  // 인터넷 시간을 받은 지 몇 초 (-1 = 아직)
  s["ip"] = WiFi.localIP().toString();
  s["rssi"] = WiFi.RSSI();
}

// 초 값 (없으면 0). 0 ~ 86400이 아니면 false
bool optSec(JsonVariantConst v, uint32_t &out) {
  out = 0;
  if (v.isNull()) return true;
  if (!L::isNum(v)) return false;
  float f = v.as<float>();
  if (f < 0 || f > MAX_SEC) return false;
  out = (uint32_t)lroundf(f);
  return true;
}

// ps[] 한 개 {"m"?, "n", "v"}: 이 보드의 출력만 (다른 보드로 보내려면 MQTT가 필요)
bool parseTarget(JsonVariantConst p, uint8_t &n, uint8_t &v, const char *&code) {
  code = "bad_v";
  if (!p.is<JsonObjectConst>()) return false;
  const char *m = p["m"] | "";
  if (*m && !mac.equalsIgnoreCase(m)) return false;
  JsonVariantConst nn = p["n"], vv = p["v"];
  if (!L::isInt(nn) || nn.as<long>() < 0 || !L::isOnOff(vv)) return false;
  if (nn.as<long>() >= plc.outputCount()) { code = "bad_n"; return false; }
  n = nn.as<long>();
  v = L::onOff(vv) ? 1 : 0;
  return true;
}

JsonVariantConst slotArg(JsonObjectConst j) {
  JsonVariantConst si = j["sI"];
  return si.isNull() ? j["si"] : si;
}

bool isOp(const char *o) {
  return !strcmp(o, "insert") || !strcmp(o, "list") || !strcmp(o, "delete") || !strcmp(o, "deleteAll");
}

// so: {"c":"so","n":0,"v":1,"d":5}  d = 켠 뒤 저절로 끌 초 (없으면 계속)
void cmdSo(JsonObjectConst j, JsonDocument &res) {
  JsonVariantConst n = j["n"], v = j["v"];
  if (!L::isInt(n) || n.as<long>() < 0 || n.as<long>() >= plc.outputCount()) return setErr(res, "so", "bad_n");
  if (!L::isOnOff(v)) return setErr(res, "so", "bad_v");
  uint32_t sec;
  if (!optSec(j["d"], sec)) return setErr(res, "so", "bad_v");
  bool on = L::onOff(v);
  plc.output(n.as<long>() + 1, on, on ? sec : 0);
  fillState(res);
}

// st: {"c":"st","epoch":현지 epoch,"tz":32400}  (웹의 "동기화")
void cmdSt(JsonObjectConst j, JsonDocument &res) {
  JsonVariantConst tz = j["tz"];
  if (tz.isNull()) tz = j["tzOffset"];
  if (tz.isNull()) tz = j["tz_offset"];
  JsonVariantConst epoch = j["epoch"];
  long z = L::isNum(tz) ? tz.as<long>() : itime.tz();
  if (z < -12 * 3600 || z > 14 * 3600) return setErr(res, "st", "bad_v");
  if (L::isNum(epoch)) {
    if (!itime.setEpoch((time_t)epoch.as<double>(), z)) return setErr(res, "st", "bad_v");
    onWebTime();
    Serial.printf("[시간] 화면에서 동기화: %s (UTC%+ld:%02ld)\n", itime.text().c_str(), z / 3600, labs(z % 3600) / 60);
  } else if (L::isNum(tz)) {
    itime.setTz(z);
  }
  fillState(res);
}

void bioList(uint8_t port, JsonDocument &res) {
  res.clear();
  res["c"] = "bio";
  res["o"] = "list";
  res["m"] = mac;
  res["n"] = port;
  JsonArray items = res["items"].to<JsonArray>();
  for (uint8_t k = 0; k < nBio; k++) {
    if (bio[k].in != port) continue;
    JsonObject it = items.add<JsonObject>();
    it["sI"] = bio[k].sI;
    it["tr"] = bio[k].tr;
    it["d"] = bio[k].d;
    it["du"] = bio[k].du;
    it["m"] = mac;
    it["n"] = bio[k].n;
    it["v"] = bio[k].v;
  }
}

// bio: {"c":"bio","o":"insert","n":0,"tr":1,"d":3,"du":4,"ps":[{"n":1,"v":1}]}
void cmdBio(JsonObjectConst j, JsonDocument &res) {
  const char *o = j["o"] | "";
  if (!isOp(o)) return setErr(res, "bio", "bad_o");
  JsonVariantConst pn = j["n"];
  if (!L::isInt(pn) || pn.as<long>() < 0 || pn.as<long>() >= plc.inputCount()) return setErr(res, "bio", "bad_n");
  uint8_t port = pn.as<long>();

  if (!strcmp(o, "insert")) {
    JsonVariantConst tr = j["tr"];
    uint32_t d, du;
    if (!L::isOnOff(tr) || !optSec(j["d"], d) || !optSec(j["du"], du)) return setErr(res, "bio", "bad_v");
    JsonArrayConst ps = j["ps"];
    if (ps.isNull() || ps.size() == 0) return setErr(res, "bio", "bad_v");
    if (nBio + ps.size() > MAX_RULES) return setErr(res, "bio", "full");
    uint8_t k = nBio;
    for (JsonVariantConst p : ps) {  // 모두 맞을 때만 넣는다
      const char *code;
      BioRule &r = bio[k++];
      if (!parseTarget(p, r.n, r.v, code)) return setErr(res, "bio", code);
      r.in = port;
      r.tr = L::onOff(tr) ? 1 : 0;
      r.d = d;
      r.du = du;
      r.wait = false;
    }
    for (uint8_t i = nBio; i < k; i++) bio[i].sI = nextBio++;
    nBio = k;
    saveBio();
  } else if (!strcmp(o, "delete")) {
    JsonVariantConst si = slotArg(j);
    int idx = -1;
    for (uint8_t k = 0; k < nBio; k++)
      if (bio[k].in == port && L::isInt(si) && bio[k].sI == si.as<long>()) idx = k;
    if (idx < 0) return setErr(res, "bio", "bad_si");
    removeAt(bio, nBio, idx);
    saveBio();
  } else if (!strcmp(o, "deleteAll")) {
    for (int k = nBio - 1; k >= 0; k--) if (bio[k].in == port) removeAt(bio, nBio, k);
    saveBio();
  }
  bioList(port, res);
}

void schList(uint8_t pi, JsonDocument &res) {
  res.clear();
  res["c"] = "sch";
  res["o"] = "list";
  res["m"] = mac;
  res["pi"] = pi;
  JsonArray items = res["items"].to<JsonArray>();
  uint8_t index = 0;
  for (uint8_t k = 0; k < nSch; k++) {
    if (sch[k].pi != pi) continue;
    JsonObject it = items.add<JsonObject>();
    it["index"] = index++;
    it["start"] = sch[k].start;
    it["end"] = sch[k].end;
    it["rm"] = sch[k].weekly ? "weekly" : "daily";
    it["dw"] = sch[k].dw;
  }
}

bool daySec(JsonVariantConst v) { return L::isInt(v) && v.as<long>() >= 0 && v.as<long>() < 86400; }

// sch: {"c":"sch","o":"insert","pi":0,"start":32400,"end":61200,"rm":"weekly","dw":[1,3]}
void cmdSch(JsonObjectConst j, JsonDocument &res) {
  const char *o = j["o"] | "";
  if (!isOp(o)) return setErr(res, "sch", "bad_o");
  JsonVariantConst pv = j["pi"];
  if (!L::isInt(pv) || pv.as<long>() < 0 || pv.as<long>() >= plc.outputCount()) return setErr(res, "sch", "bad_n");
  uint8_t pi = pv.as<long>();

  if (!strcmp(o, "insert")) {
    JsonVariantConst start = j["start"], end = j["end"];
    const char *rm = j["rm"] | "daily";
    bool weekly = !strcmp(rm, "weekly");
    if (!daySec(start) || !daySec(end) || (!weekly && strcmp(rm, "daily"))) return setErr(res, "sch", "bad_v");
    int8_t days[7];
    uint8_t nd = 0;
    if (weekly) {
      JsonVariantConst dw = j["dw"];
      bool seen[7] = {false};
      if (dw.is<JsonArrayConst>()) {
        for (JsonVariantConst d : dw.as<JsonArrayConst>()) {
          if (!L::isInt(d) || d.as<long>() < 0 || d.as<long>() > 6) return setErr(res, "sch", "bad_v");
          if (!seen[d.as<long>()]) { seen[d.as<long>()] = true; days[nd++] = d.as<long>(); }
        }
      } else if (L::isInt(dw) && dw.as<long>() >= 0 && dw.as<long>() <= 6) {
        days[nd++] = dw.as<long>();
      }
      if (nd == 0) return setErr(res, "sch", "bad_v");
    } else {
      days[nd++] = -1;
    }
    if (nSch + nd > MAX_RULES) return setErr(res, "sch", "full");
    for (uint8_t i = 0; i < nd; i++) {
      SchSlot &s = sch[nSch++];
      s.pi = pi;
      s.weekly = weekly ? 1 : 0;
      s.dw = days[i];
      s.pad = 0;
      s.start = start.as<long>();
      s.end = end.as<long>();
    }
    saveSch();
  } else if (!strcmp(o, "delete")) {
    JsonVariantConst si = slotArg(j);
    int idx = -1;
    if (L::isInt(si)) {
      long want = si.as<long>(), index = 0;
      for (uint8_t k = 0; k < nSch; k++) {
        if (sch[k].pi != pi) continue;
        if (index++ == want) { idx = k; break; }
      }
    }
    if (idx < 0) return setErr(res, "sch", "bad_si");
    removeAt(sch, nSch, idx);
    saveSch();
  } else if (!strcmp(o, "deleteAll")) {
    for (int k = nSch - 1; k >= 0; k--) if (sch[k].pi == pi) removeAt(sch, nSch, k);
    saveSch();
  }
  schList(pi, res);
}

void bsList(uint8_t ts, JsonDocument &res) {
  res.clear();
  res["c"] = "bs";
  res["o"] = "list";
  res["m"] = mac;
  res["ts"] = TS_NAME[ts];
  JsonArray items = res["items"].to<JsonArray>();
  for (uint8_t k = 0; k < nBs; k++) {
    if (bs[k].ts != ts) continue;
    JsonObject it = items.add<JsonObject>();
    it["sI"] = bs[k].sI;
    it["tr"] = bs[k].tr;
    it["tv"] = bs[k].tv;
    it["d"] = bs[k].d;
    it["du"] = bs[k].du;
    it["m"] = mac;
    it["n"] = bs[k].n;
    it["v"] = bs[k].v;
  }
}

// bs: {"c":"bs","o":"insert","ts":"temp","tr":1,"tv":30,"d":0,"du":0,"ps":[{"n":0,"v":1}]}
//     {"c":"bs","o":"cali","ts":"temp","v":25.0}  지금 값이 v가 되도록 보정
void cmdBs(JsonObjectConst j, JsonDocument &res) {
  const char *o = j["o"] | "";
  if (!strcmp(o, "calibration")) o = "cali";
  if (!isOp(o) && strcmp(o, "cali")) return setErr(res, "bs", "bad_o");
  const char *tsName = j["ts"] | "";
  int ts = -1;
  if (!strcmp(tsName, "temp") || !strcmp(tsName, "temperature")) ts = 0;
  if (!strcmp(tsName, "humi") || !strcmp(tsName, "humidity")) ts = 1;
  if (ts < 0 || !hasSensor()) return setErr(res, "bs", "bad_ts");

  if (!strcmp(o, "insert")) {
    JsonVariantConst tr = j["tr"], tv = j["tv"];
    uint32_t d, du;
    if (!L::isOnOff(tr) || !optSec(j["d"], d) || !optSec(j["du"], du)) return setErr(res, "bs", "bad_v");
    if (!L::isNum(tv) || tv.as<float>() < -40 || tv.as<float>() > 100) return setErr(res, "bs", "bad_v");
    JsonArrayConst ps = j["ps"];
    if (ps.isNull() || ps.size() != 1) return setErr(res, "bs", "bad_v");  // 출력은 하나만
    if (nBs >= MAX_RULES) return setErr(res, "bs", "full");
    BsRule &r = bs[nBs];
    const char *code;
    if (!parseTarget(ps[0], r.n, r.v, code)) return setErr(res, "bs", code);
    r.sI = nextBs++;
    r.ts = ts;
    r.tr = L::onOff(tr) ? 1 : 0;
    r.tv = tv.as<float>();
    r.d = d;
    r.du = du;
    r.last = -1;  // 처음 값은 -1: 등록할 때 이미 tv 이상이면 "올라갈 때"가 곧바로 실행된다 (v2.0과 같음)
    r.wait = false;
    nBs++;
    saveBs();
  } else if (!strcmp(o, "delete")) {
    JsonVariantConst si = slotArg(j);
    int idx = -1;
    for (uint8_t k = 0; k < nBs; k++)
      if (L::isInt(si) && bs[k].sI == si.as<long>()) idx = k;
    if (idx < 0) return setErr(res, "bs", "bad_si");
    removeAt(bs, nBs, idx);
    saveBs();
  } else if (!strcmp(o, "deleteAll")) {
    for (int k = nBs - 1; k >= 0; k--) if (bs[k].ts == ts) removeAt(bs, nBs, k);
    saveBs();
  } else if (!strcmp(o, "cali")) {
    JsonVariantConst v = j["v"];
    if (!L::isNum(v) || v.as<float>() < -40 || v.as<float>() > 100) return setErr(res, "bs", "bad_v");
    if (!plc.sensorOk() || isnan(sensorRaw(ts))) return setErr(res, "bs", "no_sensor");
    cal[ts] = L::round1(v.as<float>() - sensorRaw(ts));
    saveCal();
    Serial.printf("[보정] %s 측정 %.1f → %.1f (보정 %+.1f)\n", ts ? "습도" : "온도", sensorRaw(ts), v.as<float>(), cal[ts]);
    return fillState(res);
  }
  bsList(ts, res);
}

// name: {"c":"name","name":["전등","입력 1",...,"출력 4"]}  [0] 기기, 그다음 입력, 출력
void cmdName(JsonObjectConst j, JsonDocument &res) {
  JsonArrayConst a = j["name"];
  if (a.isNull() || a.size() != nameCount()) return setErr(res, "name", "bad_v");
  String tmp[1 + I2R_PLC::MAX_CH * 2];
  for (uint8_t i = 0; i < nameCount(); i++) {
    String s = a[i] | "";
    s.trim();
    if (s.length() > NAME_MAX_BYTES) return setErr(res, "name", "too_long");
    tmp[i] = s.length() ? s : defaultName(i);
  }
  for (uint8_t i = 0; i < nameCount(); i++) names[i] = tmp[i];
  saveNames();
  fillState(res);
}

void handleCmd(JsonObjectConst j, JsonDocument &res) {
  const char *c = j["c"] | "";
  if (!strcmp(c, "gs")) return fillState(res);
  if (!strcmp(c, "so")) return cmdSo(j, res);
  if (!strcmp(c, "st") || !strcmp(c, "setTime")) return cmdSt(j, res);
  if (!strcmp(c, "bio")) return cmdBio(j, res);
  if (!strcmp(c, "sch")) return cmdSch(j, res);
  if (!strcmp(c, "bs")) return cmdBs(j, res);
  if (!strcmp(c, "name")) return cmdName(j, res);
  setErr(res, *c ? c : "?", "unknown");
}

// JSON 한 줄을 처리하고 답을 res에 (웹과 시리얼이 함께 씀)
void runJson(const String &text, JsonDocument &res) {
  JsonDocument in;
  if (text.length() > 1024) return setErr(res, "?", "too_long");
  if (deserializeJson(in, text) || !in.is<JsonObject>()) return setErr(res, "?", "json");
  handleCmd(in.as<JsonObjectConst>(), res);
}

// ======================================================================
// 웹서버
// ======================================================================
void sendJson(JsonDocument &doc) {
  String s;
  serializeJson(doc, s);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", s);
}

void setupWeb() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
  });
  server.on("/logo.png", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "max-age=86400");  // 로고는 바뀌지 않으니 하루 동안 다시 받지 않는다
    server.send_P(200, "image/png", (const char *)LOGO_PNG, LOGO_PNG_LEN);
  });
  server.on("/state", HTTP_GET, [] {
    JsonDocument doc;
    fillState(doc);
    sendJson(doc);
  });
  server.on("/cmd", HTTP_POST, [] {
    JsonDocument res;
    runJson(server.arg("plain"), res);
    sendJson(res);
  });
  server.onNotFound([] { server.send(404, "text/plain; charset=utf-8", "없는 주소입니다"); });
  server.begin();
}

// ======================================================================
// 시간: 인터넷(NTP)으로 맞추고, 안 되면 보드 시계 그대로 계속
// ======================================================================
// - 켜질 때: 마지막에 저장한 시각에서 이어 간다 (I2R_Time.begin). 인터넷이 되면 곧 NTP로 고친다.
// - 1시간마다 NTP를 다시 요청한다. 받지 못하면 보드 시계를 그대로 쓴다 (규칙, 스케줄은 멈추지 않음).
// - 10분마다 지금 시각을 저장한다. 전원이 꺼졌다 켜져도 그 시각에서 이어 간다
//   (꺼져 있던 시간만큼은 늦어진다. 인터넷이나 화면의 "동기화"로 다시 맞춘다).
const unsigned long NTP_EVERY_MS = 3600000UL;  // 인터넷 시간 다시 받기: 1시간
const unsigned long NTP_WAIT_MS = 30000UL;     // 이 안에 답이 없으면 "받지 못함"으로 본다
const unsigned long SAVE_EVERY_MS = 600000UL;  // 시각 저장: 10분 (하루 144번, NVS 수명에 문제없음)

enum TimeSrc : uint8_t { T_NONE, T_SAVED, T_NTP, T_WEB };
TimeSrc timeSrc = T_NONE;
unsigned long ntpAt = 0;     // 마지막으로 인터넷 시간을 받은 millis (0 = 아직 없음)
unsigned long ntpAskAt = 0;  // 마지막으로 요청한 millis
bool ntpWaiting = false;
unsigned long clockSavedAt = 0;

const char *timeSrcName() {
  switch (timeSrc) {
    case T_SAVED: return "saved";
    case T_NTP: return "ntp";
    case T_WEB: return "web";
    default: return "none";
  }
}

// 지금 현지 시각을 I2R_Time과 같은 자리(NVS "i2r"의 savedTime)에 저장. 다시 켜면 여기서 이어 간다
void saveClock() {
  clockSavedAt = millis();
  if (!itime.valid()) return;
  Preferences p;
  if (p.begin("i2r", false)) {
    p.putULong("savedTime", (uint32_t)itime.now());
    p.end();
  }
}

void askNtp(const char *why) {
  ntpAskAt = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[시간] 인터넷 연결이 없습니다. 보드 시계 %s 그대로 계속합니다.\n",
                  itime.valid() ? itime.text().c_str() : "(시간 없음)");
    return;
  }
  if (esp_sntp_enabled()) esp_sntp_restart();  // 바로 요청 (처음 연결 때는 I2R_Time이 시작해 둠)
  ntpWaiting = true;
  Serial.printf("[시간] 인터넷 시간 요청 (%s)\n", why);
}

// 화면의 "동기화"로 맞췄을 때 (setEpoch가 시각을 이미 저장함)
void onWebTime() {
  timeSrc = T_WEB;
  clockSavedAt = millis();
}

// 인터넷 시간을 받은 지 몇 초 (아직 없으면 -1)
long ntpAgoSec() { return ntpAt ? (long)((millis() - ntpAt) / 1000) : -1; }

void timeBegin() {
  itime.begin();  // 시계가 비어 있으면 저장한 시각에서 이어 간다
  if (itime.restored()) {
    timeSrc = T_SAVED;
    Serial.printf("[시간] 저장된 시각 %s 에서 이어 갑니다. 인터넷 시간을 받으면 고칩니다.\n", itime.text().c_str());
  } else if (itime.valid()) {
    timeSrc = T_SAVED;  // 다시 시작(소프트 리셋)해도 시계가 남아 있는 경우
  } else {
    Serial.println("[시간] 아직 시간을 모릅니다. 인터넷 시간이나 화면의 \"동기화\"를 기다립니다. (스케줄은 시간을 알면 동작)");
  }
  clockSavedAt = millis();
}

void timeUpdate() {
  static bool wasWifi = false;
  itime.update();  // Wi-Fi가 처음 연결되면 NTP를 시작한다
  unsigned long now = millis();
  bool wifi = WiFi.status() == WL_CONNECTED;

  if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {  // 인터넷 시간을 받음 (읽으면 다시 비워짐)
    timeSrc = T_NTP;
    ntpAt = now;
    ntpWaiting = false;
    Serial.printf("[시간] 인터넷 시간으로 맞췄습니다: %s\n", itime.text().c_str());
    saveClock();
  }
  if (wifi && !wasWifi) askNtp("Wi-Fi 연결됨");                  // 켜질 때, 다시 연결될 때
  else if (now - ntpAskAt >= NTP_EVERY_MS) askNtp("1시간마다");  // 연결이 없으면 보드 시계로 계속
  wasWifi = wifi;

  if (ntpWaiting && now - ntpAskAt >= NTP_WAIT_MS) {
    ntpWaiting = false;
    Serial.printf("[시간] 인터넷 시간을 받지 못했습니다. 보드 시계 %s 그대로 계속합니다.\n",
                  itime.valid() ? itime.text().c_str() : "(시간 없음)");
  }
  if (now - clockSavedAt >= SAVE_EVERY_MS) saveClock();
}

// ======================================================================
// Wi-Fi: 저장된 값(COM 포트로 받은 si) → 없으면 스케치에 적은 값
// ======================================================================
bool wifiConfigured() { return wifiSsid.length() > 0; }

void loadWifi() {
  Preferences p;
  if (p.begin(WIFI_NS, true)) {  // 처음 켤 때는 저장 자리가 없다
    wifiSsid = p.getString("ssid", "");
    wifiPw = p.getString("pw", "");
    p.end();
  }
  if (wifiSsid.length() == 0) {
    wifiSsid = WIFI_SSID;
    wifiPw = WIFI_PASSWORD;
  }
}

// USB로 받은 {"c":"si","ssid":"..","password":"..","e":"..","mqttBroker":".."} 저장 (i2r.link 연결 → COM 포트)
// I2R_IoT(I2R_Net)와 같은 자리, 같은 이름으로 저장한다. 답 {"c":"si","ok":true} 뒤 다시 시작.
// 이메일(e)은 쓰지 않는다. 서버 칸의 i2r.link 기본값("i2r", mqtt.i2r.link)은 비운다 (공식 펌웨어 전용 서버).
void applySi(const String &text) {
  JsonDocument doc;
  if (deserializeJson(doc, text)) { Serial.println("{\"c\":\"si\",\"ok\":false,\"err\":\"json\"}"); return; }
  String ssid = doc["ssid"] | "";
  ssid.trim();
  if (ssid.length() == 0) { Serial.println("{\"c\":\"si\",\"ok\":false,\"err\":\"ssid\"}"); return; }
  String pw = doc["password"] | "";
  String broker = doc["mqttBroker"] | "";
  broker.trim();
  String lower = broker;
  lower.toLowerCase();
  if (lower == "i2r" || lower.endsWith("i2r.link")) broker = "";
  Preferences p;
  if (!p.begin(WIFI_NS, false)) { Serial.println("{\"c\":\"si\",\"ok\":false,\"err\":\"nvs\"}"); return; }
  p.clear();
  p.putString("ssid", ssid);
  p.putString("pw", pw);
  p.putString("broker", broker);  // standalone은 쓰지 않음 (② local, ③ cloud가 씀)
  p.putUShort("port", doc["mqttPort"] | (uint16_t)1883);
  p.putString("user", doc["mqttUser"] | "");
  p.putString("mpass", doc["mqttPass"] | "");
  p.putString("group", doc["g"] | "");
  p.end();
  Serial.println("{\"c\":\"si\",\"ok\":true}");
  Serial.printf("[Wi-Fi] 설정을 저장했습니다: \"%s\". 다시 시작합니다.\n", ssid.c_str());  // 비밀번호는 찍지 않는다
  Serial.flush();
  delay(1000);
  ESP.restart();
}

// ======================================================================
// Wi-Fi, LED, 시리얼
// ======================================================================
void wifiUpdate() {
  static bool was = false;
  static unsigned long lastTry = 0;
  if (!wifiConfigured()) {  // 설정이 없으면 10초마다 안내만 한다 (PLC와 규칙은 계속 돈다)
    if (millis() - lastTry >= 10000) {
      lastTry = millis();
      Serial.println("[Wi-Fi] 설정이 없습니다. i2r.link → 연결 → COM 포트(USB)에서 Wi-Fi 이름과 비밀번호를 보내세요.");
    }
    return;
  }
  bool now = WiFi.status() == WL_CONNECTED;
  if (now && !was) {
    Serial.println();
    Serial.printf("[Wi-Fi] 연결됨 \"%s\" (RSSI %d dBm)\n", wifiSsid.c_str(), WiFi.RSSI());
    Serial.printf("  웹 주소: http://%s\n", WiFi.localIP().toString().c_str());
    static bool mdns = false;
    if (!mdns && MDNS.begin(hostName.c_str())) {
      MDNS.addService("http", "tcp", 80);
      mdns = true;
    }
    if (mdns) Serial.printf("  또는:   http://%s.local\n\n", hostName.c_str());
  } else if (!now && was) {
    Serial.println("[Wi-Fi] 끊김. 다시 연결합니다.");
    lastTry = millis();
  }
  was = now;
  if (!now && millis() - lastTry >= 30000) {  // 30초마다 다시 시도
    lastTry = millis();
    Serial.printf("[Wi-Fi] \"%s\" 연결 시도 중...\n", wifiSsid.c_str());
    WiFi.disconnect();
    WiFi.begin(wifiSsid.c_str(), wifiPw.c_str());
  }
}

// 보드 LED: Wi-Fi가 연결되면 1초에 한 번 깜박이고, 아니면 계속 켜 둔다
void updateLed() {
  static unsigned long at = 0;
  static bool on = true;
  if (WiFi.status() != WL_CONNECTED) {
    if (!on) plc.led(on = true);
    return;
  }
  if (millis() - at >= 500) {
    at = millis();
    plc.led(on = !on);
  }
}

// 시리얼(USB)에서 JSON 한 줄: si는 Wi-Fi 설정 저장 (USB에서만 받음), 나머지는 웹과 같게 처리
void serialUpdate() {
  static String buf;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n') {
      buf.trim();
      if (buf.startsWith("{")) {
        JsonDocument head;
        deserializeJson(head, buf);
        if (!strcmp(head["c"] | "", "si")) {
          applySi(buf);
        } else {
          JsonDocument res;
          runJson(buf, res);
          serializeJson(res, Serial);
          Serial.println();
        }
      }
      buf = "";
    } else if (ch != '\r') {
      if (buf.length() < 1100) buf += ch;
      else buf = "";
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  plc.begin();  // 출력은 모두 꺼진 상태에서 시작
  plc.led(true);
  timeBegin();

  WiFi.mode(WIFI_STA);
  delay(100);
  mac = WiFi.macAddress();
  mac.toUpperCase();

  loadAll();
  loadWifi();
  setupWeb();

  Serial.println();
  Serial.printf("==== i2r-server %s: %s 혼자 동작 (보드 웹서버) ====\n", GROUP, plc.type());
  Serial.printf("보드 %s, MAC %s, 버전 %s\n", plc.type(), mac.c_str(), VERSION);
  Serial.printf("저장된 것: 입력 규칙 %u, 스케줄 %u, 센서 트리거 %u\n", nBio, nSch, nBs);
  if (hasSensor()) Serial.printf("온습도 센서: %s\n", plc.sensorOk() ? "있음" : "찾지 못함");

  if (wifiConfigured()) {
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(wifiSsid.c_str(), wifiPw.c_str());  // 기다리지 않는다. Wi-Fi가 없어도 PLC와 규칙은 돈다
    Serial.printf("[Wi-Fi] \"%s\" 연결 시도 중...\n", wifiSsid.c_str());
  }
}

void loop() {
  plc.update();    // 1) 입력 읽기, 자동 끄기, 센서. 항상 먼저
  timeUpdate();    // 2) 시간: 인터넷으로 맞추기 (1시간마다), 안 되면 보드 시계 그대로
  bioUpdate();     // 3) 입력 규칙
  schUpdate();     // 4) 스케줄
  bsUpdate();      // 5) 센서 트리거
  wifiUpdate();
  server.handleClient();
  serialUpdate();
  updateLed();
}
