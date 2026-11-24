// ============================================================
// eyes.ino - Otto 눈 (OLED 128x64, SH1106, I2C)
//
//  - 로봇이 동작하는 동안(걷기·굴리기·미끄러짐·댄스·AI 대화, 명령을 받은 직후)은
//    눈동자가 움직이는 눈(lcd/lcd_act/lcd_act.ino)을 보여 준다.
//  - 2분 동안 아무 동작도 없으면 잠자는 눈(lcd/lcd_sleep/lcd_sleep.ino)으로 바뀐다.
//  - 새 명령이 오면 바로 깨어난다.
//
// 눈은 loop() 와 별도의 작업(FreeRTOS 태스크, 코어 0)에서 돌린다.
//  걷기·미끄러짐 한 걸음은 약 1초 동안 loop() 를 붙잡고 있는데, 별도 작업이라서
//  로봇이 움직이는 동안에도 눈동자가 끊기지 않고 계속 움직인다.
//
// OLED 연결 확인 (건전지에서도 눈이 안정적으로 나오도록):
//  - 건전지는 USB 보다 전원이 늦게 올라와서 켜자마자는 OLED 가 응답하지 않을 수 있다.
//    그래서 시작할 때 최대 5초 동안 응답을 기다렸다가, 응답하면 초기화한다.
//  - 시작한 뒤에는 연결을 계속 확인하지 않는다. (확인하거나 초기화할 때마다 화면이 깜빡이기 때문)
//  - loop() 에서 2분에 한 번만 eyesCheck() 로 확인한다.
//      처음 못 찾았으면 지금 초기화하고 시작하고, 이미 나오고 있으면 초기화 명령을 다시 보낸다.
//      (서보가 움직일 때 전원이 흔들려 OLED 가 리셋됐더라도 2분 안에 눈이 돌아온다)
//
// 연결: SDA = GPIO18, SCL = GPIO17 (i2r-05 의 I2C 단자)
// OLED 가 연결되어 있지 않으면 눈 작업은 아무것도 그리지 않는다. (다른 기능이 느려지지 않는다)
// ============================================================

#include <U8g2lib.h>
#include <Wire.h>

#define EYES_SDA 18
#define EYES_SCL 17
#define EYES_OLED_ADDR 0x3C                       // SH1106 I2C 주소
const unsigned long EYES_SLEEP_AFTER_MS = 120000; // 2분 동안 동작이 없으면 잠든다
const unsigned long EYES_FRAME_MS = 100;          // 눈동자를 움직이는 간격
const unsigned long EYES_CHECK_MS = 120000;       // OLED 연결 확인 간격: 2분
const unsigned long EYES_BOOT_WAIT_MS = 5000;     // 시작할 때 OLED 가 응답하길 기다리는 최대 시간

U8G2_SH1106_128X64_NONAME_F_HW_I2C eyesDisplay(U8G2_R0, U8X8_PIN_NONE);

SemaphoreHandle_t eyesMutex; // 눈 작업과 loop() 의 초기화가 OLED 를 동시에 쓰지 않게 한다

// loop() 와 눈 작업이 같이 읽고 쓰는 값은 volatile 로 둔다.
volatile bool eyesPresent = false;       // OLED 를 찾아서 초기화했는지
volatile bool eyesAsleep = false;        // 지금 잠자는 눈을 보여 주는 중인지
volatile bool eyesNeedDraw = true;       // 화면을 새로 그려야 하는지
volatile unsigned long eyesLastActive = 0; // 마지막으로 동작한 시각
unsigned long eyesLastFrame = 0;
int eyesPupilX = 0, eyesPupilY = 0;
int eyesDirX = 1, eyesDirY = 1;
volatile bool eyesClockMode = false;     // true 이면 눈 대신 큰 시계를 보여 준다 (웹의 [시계] 버튼)
time_t eyesClockLastSec = 0;             // 큰 시계를 마지막으로 그린 초 (초가 바뀌면 다시 그린다)

