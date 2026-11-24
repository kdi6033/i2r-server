// i2r-otto.ino: Otto 로봇 혼자 동작하는 펌웨어 (i2r-server 그룹 ① standalone)
//
// i2r-server 펌웨어는 세 그룹으로 나뉜다. 이 파일은 ①이다.
//   ① standalone  보드 혼자 동작. 보드가 웹서버가 되어 같은 공유기의 휴대폰, PC에서 제어
//   ② local       현장 서버: 여러 대 + i2r-05(MQTT 서버)
//   ③ aws         아마존(AWS) 서버와 연결해 인터넷 너머에서 제어
//
// 보드가 직접 웹페이지를 띄운다. 같은 공유기에 연결된 휴대폰이나 PC 브라우저에서
// 시리얼 모니터, OLED 에 나온 주소(http://192.168.x.x) 또는 http://otto.local 을 연다.
//
// [화면에서 하는 것] (i2r.link 의 로봇 화면과 같음)
//   조종판     걷기 / 굴리기 / 미끄러짐. 전진·후진은 가운데에 가까울수록 느리게(40), 바깥일수록 빠르게(200)
//   버튼       댄스(+내 음악), 순찰, AI 대화, 라디오, 말하기, 받아쓰기, 시계, 책읽기, 예약, 내 음악, 조명,
//              LED 데모, OTTO 데모, 로봇 맞추기
//
// [Wi-Fi 설정] 두 가지 중 하나
//   1) i2r.link 웹 → 연결 → COM 포트(USB)에서 Wi-Fi 이름, 비밀번호를 보낸다.
//      보드가 {"c":"si",...}를 받아 SSID, 비밀번호만 저장하고 다시 시작한다. (Email, Broker 칸은 쓰지 않음)
//   2) 개발용: secrets.example.h 를 secrets.h 로 복사해 WIFI_SSID, WIFI_PASSWORD 를 적는다. 1)로 저장한 값이 먼저다.
//   저장 자리는 I2R_IoT 와 같다 (NVS "i2riot"). 그래서 ② local, ③ aws 펌웨어로 바꿔도 Wi-Fi 가 이어진다.
//
// [파일 나눠 맡기]
//   i2r-otto.ino  이 파일: Wi-Fi, 웹서버, 명령 나누기, setup/loop
//   page.h        웹 화면 (HTML, JavaScript 한 파일)
//   motion.h      Otto 동작과 보정 (OttoI2R 라이브러리)
//   chat.ino      AI 대화 (보드 마이크 → Gemini → 스피커)     radio.ino   인터넷 라디오
//   speak.ino     말하기 (책읽기도 이것을 씀)                    music.ino   내 음악 (SPIFFS 의 mp3)
//   light.ino     조명, LED 데모 (WS2812B 16개)                   eyes.ino    OLED 눈, 큰 시계
//   clock.ino     시계 (I2R_Time)                                schedule.ino 예약
//   patrol.ino    순찰 (거리 센서로 장애물 피하기)
//   받아쓰기, 책읽기는 화면(브라우저)이 하고, 보드는 조명(ol)과 말하기(sk) 명령만 받는다.
//
// [명령 모양] i2r 프로토콜(https://github.com/kdi6033/i2r-server/blob/main/protocol.md 10장, Otto 초안)과
//   같은 JSON 을 웹에서 POST /cmd 로 보낸다. 예: {"c":"ot","a":"wf","du":0,"sp":100}
//   보드의 답과 알림(라디오 제목, 대화 기록, 말하기 끝 등)은 화면이 GET /ev 로 0.5초마다 가져간다.
//   시리얼 모니터에 같은 JSON 한 줄을 넣어도 된다 (답은 시리얼로).
//
// [필요한 라이브러리] OttoI2R, I2R_IoT, ArduinoJson 7, PubSubClient(I2R_IoT 가 씀),
//   ESP32-audioI2S(schreibfaul1), U8g2, FastLED, Adafruit_VL53L0X
// [보드 설정] ESP32S3 Dev Module, Flash 16MB, Partition Scheme "Custom"(이 폴더의 partitions.csv),
//   PSRAM "OPI PSRAM", USB CDC On Boot "Enabled"
//
// [주의]
// - 같은 공유기에 붙은 사람은 누구나 이 화면으로 로봇을 움직일 수 있다. 공용 Wi-Fi 에서 쓰지 않는다.
// - 비밀번호는 secrets.h 에만 적는다 (.gitignore 로 GitHub 에 올라가지 않음).
// - 화면이 3초 넘게 말이 없으면(휴대폰 화면 꺼짐, Wi-Fi 끊김) 로봇이 스스로 멈춘다.
// - 2.4GHz Wi-Fi 만 된다. 걷다가 멈추면 USB 전원이 모자란 것이다 (외부 건전지를 쓴다).

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include "esp_system.h"
#include <I2R_IoT.h>  // I2R_Time(시간), I2R_Link(값 검사 함수)
#include <OttoI2R.h>  // Otto 동작 (walkFront, rollingFront, slideFront ...)
#include "page.h"

