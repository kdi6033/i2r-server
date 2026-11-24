// ============================================================
// light.ino - Otto 조명 (WS2812B LED 16개, 데이터 핀 GPIO1)
//
// 웹의 [조명] 패널(색상 선택 + 밝기)과 [LED 데모] 버튼이 보내는 ol 명령으로 켜고 끈다.
//
//   {"c":"ol","m":"..","ef":"sd","co":"ff0000","br":128}   단색으로 켜기 (co 색상, br 밝기 0~255)
//   {"c":"ol","m":"..","ef":"demo","br":128}                LED 데모 시작 (무지개 → 점멸 → 흐름 반복)
//   {"c":"ol","m":"..","ef":"off"}                          끄기 (데모도 멈춘다)
//   {"c":"ol","m":"..","ef":"get"}                          지금 상태 알려 달라는 요청
//
// 응답: {"c":"ol","m":"..","ef":"sd","co":"ff0000","br":128,"ok":1}
//        ef 는 지금 상태이다. 단색이면 "sd", 데모 중이면 "demo", 꺼져 있으면 "off".
//
// LED 데모는 ledDemo/ledDemo.ino 의 세 효과를 delay() 없이(시간표로) 옮긴 것이다.
//   ① 무지개 5초 → ② 점멸 5번(약 3초) → ③ 흐름 16칸(약 1.6초) → 처음으로
// 그래서 데모 중에도 MQTT 명령(끄기 등)을 바로 받는다.
// ============================================================

#include <FastLED.h>

#define LIGHT_PIN 1     // 데이터 핀 (D1)
#define LIGHT_COUNT 16  // LED 개수
#define LIGHT_MAX_MA 500 // 전류 상한(mA). 밝기를 올려도 이 전류를 넘지 않게 FastLED 가 자동으로 줄인다.

CRGB lightLeds[LIGHT_COUNT];
bool lightOn = false;          // 단색으로 켜져 있는지
uint8_t lightR = 255, lightG = 255, lightB = 255; // 마지막으로 고른 색
uint8_t lightBrightness = 128; // 밝기 (0 ~ 255)
bool lightChatActive = false;  // AI 대화 중인지. 대화 중에는 LED 가 대화 상태(녹색/빨간색)를 알려 주고, 조명·데모는 잠시 멈춘다

// ── LED 데모 상태 ───────────────────────────────────────────
bool lightDemoOn = false;        // 데모 중인지
uint8_t lightDemoPhase = 0;      // 0 무지개, 1 점멸, 2 흐름
int lightDemoStep = 0;           // 지금 효과 안에서 몇 번째 단계인지
unsigned long lightDemoNext = 0; // 다음 단계를 실행할 시각 (millis)
uint8_t lightHue = 0;            // 무지개 색상 변화용

const int DEMO_RAINBOW_STEPS = 100; // 무지개 100번 × 50ms
const int DEMO_RAINBOW_MS = 50;
const int DEMO_BLINK_STEPS = 10;    // 켜짐/꺼짐 각 5번 × 300ms
const int DEMO_BLINK_MS = 300;
const int DEMO_FLOW_MS = 100;       // 흐름: LED 한 칸마다 100ms

// 지금 상태를 LED 에 반영한다. (단색 또는 끔)
void lightApply() {
  if (lightChatActive)
    return; // AI 대화 중에는 대화 상태 색을 유지한다. 고른 색은 저장만 해 두고 대화가 끝나면 보여 준다
  if (lightOn) {
    fill_solid(lightLeds, LIGHT_COUNT, CRGB(lightR, lightG, lightB));
    FastLED.setBrightness(lightBrightness);
  } else {
    fill_solid(lightLeds, LIGHT_COUNT, CRGB::Black);
  }
  FastLED.show();
}