// 큰 시계: 화면 가득 "HH:MM" 을 크게 그린다. 멀리서도 보이도록 들어가는 가장 큰 글꼴을 고른다.
// 콜론(:)은 1초마다 깜빡인다. 시간을 아직 못 받았으면 "NO TIME" 을 보여 준다. (뮤텍스를 잡은 상태에서 부른다)
void eyesDrawBigClock() {
  char text[6];
  eyesDisplay.clearBuffer();
  eyesDisplay.setDrawColor(1);
  if (!clockText(text)) {
    eyesDisplay.setFont(u8g2_font_helvB14_tr);
    const char *msg = "NO TIME";
    eyesDisplay.drawStr((128 - eyesDisplay.getStrWidth(msg)) / 2, 38, msg);
    return;
  }
  static const uint8_t *fonts[] = {u8g2_font_logisoso42_tn, u8g2_font_logisoso38_tn, u8g2_font_logisoso34_tn,
                                   u8g2_font_logisoso30_tn, u8g2_font_logisoso26_tn};
  for (const uint8_t *f : fonts) {
    eyesDisplay.setFont(f);
    if (eyesDisplay.getStrWidth(text) <= 126)
      break; // 화면 가로(128)에 들어가는 가장 큰 글꼴
  }
  char hh[3] = {text[0], text[1], 0};
  char mm[3] = {text[3], text[4], 0};
  int total = eyesDisplay.getStrWidth(text);
  int x0 = (128 - total) / 2;
  int xColon = x0 + eyesDisplay.getStrWidth(hh);
  int xMin = x0 + total - eyesDisplay.getStrWidth(mm); // 분은 오른쪽 끝에 맞춘다
  int y = (64 + eyesDisplay.getAscent()) / 2; // 세로 가운데
  eyesDisplay.drawStr(x0, y, hh);
  if (time(nullptr) % 2 == 0) // 짝수 초에만 콜론을 보여 준다
    eyesDisplay.drawStr(xColon, y, ":");
  eyesDisplay.drawStr(xMin, y, mm);
}

// ── 깨어 있는 눈 (lcd_act.ino) ──────────────────────────────
// 눈 하나: 굵은 테두리 + 큰 눈동자. 화면이 128x64 라서 눈 하나는 지름 약 60 (반지름 29)이 최대다.
void eyesDrawOpenEye(int cx, int cy, int px, int py) {
  const int outerR = 29; // 눈 바깥 반지름
  const int lineW = 4;   // 테두리 굵기(픽셀)
  const int pupilR = 11; // 눈동자 반지름

  eyesDisplay.setDrawColor(1);
  eyesDisplay.drawDisc(cx, cy, outerR, U8G2_DRAW_ALL);
  eyesDisplay.setDrawColor(0);
  eyesDisplay.drawDisc(cx, cy, outerR - lineW, U8G2_DRAW_ALL);

  eyesDisplay.setDrawColor(1);
  eyesDisplay.drawDisc(cx + px, cy + py, pupilR, U8G2_DRAW_ALL);
}

void eyesDrawAwake() {
  // 안쪽 반지름 25, 눈동자 반지름 11 → 가로 ±8, 세로 ±6 안에서 움직이면 테두리를 넘지 않는다.
  eyesPupilX += eyesDirX;
  eyesPupilY += eyesDirY;
  if (abs(eyesPupilX) > 8) eyesDirX = -eyesDirX;
  if (abs(eyesPupilY) > 6) eyesDirY = -eyesDirY;

  if (xSemaphoreTake(eyesMutex, pdMS_TO_TICKS(500))) { // loop() 가 초기화하는 중이면 기다린다
    eyesDisplay.clearBuffer();
    eyesDrawOpenEye(32, 32, eyesPupilX, eyesPupilY); // 왼쪽 눈
    eyesDrawOpenEye(96, 32, eyesPupilX, eyesPupilY); // 오른쪽 눈
    eyesDisplay.sendBuffer();
    xSemaphoreGive(eyesMutex);
  }
}