// ===== 개발용 Wi-Fi (선택): secrets.example.h 를 secrets.h 로 복사해 내 값을 넣는다 =====
#if !defined(I2R_RELEASE) && __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""  // 비워 두면 i2r.link → 연결 → COM 포트로 넣는다
#define WIFI_PASSWORD ""
#endif

// ===== 보드 정보 =====
const char *BOARD_TYPE = "i2r-otto";
const char *GROUP = "standalone";
const char *VERSION = "standalone-0.1.0";
const char *WIFI_NS = "i2riot";  // Wi-Fi 저장 자리 (I2R_IoT 의 I2R_Net 과 같음)
const unsigned long WEB_SILENT_STOP_MS = 3000;  // 화면이 이 시간 동안 말이 없으면 동작을 멈춘다

WebServer server(80);
I2R_Time itime;           // 시간: NTP, 화면이 보낸 st, 껐다 켜도 이어 가기 (clock.ino, schedule.ino)
Preferences preferences;  // chat.ino(API 키), 공장 초기화에서 쓴다
String wifiSsid, wifiPw, hostName;

// 상태 LED: GPIO48 의 RGB LED. Wi-Fi 가 연결되면 1초에 한 번 깜박이고, 아니면 계속 켜 둔다
const int ledPin = 48;
void statusLed(bool on) { rgbLedWrite(ledPin, on ? 40 : 0, 0, 0); }

// ======================================================================
// 다른 파일의 함수 (Arduino 는 이 파일 뒤에 다른 .ino 를 이어 붙인다)
// ======================================================================
void chatSetEnabled(bool on);
bool chatBusy();
void chatLoop();
void chatLoadKey();
void chatKeyCommand(JsonDocument &doc);
void radioCommand(JsonDocument &doc);
void radioLoop();
void speakCommand(JsonDocument &doc);
void speakLoop();
void schedCommand(JsonDocument &doc);
void scheduleLoop();
void schedLoad();
void musicCommand(JsonDocument &doc);
void musicLoop();
bool musicWebBegin(size_t size, const String &title);
void musicWebWrite(const uint8_t *buf, size_t n);
void musicWebEnd(bool ok);
void lightSetup();
void lightDemoBegin();
void lightDemoEnd();
void lightLoop();
void lightCommand(JsonDocument &doc);
void patrolReset();
void patrolRun();
void eyesSetup();
void eyesCheck();
void eyesWake();
void eyesStatus(const char *line1, const char *line2 = "", unsigned long holdMs = 0);
void clockCommand(JsonDocument &doc);
void clockLoop();
bool clockSynced();

// ======================================================================
// 보드가 보내는 답과 알림 모으기
// ======================================================================
// 다른 파일들은 답을 wifi.publishOut(JSON) 으로 보낸다 (원본 펌웨어에서는 MQTT 로 나갔다).
// standalone 에서는 그것을 여기 모아 두고, 화면이 GET /ev?s=번호 로 새 것만 가져간다.
const int EV_MAX = 32;
String evList[EV_MAX];
uint32_t evNext = 0;  // 다음 알림 번호