// 지금 상태를 웹으로 알린다.
void lightAck(bool ok, const char *err = nullptr) {
  char hex[7];
  snprintf(hex, sizeof(hex), "%02x%02x%02x", lightR, lightG, lightB);
  DynamicJsonDocument res(256);
  res["c"] = "ol";
  res["m"] = wifi.mac;
  res["ef"] = lightDemoOn ? "demo" : (lightOn ? "sd" : "off");
  res["co"] = hex;
  res["br"] = lightBrightness;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

void lightSetup() {
  FastLED.addLeds<WS2812B, LIGHT_PIN, GRB>(lightLeds, LIGHT_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LIGHT_MAX_MA);
  lightApply(); // 시작할 때는 꺼 둔다
}

// loop() 에서 계속 불린다. 데모 중일 때만 시간이 된 단계를 한 번 실행한다.
void lightLoop() {
  if (!lightDemoOn || lightChatActive)
    return; // 데모가 아니거나, AI 대화 중이면(대화 상태 색을 보여 주는 중) 건드리지 않는다
  unsigned long now = millis();
  if ((long)(now - lightDemoNext) < 0)
    return;

  switch (lightDemoPhase) {
  case 0: // 🌈 무지개
    fill_rainbow(lightLeds, LIGHT_COUNT, lightHue++);
    FastLED.show();
    lightDemoNext = now + DEMO_RAINBOW_MS;
    if (++lightDemoStep >= DEMO_RAINBOW_STEPS) {
      lightDemoPhase = 1;
      lightDemoStep = 0;
    }
    break;

  case 1: // ✨ 점멸: 짝수 단계는 켜짐(흰색), 홀수 단계는 꺼짐
    fill_solid(lightLeds, LIGHT_COUNT, (lightDemoStep % 2 == 0) ? CRGB::White : CRGB::Black);
    FastLED.show();
    lightDemoNext = now + DEMO_BLINK_MS;
    if (++lightDemoStep >= DEMO_BLINK_STEPS) {
      lightDemoPhase = 2;
      lightDemoStep = 0;
      fill_solid(lightLeds, LIGHT_COUNT, CRGB::Black); // 흐름은 모두 꺼진 상태에서 시작
    }
    break;

  case 2: // 💨 흐름: 파란 불이 앞에서 뒤로 한 칸씩 이동
    if (lightDemoStep < LIGHT_COUNT) {
      lightLeds[lightDemoStep] = CRGB::Blue;
      if (lightDemoStep > 0)
        lightLeds[lightDemoStep - 1] = CRGB::Black; // 이전 LED 끄기
      FastLED.show();
      lightDemoNext = now + DEMO_FLOW_MS;
      lightDemoStep++;
    } else { // 마지막 칸을 끄고 처음(무지개)으로 돌아간다
      lightLeds[LIGHT_COUNT - 1] = CRGB::Black;
      FastLED.show();
      lightDemoPhase = 0;
      lightDemoStep = 0;
      lightDemoNext = now;
    }
    break;
  }
}

// AI 대화(chat.ino)가 부르는 함수: 대화 상태를 LED 색으로 알려 준다.
//   녹색   = 말해도 되는 때 (말을 기다리는 중, 목소리를 입력받는 중)
//   빨간색 = 그 밖의 모든 때 (생각 중, 로봇이 말하는 중)
void lightChatColor(bool green) {
  lightChatActive = true;
  fill_solid(lightLeds, LIGHT_COUNT, green ? CRGB(0, 255, 0) : CRGB(255, 0, 0));
  FastLED.setBrightness(lightBrightness);
  FastLED.show();
}

// AI 대화가 끝났을 때: 대화 전의 조명 상태로 되돌린다. (데모 중이었다면 데모가 이어서 돈다)
void lightChatEnd() {
  if (!lightChatActive)
    return;
  lightChatActive = false;
  if (lightDemoOn)
    FastLED.setBrightness(lightBrightness); // 데모는 lightLoop() 가 다음 단계부터 이어서 그린다
  else
    lightApply();                           // 단색 또는 꺼짐 상태 복구
}

// OTTO 데모(otto.ino)가 부르는 함수: 로봇 데모가 도는 동안 LED 데모도 같이 돌린다.
// 지금 상태(demo / off)를 웹에도 알려서 [LED 데모] 버튼이 맞게 표시되게 한다.
void lightDemoBegin() {
  if (!lightDemoOn) { // 이미 데모 중이면 처음부터 다시 시작하지 않는다
    lightOn = false;
    lightDemoOn = true;
    lightDemoPhase = 0;
    lightDemoStep = 0;
    lightDemoNext = 0;
    FastLED.setBrightness(lightBrightness);
  }
  lightAck(true);
}

void lightDemoEnd() {
  lightDemoOn = false;
  lightOn = false;
  lightApply(); // LED 를 끈다
  lightAck(true);
}

// "rrggbb" 16진수 6자리를 읽는다. 형식이 맞지 않으면 false.
bool lightParseHex(const char *s, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (strlen(s) != 6 || strspn(s, "0123456789abcdefABCDEF") != 6)
    return false;
  long v = strtol(s, nullptr, 16);
  r = (v >> 16) & 0xFF;
  g = (v >> 8) & 0xFF;
  b = v & 0xFF;
  return true;
}

// ol 명령 처리
void lightCommand(JsonDocument &doc) {
  String ef = doc["ef"] | "";

  if (ef == "sd") {
    const char *co = doc["co"] | "";
    if (co[0] != '\0') { // 색을 보냈으면 바꾼다. 안 보냈으면 마지막 색 그대로
      uint8_t r, g, b;
      if (!lightParseHex(co, r, g, b)) {
        lightAck(false, "bad_param");
        return;
      }
      lightR = r;
      lightG = g;
      lightB = b;
    }
    if (!doc["br"].isNull())
      lightBrightness = constrain(doc["br"].as<int>(), 0, 255);
    lightDemoOn = false; // 단색으로 켜면 데모는 멈춘다
    lightOn = true;
    lightApply();
    Serial.printf("[조명] 켬 색=%02x%02x%02x 밝기=%d\n", lightR, lightG, lightB, lightBrightness);
    lightAck(true);
  } else if (ef == "demo") {
    if (!doc["br"].isNull()) // 데모 중에도 밝기는 바꿀 수 있다
      lightBrightness = constrain(doc["br"].as<int>(), 0, 255);
    FastLED.setBrightness(lightBrightness);
    if (!lightDemoOn) { // 이미 데모 중이면 밝기만 바꾸고 처음부터 다시 시작하지 않는다
      lightOn = false;
      lightDemoOn = true;
      lightDemoPhase = 0;
      lightDemoStep = 0;
      lightDemoNext = 0;
      Serial.println("[조명] LED 데모 시작");
    }
    lightAck(true);
  } else if (ef == "off") {
    lightDemoOn = false;
    lightOn = false;
    lightApply();
    Serial.println("[조명] 끔");
    lightAck(true);
  } else if (ef == "get") {
    lightAck(true);
  } else {
    lightAck(false, "bad_action"); // bk, fl 같은 단독 효과는 아직 없다
  }
}