// ── 화난 눈 ─────────────────────────────────────────────────
// Wi-Fi 나 MQTT 가 연결되지 않았을 때 보여 준다. 눈 위쪽을 안쪽으로 비스듬히 가려서 찌푸린 표정을 만든다.
// dir = +1 이면 왼쪽 눈(안쪽이 오른쪽), -1 이면 오른쪽 눈
void eyesDrawAngryEye(int cx, int cy, int dir) {
  const int outerR = 29;
  const int pupilR = 11;
  eyesDisplay.setDrawColor(1);
  eyesDisplay.drawDisc(cx, cy, outerR, U8G2_DRAW_ALL);
  eyesDisplay.setDrawColor(0);
  eyesDisplay.drawDisc(cx, cy, outerR - 4, U8G2_DRAW_ALL);
  eyesDisplay.setDrawColor(1);
  eyesDisplay.drawDisc(cx + dir * 4, cy + 6, pupilR, U8G2_DRAW_ALL); // 눈동자는 안쪽 아래를 노려본다

  // 눈썹선: 바깥쪽이 높고 안쪽이 낮다. 그 위쪽은 지운다.
  int xo = cx - dir * 32, yo = cy - 18; // 바깥쪽 끝 (높음)
  int xi = cx + dir * 32, yi = cy + 2;  // 안쪽 끝 (낮음)
  eyesDisplay.setDrawColor(0);
  eyesDisplay.drawTriangle(xo, yo, xi, yi, xo, cy - 34);
  eyesDisplay.drawTriangle(xi, yi, xi, cy - 34, xo, cy - 34);
  eyesDisplay.setDrawColor(1);
  eyesDrawThickLine(xo, yo, xi, yi, 5);
}

void eyesDrawAngry() {
  if (xSemaphoreTake(eyesMutex, pdMS_TO_TICKS(500))) {
    eyesDisplay.clearBuffer();
    eyesDrawAngryEye(32, 34, +1);
    eyesDrawAngryEye(96, 34, -1);
    eyesDisplay.sendBuffer();
    xSemaphoreGive(eyesMutex);
  }
}

// ── 잠자는 눈 (lcd_sleep.ino) ───────────────────────────────
void eyesDrawThickPoint(int x, int y, int r) {
  eyesDisplay.drawDisc(x, y, r, U8G2_DRAW_ALL);
}

void eyesDrawThickLine(int x1, int y1, int x2, int y2, int w) {
  int h = w / 2;
  for (int ox = -h; ox <= h; ox++) {
    for (int oy = -h; oy <= h; oy++) {
      if (ox * ox + oy * oy <= h * h + 1) { // 모서리를 둥글게
        eyesDisplay.drawLine(x1 + ox, y1 + oy, x2 + ox, y2 + oy);
      }
    }
  }
}

void eyesDrawSleepingEye(int cx, int cy) {
  const int radius = 30;       // 눈꺼풀 원호의 반지름
  const int line_w = 5;        // 눈꺼풀 굵기(픽셀)
  const int eyelash_len = 8;   // 속눈썹 길이
  const int eyelash_w = 3;     // 속눈썹 굵기(픽셀)
  const int eyelash_step = 15; // 속눈썹 간격(도)
  const int start_deg = 30;
  const int end_deg = 150;

  // 눈꺼풀: 아래로 휘어진 부드러운 원호 (점마다 작은 원판을 찍어서 굵게)
  for (int angle = start_deg; angle <= end_deg; angle++) {
    float rad = angle * PI / 180.0;
    int x = cx + radius * cos(rad);
    int y = cy + radius * sin(rad);
    eyesDrawThickPoint(x, y, line_w / 2);
  }

  // 속눈썹
  for (int angle = start_deg; angle <= end_deg; angle += eyelash_step) {
    float rad = angle * PI / 180.0;
    int x1 = cx + radius * cos(rad);
    int y1 = cy + radius * sin(rad);
    float dx = cos(rad + PI / 2.0);
    float dy = sin(rad + PI / 2.0);
    int x2 = x1 + eyelash_len * dx;
    int y2 = y1 + eyelash_len * dy;
    eyesDrawThickLine(x1, y1, x2, y2, eyelash_w);
  }
}