bool serialReply = false;     // 시리얼로 받은 명령을 처리하는 중이면 답을 시리얼로 보낸다
bool commandViaMqtt = false;  // music.ino 가 본다. standalone 에는 MQTT 가 없어서 늘 false
bool wifiOff = false;         // Wi-Fi 설정이 없으면 true (라디오, AI 대화처럼 인터넷이 필요한 일을 건너뛴다)

struct WifiCompat {
  String mac;                    // 기기 ID (MAC 대문자)
  bool isConnectedMqtt = false;  // eyes.ino 가 본다. standalone 에서는 Wi-Fi 연결 상태

  void publishOut(const String &json) {
    if (serialReply) {
      Serial.println(json);
      return;
    }
    evList[evNext % EV_MAX] = json;
    evNext++;
  }
  void loop();  // chat.ino 가 녹음하는 동안 불러서 화면의 요청(대화 끄기 등)을 받는다. 아래 setupWeb 뒤에 있다
} wifi;

#include "motion.h"  // Otto 동작과 보정 (otto 객체). wifi 를 쓰므로 여기서 불러온다

// ======================================================================
// 명령 나누기: 웹(/cmd), 시리얼 모두 여기로 온다. 처리했으면 true
// ======================================================================
bool handleCommand(const String &c, JsonDocument &doc) {
  if (c == "ot" || c == "lm" || c == "oc" || c == "rd" || c == "sk" || c == "mu")
    eyesWake();  // 명령이 오면 잠자던 눈이 깨어난다

  if (c == "ot") {  // 동작 (motion.h)
    String a = doc["a"] | "";
    if (chatBusy() && a != "stop") {  // AI 대화 중에는 정지 말고는 받지 않는다
      otto.ack(a, false, "busy");
      return true;
    }
    otto.command(a, doc["du"] | OTTO_DEFAULT_SEC, doc["sp"] | OTTO_SPEED_DEFAULT);
    return true;
  }
  if (c == "oc") { ottoCalCommand(doc); return true; }     // 보정 (motion.h)
  if (c == "ak") { chatKeyCommand(doc); return true; }     // API 키 (chat.ino)
  if (c == "lm") { chatSetEnabled(doc["v"].as<int>() != 0); return true; }  // AI 대화 켜기/끄기
  if (c == "ck") { clockCommand(doc); return true; }       // 큰 시계 (eyes.ino)
  if (c == "ol") { lightCommand(doc); return true; }       // 조명, LED 데모 (light.ino)
  if (c == "rd") { radioCommand(doc); return true; }       // 라디오 (radio.ino)
  if (c == "sk") { speakCommand(doc); return true; }       // 말하기 (speak.ino)
  if (c == "mu") { musicCommand(doc); return true; }       // 내 음악 (music.ino)
  if (c == "os") { schedCommand(doc); return true; }       // 예약 (schedule.ino)

  if (c == "st" || c == "setTime") {  // 시간 맞추기: epoch = 현지 시각(초), tz = 시간대(초)
    JsonVariantConst epoch = doc["epoch"], tz = doc["tz"];
    bool ok = I2R_Link::isNum(epoch) &&
              itime.setEpoch((time_t)epoch.as<double>(), I2R_Link::isNum(tz) ? tz.as<long>() : itime.tz());
    wifi.publishOut(String("{\"c\":\"st\",\"ok\":") + (ok ? 1 : 0) + ",\"time\":\"" + itime.text() + "\"}");
    return true;
  }
  return false;
}

// JSON 한 줄을 처리한다. 결과 {"ok":1} 또는 {"ok":0,"err":".."}
String runJson(const String &text) {
  JsonDocument doc;
  if (deserializeJson(doc, text) || !doc.is<JsonObject>()) return "{\"ok\":0,\"err\":\"json\"}";
  String c = doc["c"] | "";
  if (c == "si") return "{\"ok\":0,\"err\":\"usb_only\"}";  // Wi-Fi 설정은 USB 로만
  if (!handleCommand(c, doc)) return "{\"ok\":0,\"err\":\"unknown\"}";
  return "{\"ok\":1}";
}

// ======================================================================
// 웹서버
// ======================================================================
unsigned long lastWebAt = 0;  // 화면이 마지막으로 말을 건 시각
bool moveByWeb = false;       // 지금 동작을 웹에서 시작했는지 (시리얼로 시작한 동작은 멈추지 않는다)