void eyesDrawAsleep() {
  if (xSemaphoreTake(eyesMutex, pdMS_TO_TICKS(500))) {
    eyesDisplay.clearBuffer();
    eyesDrawSleepingEye(32, 10); // 왼쪽 눈
    eyesDrawSleepingEye(96, 10); // 오른쪽 눈
    eyesDisplay.sendBuffer();
    xSemaphoreGive(eyesMutex);
  }
}

// ── 부팅 상태 문구 ──────────────────────────────────────────
// eyesStatus() 로 문구를 정하면 눈 대신 그 문구를 보여 준다. (눈 작업이 그리므로 loop() 가 막혀 있어도 갱신된다)
// holdMs = 0 이면 다음 eyesStatus() 가 올 때까지, 아니면 그 시간 뒤에 눈으로 돌아간다.
portMUX_TYPE eyesMsgMux = portMUX_INITIALIZER_UNLOCKED;
char eyesMsg1[40] = "", eyesMsg2[40] = "";
volatile bool eyesMsgOn = false;
volatile bool eyesMsgDirty = false;
volatile unsigned long eyesMsgUntil = 0; // 0 = 계속

void eyesStatus(const char *line1, const char *line2, unsigned long holdMs) {
  portENTER_CRITICAL(&eyesMsgMux);
  strlcpy(eyesMsg1, line1 ? line1 : "", sizeof(eyesMsg1));
  strlcpy(eyesMsg2, line2 ? line2 : "", sizeof(eyesMsg2));
  eyesMsgUntil = holdMs ? millis() + holdMs : 0;
  eyesMsgOn = true;
  eyesMsgDirty = true;
  portEXIT_CRITICAL(&eyesMsgMux);
}

// 문구를 보여 주는 중이면 true (그려야 하면 그린다). 시간이 지났으면 문구를 끄고 눈으로 돌아간다.
bool eyesStatusStep() {
  if (!eyesMsgOn)
    return false;
  if (eyesMsgUntil && (long)(millis() - eyesMsgUntil) >= 0) {
    eyesMsgOn = false;
    eyesNeedDraw = true; // 눈을 다시 그린다
    return false;
  }
  if (eyesMsgDirty) {
    char m1[40], m2[40];
    portENTER_CRITICAL(&eyesMsgMux);
    memcpy(m1, eyesMsg1, sizeof(m1));
    memcpy(m2, eyesMsg2, sizeof(m2));
    eyesMsgDirty = false;
    portEXIT_CRITICAL(&eyesMsgMux);
    if (xSemaphoreTake(eyesMutex, pdMS_TO_TICKS(500))) {
      eyesDisplay.clearBuffer();
      eyesDisplay.setDrawColor(1);
      eyesDisplay.setFont(u8g2_font_unifont_t_korean2); // 한글 16x16 (한 줄 최대 8자)
      eyesDisplay.drawUTF8((128 - eyesDisplay.getUTF8Width(m1)) / 2, m2[0] ? 26 : 38, m1);
      if (m2[0])
        eyesDisplay.drawUTF8((128 - eyesDisplay.getUTF8Width(m2)) / 2, 54, m2);
      eyesDisplay.sendBuffer();
      xSemaphoreGive(eyesMutex);
    } else {
      eyesMsgDirty = true; // 못 그렸으면 다시 시도
    }
  }
  eyesLastActive = millis(); // 문구를 보는 동안은 잠들지 않는다
  return true;
}

// ── OLED 확인 / 초기화 ──────────────────────────────────────
void eyesTask(void *param);

// OLED 가 I2C 에 응답하는지 확인한다.
bool eyesProbe() {
  Wire.beginTransmission(EYES_OLED_ADDR);
  return Wire.endTransmission() == 0;
}