// GET /ev?s=번호 : 번호부터의 새 알림과 지금 상태를 돌려준다
//   {"s":다음번호,"a":"wf","tm":"10:44","ts":1,"ev":[{..},{..}]}   a 지금 동작, tm 보드 시각, ts 시간을 맞췄는지
void sendEvents() {
  uint32_t from = server.hasArg("s") ? (uint32_t)server.arg("s").toInt() : evNext;
  if (from > evNext) from = 0;                              // 보드가 다시 켜진 경우
  if (evNext - from > (uint32_t)EV_MAX) from = evNext - EV_MAX;  // 너무 오래된 것은 사라졌다
  String out = "{\"s\":" + String(evNext);
  out += ",\"a\":\"" + String(otto.current ? otto.current->code : "") + "\"";
  char hm[6];
  out += ",\"tm\":\"" + String(clockText(hm) ? hm : "") + "\"";
  out += ",\"ts\":" + String(clockSynced() ? 1 : 0);
  out += ",\"ev\":[";
  for (uint32_t i = from; i < evNext; i++) {
    if (i > from) out += ',';
    out += evList[i % EV_MAX];
  }
  out += "]}";
  server.send(200, "application/json; charset=utf-8", out);
}

void setupWeb() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
  });
  // 기기 정보 (화면 맨 위)
  server.on("/info", HTTP_GET, [] {
    lastWebAt = millis();
    String out = "{\"m\":\"" + wifi.mac + "\",\"t\":\"" + BOARD_TYPE + "\",\"ver\":\"" + VERSION +
                 "\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"host\":\"" + hostName + "\"}";
    server.send(200, "application/json; charset=utf-8", out);
  });
  server.on("/ev", HTTP_GET, [] {
    lastWebAt = millis();
    sendEvents();
  });
  server.on("/cmd", HTTP_POST, [] {
    lastWebAt = millis();
    String body = server.arg("plain");
    if (body.indexOf("\"ak\"") < 0) Serial.println("[웹] " + body.substring(0, 120));  // API 키는 찍지 않는다
    String res = runJson(body);
    if (body.indexOf("\"ot\"") >= 0) moveByWeb = otto.current != nullptr;  // 웹이 시작한 동작인지 기억
    server.send(200, "application/json; charset=utf-8", res);
  });
  // 내 음악 저장: POST /upload?sz=바이트&tx=제목 (multipart 파일 하나). 결과는 /ev 의 mu put 알림
  server.on(
      "/upload", HTTP_POST,
      [] {
        lastWebAt = millis();
        server.send(200, "application/json; charset=utf-8", "{\"ok\":1}");
      },
      [] {
        HTTPUpload &up = server.upload();
        if (up.status == UPLOAD_FILE_START) {
          otto.stop();
          musicWebBegin((size_t)server.arg("sz").toInt(), server.arg("tx"));
        } else if (up.status == UPLOAD_FILE_WRITE) {
          musicWebWrite(up.buf, up.currentSize);
        } else if (up.status == UPLOAD_FILE_END) {
          musicWebEnd(true);
        } else if (up.status == UPLOAD_FILE_ABORTED) {
          musicWebEnd(false);
        }
        lastWebAt = millis();
      });
  server.onNotFound([] { server.send(404, "text/plain; charset=utf-8", "없는 주소입니다"); });
  server.begin();
}

// chat.ino 가 녹음하는 동안(몇 초씩 걸림) 불러서 화면의 요청을 계속 받는다
void WifiCompat::loop() {
  isConnectedMqtt = WiFi.status() == WL_CONNECTED;
  server.handleClient();
}

// 화면이 3초 넘게 말이 없으면 웹에서 시작한 동작을 멈춘다 (휴대폰 화면이 꺼졌거나 Wi-Fi 가 끊겼을 때)
void webWatch() {
  if (!otto.current) {
    moveByWeb = false;
    return;
  }
  if (moveByWeb && millis() - lastWebAt > WEB_SILENT_STOP_MS) {
    Serial.println("[안전] 화면이 3초 동안 말이 없어 멈춥니다");
    otto.stop();
    otto.ack("stop", true);
    moveByWeb = false;
  }
}

// ======================================================================
// Wi-Fi
// ======================================================================
void loadWifi() {
  Preferences p;
  if (p.begin(WIFI_NS, true)) {
    wifiSsid = p.getString("ssid", "");
    wifiPw = p.getString("pw", "");
    p.end();
  }
  if (wifiSsid.length() == 0) {
    wifiSsid = WIFI_SSID;
    wifiPw = WIFI_PASSWORD;
  }
}

// USB 로 받은 {"c":"si","ssid":"..","password":".."} 저장 (i2r.link 연결 → COM 포트)
// I2R_Net 과 같은 자리, 같은 이름(ssid, pw). 답 {"c":"si","ok":true} 뒤 다시 시작한다.
void applySi(const String &text) {
  JsonDocument doc;
  if (deserializeJson(doc, text)) { Serial.println("{\"c\":\"si\",\"ok\":false,\"err\":\"json\"}"); return; }
  String ssid = doc["ssid"] | "";
  ssid.trim();
  if (ssid.length() == 0) { Serial.println("{\"c\":\"si\",\"ok\":false,\"err\":\"ssid\"}"); return; }
  Preferences p;
  if (!p.begin(WIFI_NS, false)) { Serial.println("{\"c\":\"si\",\"ok\":false,\"err\":\"nvs\"}"); return; }
  p.clear();
  p.putString("ssid", ssid);
  p.putString("pw", doc["password"] | "");
  p.end();
  Serial.println("{\"c\":\"si\",\"ok\":true}");
  Serial.printf("[Wi-Fi] 설정을 저장했습니다: \"%s\". 다시 시작합니다.\n", ssid.c_str());  // 비밀번호는 찍지 않는다
  Serial.flush();
  delay(1000);
  ESP.restart();
}

void wifiUpdate() {
  static bool was = false;
  static unsigned long lastTry = 0;
  wifi.isConnectedMqtt = WiFi.status() == WL_CONNECTED;
  if (wifiOff) {
    if (millis() - lastTry >= 10000) {
      lastTry = millis();
      Serial.println("[Wi-Fi] 설정이 없습니다. i2r.link → 연결 → COM 포트(USB)에서 Wi-Fi 이름과 비밀번호를 보내세요.");
    }
    return;
  }
  bool now = wifi.isConnectedMqtt;
  if (now && !was) {
    String ip = WiFi.localIP().toString();
    Serial.printf("\n[Wi-Fi] 연결됨 \"%s\" (RSSI %d dBm)\n", wifiSsid.c_str(), WiFi.RSSI());
    Serial.printf("  웹 주소: http://%s\n", ip.c_str());
    static bool mdns = false;
    if (!mdns && MDNS.begin(hostName.c_str())) {
      MDNS.addService("http", "tcp", 80);
      mdns = true;
    }
    if (mdns) Serial.printf("  또는:   http://%s.local\n\n", hostName.c_str());
    eyesStatus(ip.c_str(), (hostName + ".local").c_str(), 8000);  // OLED 에 주소를 8초 보여 준다
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

void updateLed() {
  static unsigned long at = 0;
  static bool on = true;
  if (!wifi.isConnectedMqtt) {
    if (!on) statusLed(on = true);
    return;
  }
  if (millis() - at >= 500) {
    at = millis();
    statusLed(on = !on);
  }
}

// ======================================================================
// 시리얼(USB): si 는 Wi-Fi 저장, 나머지는 웹과 같게 처리하고 답은 시리얼로
// ======================================================================
void serialUpdate() {
  static String buf;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      buf.trim();
      if (buf.startsWith("{")) {
        JsonDocument head;
        deserializeJson(head, buf);
        if (!strcmp(head["c"] | "", "si")) {
          applySi(buf);
        } else {
          serialReply = true;
          Serial.println(runJson(buf));
          serialReply = false;
        }
      }
      buf = "";
    } else if (buf.length() < 600) {
      buf += ch;
    } else {
      buf = "";
    }
  }
}