// OLED 를 처음 초기화한다. (뮤텍스를 잡고 부른다)
void eyesInitDisplay() {
  delay(200); // 전원이 안정될 때까지 잠깐
  eyesDisplay.begin();
  eyesDisplay.setBusClock(400000); // 화면 한 번 갱신하는 시간을 줄인다 (약 25ms)
}

// ── 외부에서 쓰는 함수 ──────────────────────────────────────
// setup() 에서 부른다. I2C 와 눈 작업만 시작하고, OLED 를 기다리는 일은 눈 작업이 하므로
// OLED 가 없거나 늦게 켜져도 부팅이 늦어지지 않는다.
void eyesSetup() {
  Wire.begin(EYES_SDA, EYES_SCL);
  eyesMutex = xSemaphoreCreateMutex();
  eyesLastActive = millis();
  eyesAsleep = false;
  eyesNeedDraw = true;

  // 눈을 그리는 일은 별도 작업으로 돌린다. (코어 0, 낮은 우선순위)
  xTaskCreatePinnedToCore(eyesTask, "eyes", 6144, nullptr, 1, nullptr, 0);
}

// loop() 에서 계속 부른다. 2분에 한 번만 실제로 확인한다.
//  - OLED 가 응답하지 않으면 아무것도 하지 않는다. (2분 뒤에 다시 본다)
//  - 아직 못 찾았는데 응답하면 지금 초기화하고 눈을 시작한다.
//  - 이미 눈이 나오고 있으면 초기화 명령을 다시 보낸다. (리셋된 OLED 복구, 화면이 잠깐 깜빡일 수 있다)
void eyesCheck() {
  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck < EYES_CHECK_MS)
    return;
  lastCheck = now;

  if (!eyesProbe())
    return;

  xSemaphoreTake(eyesMutex, portMAX_DELAY);
  if (!eyesPresent) {
    eyesInitDisplay();
    eyesPresent = true;
    Serial.println("[눈] OLED 를 찾아서 초기화했습니다.");
  } else {
    eyesDisplay.initDisplay();
    eyesDisplay.setPowerSave(0);
  }
  xSemaphoreGive(eyesMutex);
  eyesNeedDraw = true; // 잠자는 눈은 한 번만 그리므로, 리셋됐을 때를 위해 다시 그리게 한다
}

// 동작 명령을 받았을 때 부른다: 자고 있었다면 깨운다.
void eyesWake() {
  eyesLastActive = millis();
  if (eyesAsleep) {
    eyesAsleep = false;
    eyesNeedDraw = true;
  }
}