// ======================================================================
// 공장 초기화 스위치 (SW3: GPIO41 또는 GPIO16, 누르면 LOW)
//   짧게 누르면 Wi-Fi 설정과 API 키를 지우고, 3초 이상 누르면 내 음악(SPIFFS)까지 지운 뒤 다시 시작한다
// ======================================================================
const int RESET_PIN1 = 41, RESET_PIN2 = 16;
bool resetPressed() { return digitalRead(RESET_PIN1) == LOW || digitalRead(RESET_PIN2) == LOW; }

void checkFactoryReset() {
  if (!resetPressed()) return;
  unsigned long t0 = millis();
  while (resetPressed()) delay(10);
  unsigned long held = millis() - t0;
  if (held < 100) return;  // 잡음
  if (held >= 3000) {
    Serial.println("[초기화] 3초 이상: 내 음악까지 모두 지웁니다");
    SPIFFS.format();
  }
  Preferences p;
  p.begin(WIFI_NS, false);
  p.clear();
  p.end();
  p.begin("cfg", false);
  p.remove("gemini");
  p.end();
  Serial.println("[초기화] Wi-Fi 설정과 API 키를 지웠습니다. 다시 시작합니다.");
  delay(1000);
  ESP.restart();
}

// ======================================================================
void setup() {
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  delay(300);
  pinMode(RESET_PIN1, INPUT_PULLUP);
  pinMode(RESET_PIN2, INPUT_PULLUP);

  // 조명(FastLED)을 상태 LED 보다 먼저 준비한다. 둘 다 RMT 장치를 쓰는데, 상태 LED(rgbLedWrite)가 먼저
  // 자리를 잡으면 FastLED 가 "rmt: intr_priority conflict" 로 채널을 만들지 못해 조명이 켜지지 않는다.
  lightSetup();   // 조명 (꺼 둔다)
  statusLed(true);

  itime.begin();  // 저장된 시간대와 마지막 시각
  otto.setup();   // 보정값 (NVS 에 저장된 값이 있으면 그 값)
  chatLoadKey();  // Gemini API 키
  schedLoad();    // 예약
  eyesSetup();    // OLED 눈

  if (!SPIFFS.begin()) {  // 내 음악 저장 자리
    Serial.println("SPIFFS 를 처음 준비합니다 (포맷)...");
    eyesStatus("SPIFFS 포맷중", "기다려 주세요..");
    SPIFFS.format();
    ESP.restart();
  }

  WiFi.mode(WIFI_STA);
  delay(100);
  wifi.mac = WiFi.macAddress();
  wifi.mac.toUpperCase();
  hostName = "otto";  // http://otto.local (같은 공유기에 Otto 가 여러 대면 이름이 겹치니 IP 주소로 연다)

  loadWifi();
  setupWeb();

  Serial.println();
  Serial.printf("==== i2r-server %s: Otto 혼자 동작 (보드 웹서버) ====\n", GROUP);
  Serial.printf("보드 %s, MAC %s, 버전 %s\n", BOARD_TYPE, wifi.mac.c_str(), VERSION);

  if (wifiSsid.length() == 0) {
    wifiOff = true;
    eyesStatus("와이파이 설정", "필요해요", 6000);
  } else {
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(wifiSsid.c_str(), wifiPw.c_str());  // 기다리지 않는다
    Serial.printf("[Wi-Fi] \"%s\" 연결 시도 중...\n", wifiSsid.c_str());
    eyesStatus("와이파이", "접속 중...", 10000);
  }
}

void loop() {
  checkFactoryReset();
  serialUpdate();
  eyesCheck();
  wifiUpdate();
  server.handleClient();  // 화면의 요청
  itime.update();         // Wi-Fi 가 붙으면 NTP 로 시간 맞추기
  clockLoop();
  webWatch();
  otto.loop();            // 동작 한 걸음 (걷기, 미끄러짐은 한 번에 약 1초)
  if (!wifiOff) {
    chatLoop();   // AI 대화 (인터넷 필요)
    radioLoop();  // 라디오 (인터넷 필요)
    speakLoop();  // 말하기 (인터넷 필요)
  }
  scheduleLoop();
  musicLoop();
  lightLoop();
  updateLed();
}