// 눈 상태를 한 단계 진행한다. 눈 작업이 계속 부르고, 시간이 되었을 때만 화면을 갱신한다.
void eyesStep() {
  if (!eyesPresent)
    return;

  if (eyesStatusStep()) // 부팅 상태 문구를 보여 주는 중
    return;

  unsigned long now = millis();

  // Wi-Fi 나 MQTT 가 연결되지 않았으면 화난 눈을 보여 준다. (Wi-Fi 없이 쓰는 모드는 제외, 잠들지 않는다)
  static bool wasAngry = false;
  if (!wifiOff && (WiFi.status() != WL_CONNECTED || !wifi.isConnectedMqtt)) {
    eyesLastActive = now;
    eyesAsleep = false;
    if (!wasAngry || eyesNeedDraw) {
      eyesDrawAngry();
      eyesNeedDraw = false;
      wasAngry = true;
    }
    return;
  }
  if (wasAngry) { // 연결되면 보통 눈으로 돌아간다
    wasAngry = false;
    eyesNeedDraw = true;
  }

  // 움직이는 중이거나 AI 대화 중이면 동작 중이다.
  if (otto.current != nullptr || chatOn)
    eyesLastActive = now;

  // 시계 모드: 눈 대신 큰 시계를 보여 준다. 초가 바뀔 때만 다시 그린다. (잠들지 않는다)
  if (eyesClockMode) {
    eyesLastActive = now;
    eyesAsleep = false;
    time_t sec = time(nullptr);
    if (eyesNeedDraw || sec != eyesClockLastSec) {
      eyesClockLastSec = sec;
      eyesNeedDraw = false;
      if (xSemaphoreTake(eyesMutex, pdMS_TO_TICKS(500))) {
        eyesDrawBigClock();
        eyesDisplay.sendBuffer();
        xSemaphoreGive(eyesMutex);
      }
    }
    return;
  }

  // 2분 동안 동작이 없으면 잠든다.
  if (!eyesAsleep && now - eyesLastActive >= EYES_SLEEP_AFTER_MS) {
    eyesAsleep = true;
    eyesNeedDraw = true;
    Serial.println("[눈] 2분 동안 동작이 없어 잠듭니다.");
  }

  if (eyesAsleep) {
    if (eyesNeedDraw) { // 잠자는 눈은 한 번만 그린다
      eyesDrawAsleep();
      eyesNeedDraw = false;
    }
    return;
  }

  // AI 대화 중에는 화면 갱신(I2C 약 25ms)이 음성 재생·녹음을 방해하지 않도록 눈동자를 멈춘다.
  if (chatOn) {
    if (eyesNeedDraw) {
      eyesDrawAwake();
      eyesNeedDraw = false;
    }
    return;
  }

  if (eyesNeedDraw || now - eyesLastFrame >= EYES_FRAME_MS) {
    eyesLastFrame = now;
    eyesNeedDraw = false;
    eyesDrawAwake();
  }
}

// 눈 작업: loop() 가 걷기·미끄러짐으로 바빠도 계속 돌면서 눈동자를 움직인다.
void eyesTask(void *param) {
  // 시작 직후: 최대 5초 동안 OLED 가 응답하길 기다렸다가 초기화한다. (건전지는 전원이 늦게 올라온다)
  unsigned long t0 = millis();
  while (!eyesPresent && millis() - t0 < EYES_BOOT_WAIT_MS) {
    if (eyesProbe()) {
      xSemaphoreTake(eyesMutex, portMAX_DELAY);
      if (!eyesPresent) { // 기다리는 사이에 loop() 의 확인이 먼저 초기화했을 수도 있다
        eyesInitDisplay();
        eyesPresent = true;
        Serial.println("[눈] OLED 초기화 완료");
      }
      xSemaphoreGive(eyesMutex);
    } else {
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
  if (!eyesPresent)
    Serial.println("[눈] OLED 가 응답하지 않습니다. 2분마다 다시 확인합니다.");

  for (;;) {
    eyesStep();
    vTaskDelay(pdMS_TO_TICKS(20)); // 20ms 마다 시간을 확인한다 (화면은 EYES_FRAME_MS 마다 갱신)
  }
}

// ck: 시계 (웹의 [시계] 버튼). a = on(큰 시계 보기) / off(눈으로 돌아가기) / get(상태만 알려 달라)
//   응답: {"c":"ck","m":"..","a":"on"|"off","ok":1}
void clockCommand(JsonDocument &doc) {
  String a = doc["a"] | "get";
  if (a == "on" || a == "off") {
    eyesClockMode = (a == "on");
    eyesAsleep = false; // 시계를 끄면 눈은 깨어난 상태로 돌아간다
    eyesLastActive = millis();
    eyesNeedDraw = true;
    Serial.println(eyesClockMode ? "[시계] 큰 시계 표시" : "[시계] 눈 화면으로 돌아감");
  } else if (a != "get") {
    DynamicJsonDocument bad(128);
    bad["c"] = "ck";
    bad["m"] = wifi.mac;
    bad["ok"] = 0;
    bad["err"] = "bad_action";
    String o;
    serializeJson(bad, o);
    wifi.publishOut(o);
    return;
  }
  DynamicJsonDocument res(128);
  res["c"] = "ck";
  res["m"] = wifi.mac;
  res["a"] = eyesClockMode ? "on" : "off";
  res["ok"] = 1;
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}
