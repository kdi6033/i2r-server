// motion.h - Otto 동작과 보정 (OttoI2R 라이브러리를 부르는 부분)
//
// 원본 펌웨어(otto/otto.ino)의 동작 부분을 그대로 옮겼다. i2r-otto.ino 가 #include 한다.
//   ot 명령 → otto.command(동작 코드, 시간, 속도) → loop() 마다 otto.loop() 가 한 걸음씩 움직인다.
//   oc 명령 → ottoCalCommand() 보정값 조회, 바꾸기, 저장 (웹의 [로봇 맞추기])
//
// 동작 코드 (a)
//   걷기   wf 전진  wb 후진  wl 좌회전  wr 우회전   (wf, wb 는 속도 sp 40~200%)
//   굴리기 rf rb rl rr                              (rf, rb 는 속도)
//   미끄러짐 sf sb sl sr
//   댄스 dn (dl 왼발만, dr 오른발만)  순찰 pt  OTTO 데모 od  정지 stop
//   du = 유지 시간(초). 0 이면 stop 이 올 때까지 계속
//
// .h 로 둔 까닭: chat.ino, eyes.ino, patrol.ino 가 otto 객체를 쓰는데, Arduino 는 메인 .ino 다음에
// 다른 .ino 를 이어 붙이므로 otto 는 메인 파일 앞쪽에서 만들어져야 한다.

#pragma once

// 로봇 보정값. 아래 값은 "기본값"이고, 웹의 [굴림보정]/[걷기보정]으로 바꿔 NVS("ottocal")에 저장하면
// 부팅할 때 저장된 값으로 바뀐다. (OttoI2R 라이브러리의 ottoLoadCalibration)
const int DEF_YL_PULSE_CENTER = 1250;
const int DEF_YR_PULSE_CENTER = 1450;
const int DEF_RL_SPEED_US = 450;
const int DEF_RR_SPEED_US = 550;
const int DEF_WALK_TILT_R = 20;
const int DEF_WALK_TILT_L = 23;
int YL_PULSE_CENTER = DEF_YL_PULSE_CENTER; // 왼쪽 다리가 수직(90도)일 때의 펄스폭(us)
int YR_PULSE_CENTER = DEF_YR_PULSE_CENTER; // 오른쪽 다리
int RL_SPEED_US = DEF_RL_SPEED_US;         // 왼발 속도
int RR_SPEED_US = DEF_RR_SPEED_US;         // 오른발 속도
int WALK_TILT_R = DEF_WALK_TILT_R;         // 걷기: 오른발을 들 때 기울이는 각도
int WALK_TILT_L = DEF_WALK_TILT_L;         // 걷기: 왼발을 들 때
const int SLIDE_TILT_R = 5;       // 미끄러짐
const int SLIDE_TILT_L = 6;

const int OTTO_DEFAULT_SEC = 2; // 명령에 du 가 없을 때 동작을 유지하는 시간(초)
const int OTTO_MAX_SEC = 30;    // du 상한
const int OTTO_SPEED_DEFAULT = 100; // 명령에 sp 가 없을 때 속도(%) = 보정한 보통 속도
const int OTTO_SPEED_MIN = 40;      // sp 하한(%)
const int OTTO_SPEED_MAX = 200;     // sp 상한(%)

enum OttoMode { OTTO_WALK, OTTO_SLIDE, OTTO_ROLL, OTTO_DANCE, OTTO_DEMO, OTTO_PATROL };

bool hipsNeedNeutral = false; // 굴리기 자세에서 넘어와서 엉덩이를 가운데로 먼저 맞춰야 할 때 true

// 제자리에서 오른쪽으로 돌기 (slide_right 동작): slideLeft() 와 좌우 대칭이다.
//  - slideLeft : 오른쪽 다리(YR)만 기울이고, 왼발 시계 / 오른발 반시계
//  - 이 함수   : 왼쪽 다리(YL)만 기울이고, 왼발 반시계 / 오른발 시계
// (라이브러리의 slideRight()는 slideFront 를 좌우로 뒤집은 동작이라 제자리 회전이 아니다.)
void slideTurnRight() {
  const int RAMP_STEPS = 12; // 다리를 나눠서 옮기는 단계 수
  const int RAMP_MS = 120;   // 다리를 옮기는 시간(ms)
  const int ROLL_MS = 200;   // 두 발을 굴리는 시간(ms)
  static int curYL = 90;
  static bool started = false;

  // 처음이거나 굴리기 자세에서 넘어왔으면 엉덩이를 가운데로 맞춘다.
  // (moveHip 은 처음 호출될 때 라이브러리가 서보를 초기화하기도 한다)
  if (!started || hipsNeedNeutral) {
    moveHip(90, 90);
    curYL = 90;
    started = true;
    hipsNeedNeutral = false;
  }

  // 왼쪽 다리를 기울인다
  int target = 90 - SLIDE_TILT_L;
  for (int i = 1; i <= RAMP_STEPS; i++) {
    writeYL(curYL + (target - curYL) * i / RAMP_STEPS);
    delay(RAMP_MS / RAMP_STEPS);
  }
  curYL = target;

  // 두 발을 같은 시점에 굴린다: 왼발 반시계, 오른발 시계 (slideLeft 와 반대 방향)
  ottoRollFeet(RR_SPEED_US, -RL_SPEED_US);
  delay(ROLL_MS);
  ottoRollFeet(0, 0);

  // 다리를 90도로 되돌린다
  target = 90;
  for (int i = 1; i <= RAMP_STEPS; i++) {
    writeYL(curYL + (target - curYL) * i / RAMP_STEPS);
    delay(RAMP_MS / RAMP_STEPS);
  }
  curYL = target;
}

// 댄스: 오른발 딛고 좌회전→우회전, 왼발 딛고 좌회전→우회전 을 계속 반복한다.
// 중간에 멈출 수 있도록 delay() 를 쓰지 않고 시간표(단계 목록)로 진행한다.
//  kind: 'H' 엉덩이 각도(a=YR, b=YL), 'A' 왼발 서기(라이브러리의 댄스 각도), 'B' 오른발 서기(같음),
//        'L' 왼발 속도(a), 'R' 오른발 속도(a)
//  엉덩이 127 = 왼쪽으로 기울여 왼발을 들고 오른발로 선다. 53 = 오른발을 들고 왼발로 선다.
//  서 있는 쪽의 반대(들린) 발을 굴려 몸을 돌린다. 발 속도: 양수 = 앞으로, 음수 = 뒤로
const int DANCE_HIP_MS = 700;    // 엉덩이를 옮긴 뒤 기다리는 시간 (OTTO 데모와 같음, 이전 2000)
const int DANCE_ROLL_MS = 1000;  // 발을 굴리는 시간 (이전 2000)
const int DANCE_PAUSE_MS = 300;  // 발을 멈추고 기다리는 시간 (이전 1000)
const int DANCE_SPEED = 300;     // 발 속도 (이전 150)
// 한 발로 설 때 엉덩이 각도는 라이브러리의 댄스 각도(getDanceTilts)를 쓴다. 웹의 [로봇 맞추기] 5번에서 바꾸고 NVS 에 저장한다.

struct DanceStep {
  char kind;
  int a;
  int b;
  int ms;
};

const DanceStep danceSteps[] = {
    {'H', 90, 90, DANCE_HIP_MS},                  // 가운데
    {'A', 0, 0, DANCE_HIP_MS},                    // 왼발 딛기 (오른발 듦)
    {'L', -DANCE_SPEED, 0, DANCE_ROLL_MS},        // 좌회전
    {'L', 0, 0, DANCE_PAUSE_MS},                  // 멈춤
    {'L', DANCE_SPEED, 0, DANCE_ROLL_MS},         // 우회전
    {'L', 0, 0, DANCE_PAUSE_MS},                  // 멈춤
    {'H', 90, 90, DANCE_HIP_MS},                  // 가운데
    {'B', 0, 0, DANCE_HIP_MS},                    // 오른발 딛기 (왼발 듦)
    {'R', DANCE_SPEED, 0, DANCE_ROLL_MS},         // 좌회전
    {'R', 0, 0, DANCE_PAUSE_MS},                  // 멈춤
    {'R', -DANCE_SPEED, 0, DANCE_ROLL_MS},        // 우회전
    {'R', 0, 0, DANCE_PAUSE_MS},                  // 멈춤
};
const int DANCE_STEP_COUNT = sizeof(danceSteps) / sizeof(danceSteps[0]);

// 엉덩이 단계('H' 고정 각도, 'A' 왼발 서기, 'B' 오른발 서기)를 적용한다. 'A'/'B' 는 매번 라이브러리 값을 읽어서
// 웹에서 바꾼 각도가 다음 단계부터 바로 반영된다.
// (아두이노가 만드는 함수 선언이 DanceStep 보다 앞에 놓이므로 구조체 대신 값으로 받는다)
void danceMoveHip(char kind, int a, int b) {
  int rYr, rYl, lYr, lYl;
  if (kind == 'A') {
    getDanceTilts(rYr, rYl, lYr, lYl);
    moveHip(rYr, rYl);
  } else if (kind == 'B') {
    getDanceTilts(rYr, rYl, lYr, lYl);
    moveHip(lYr, lYl);
  } else {
    moveHip(a, b);
  }
}

int danceIndex = -1;          // 지금 단계 (-1 = 아직 시작 전)
unsigned long danceUntil = 0; // 이 시각까지 지금 단계를 유지한다

void danceReset() { danceIndex = -1; }

// loop() 에서 계속 불린다. 단계 시간이 지났을 때만 다음 단계를 적용한다.
// from~to-1 번 단계만 반복한다. (0~5 = 왼발 서기, 6~11 = 오른발 서기, 0~12 = 전체)
void danceRunRange(int from, int to) {
  unsigned long now = millis();
  if (danceIndex >= 0 && (long)(now - danceUntil) < 0)
    return;
  int next = danceIndex + 1;
  if (danceIndex < from || next >= to) // 시작 전이거나 범위를 벗어났거나 끝까지 왔으면 처음 단계로
    next = from;
  danceIndex = next;
  const DanceStep &s = danceSteps[danceIndex];
  switch (s.kind) {
  case 'H':
  case 'A':
  case 'B':
    danceMoveHip(s.kind, s.a, s.b);
    break;
  case 'L':
    rollLeftFoot(s.a);
    break;
  case 'R':
    rollRightFoot(s.a);
    break;
  }
  danceUntil = millis() + s.ms; // 서보 초기화 등으로 시간이 걸렸어도 단계 시간은 다시 잰다
}

void danceRun() { danceRunRange(0, DANCE_STEP_COUNT); }          // dn: 전체 댄스
void danceRunLeft() { danceRunRange(0, DANCE_STEP_COUNT / 2); }  // dl: 왼발 서기만 (좌회전→우회전 반복)
void danceRunRight() { danceRunRange(DANCE_STEP_COUNT / 2, DANCE_STEP_COUNT); } // dr: 오른발 서기만

// ============================================================
// OTTO 데모 (ottoDemo/ottoDemo.ino 와 같은 순서). 정지 명령이 올 때까지 계속 반복한다.
//   걷기    : 앞으로 3초, 뒤로 3초
//   미끄러짐: 앞으로, 뒤로, 좌회전, 우회전 각 2초
//   굴리기  : 앞으로, 뒤로, 좌회전, 우회전 각 2초
//   댄스    : 오른발로 딛고 좌회전→우회전, 왼발로 딛고 좌회전→우회전 (빠른 속도)
// LED 데모(light.ino)와 눈(eyes.ino)도 같이 돈다.
//
// ottoDemo.ino 는 delay() 로 쭉 진행하지만, 여기서는 정지·종료 명령을 받을 수 있게
// 한 번 불릴 때 걸음 하나(약 1초)만 하고 돌아오는 "단계 목록" 방식으로 만들었다.
// ============================================================
const unsigned long DEMO_WALK_MS = 3000;   // 걷기 한 방향 시간
const unsigned long DEMO_SLIDE_MS = 2000;  // 미끄러짐 한 방향 시간
const unsigned long DEMO_ROLL_MS = 2000;   // 굴리기 한 방향 시간
const unsigned long DEMO_PAUSE_MS = 300;   // 미끄러짐·굴리기 방향을 바꾸기 전에 쉬는 시간

enum DemoKind {
  DEMO_WALK_F, DEMO_WALK_B,
  DEMO_SLIDE_F, DEMO_SLIDE_B, DEMO_SLIDE_L, DEMO_SLIDE_R,
  DEMO_ROLL_PREP, DEMO_ROLL_F, DEMO_ROLL_B, DEMO_ROLL_L, DEMO_ROLL_R, DEMO_ROLL_END,
  DEMO_DANCE
};
const DemoKind demoOrder[] = {
    DEMO_WALK_F,  DEMO_WALK_B,
    DEMO_SLIDE_F, DEMO_SLIDE_B, DEMO_SLIDE_L, DEMO_SLIDE_R,
    DEMO_ROLL_PREP, DEMO_ROLL_F, DEMO_ROLL_B, DEMO_ROLL_L, DEMO_ROLL_R, DEMO_ROLL_END,
    DEMO_DANCE};
const int DEMO_COUNT = sizeof(demoOrder) / sizeof(demoOrder[0]);

// 데모의 댄스 (ottoDemo.ino 의 빠른 댄스): 엉덩이는 라이브러리의 댄스 각도, 발 속도 300
const int DEMO_DANCE_SPEED = 300;
const DanceStep demoDanceSteps[] = {
    {'H', 90, 90, 700},                    // 가운데
    {'A', 0, 0, 700},                     // 왼발로 디디기 (오른발 듦)
    {'L', -DEMO_DANCE_SPEED, 0, 1000},     // 좌회전
    {'L', 0, 0, 300},                      // 멈춤
    {'L', DEMO_DANCE_SPEED, 0, 1000},      // 우회전
    {'L', 0, 0, 300},
    {'H', 90, 90, 700},                    // 가운데
    {'B', 0, 0, 700},                     // 오른발로 디디기 (왼발 듦)
    {'R', DEMO_DANCE_SPEED, 0, 1000},      // 좌회전
    {'R', 0, 0, 300},
    {'R', -DEMO_DANCE_SPEED, 0, 1000},     // 우회전
    {'R', 0, 0, 300},
    {'H', 90, 90, 500},                    // 가운데로 되돌려 걷기 자세로
};
const int DEMO_DANCE_COUNT = sizeof(demoDanceSteps) / sizeof(demoDanceSteps[0]);

int demoIdx = -1;                 // 지금 단계 (demoOrder 의 위치)
bool demoEntered = false;         // 지금 단계를 시작했는지
unsigned long demoStart = 0;      // 지금 단계를 시작한 시각
unsigned long demoWaitUntil = 0;  // 이 시각까지는 아무것도 하지 않고 기다린다 (단계 사이 쉬는 시간)
int demoDanceIdx = -1;            // 댄스 단계 안에서의 위치
unsigned long demoDanceUntil = 0;

void ottoDemoReset() {
  demoIdx = -1;
  demoEntered = false;
  demoWaitUntil = 0;
  demoDanceIdx = -1;
}

// 지금 단계를 끝내고 다음 단계로 넘어가게 한다. pauseMs 동안은 기다린다.
void demoFinishStep(unsigned long pauseMs) {
  demoEntered = false;
  demoWaitUntil = millis() + pauseMs;
}

// loop() 에서 계속 불린다. 한 번에 걸음 하나(약 1초 이하)만 하고 돌아온다.
void ottoDemoRun() {
  unsigned long now = millis();
  if ((long)(now - demoWaitUntil) < 0)
    return; // 쉬는 중

  // ── 새 단계를 시작한다 ──
  if (!demoEntered) {
    demoIdx = (demoIdx + 1) % DEMO_COUNT;
    demoEntered = true;
    demoStart = now;
    switch (demoOrder[demoIdx]) {
    case DEMO_WALK_F:
    case DEMO_WALK_B: // 걷기용 기울기 (오른발 20, 왼발 23)
      ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, WALK_TILT_R, WALK_TILT_L, RL_SPEED_US, RR_SPEED_US);
      break;
    case DEMO_SLIDE_F:
    case DEMO_SLIDE_B:
    case DEMO_SLIDE_L:
    case DEMO_SLIDE_R: // 미끄러짐용 기울기 (오른발 5, 왼발 6)
      ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, SLIDE_TILT_R, SLIDE_TILT_L, RL_SPEED_US, RR_SPEED_US);
      break;
    case DEMO_ROLL_PREP: // 엉덩이를 끝까지 돌려 굴리기 자세로 (YR 0, YL 180)
      moveHip(0, 180);
      demoFinishStep(1000);
      return;
    case DEMO_ROLL_END: // 엉덩이를 가운데로 되돌려 걷기 자세로
      moveHip(90, 90);
      demoFinishStep(500);
      return;
    case DEMO_DANCE:
      demoDanceIdx = -1;
      break;
    default:
      break;
    }
  }

  // ── 지금 단계를 진행한다 ──
  bool timeUp;
  switch (demoOrder[demoIdx]) {
  case DEMO_WALK_F:
    if (millis() - demoStart >= DEMO_WALK_MS) { demoFinishStep(0); return; }
    walkFront();
    break;
  case DEMO_WALK_B:
    if (millis() - demoStart >= DEMO_WALK_MS) { demoFinishStep(0); return; }
    walkBack();
    break;

  case DEMO_SLIDE_F:
  case DEMO_SLIDE_B:
  case DEMO_SLIDE_L:
  case DEMO_SLIDE_R:
    timeUp = millis() - demoStart >= DEMO_SLIDE_MS;
    if (timeUp) { ottoRollFeet(0, 0); demoFinishStep(DEMO_PAUSE_MS); return; }
    if (demoOrder[demoIdx] == DEMO_SLIDE_F) slideFront();
    else if (demoOrder[demoIdx] == DEMO_SLIDE_B) slideBack();
    else if (demoOrder[demoIdx] == DEMO_SLIDE_L) slideLeft();
    else slideTurnRight();
    break;

  case DEMO_ROLL_F:
  case DEMO_ROLL_B:
  case DEMO_ROLL_L:
  case DEMO_ROLL_R:
    timeUp = millis() - demoStart >= DEMO_ROLL_MS;
    if (timeUp) { ottoRollFeet(0, 0); demoFinishStep(DEMO_PAUSE_MS); return; }
    if (demoOrder[demoIdx] == DEMO_ROLL_F) rollingFront();
    else if (demoOrder[demoIdx] == DEMO_ROLL_B) rollingBack();
    else if (demoOrder[demoIdx] == DEMO_ROLL_L) rollingLeft();
    else rollingRight();
    break;

  case DEMO_DANCE:
    if (demoDanceIdx >= 0 && (long)(millis() - demoDanceUntil) < 0)
      return; // 지금 댄스 단계를 유지하는 중
    demoDanceIdx++;
    if (demoDanceIdx >= DEMO_DANCE_COUNT) { // 댄스가 끝났다 → 처음(걷기)으로
      ottoRollFeet(0, 0);
      demoFinishStep(0);
      return;
    }
    {
      const DanceStep &d = demoDanceSteps[demoDanceIdx];
      if (d.kind == 'H' || d.kind == 'A' || d.kind == 'B') danceMoveHip(d.kind, d.a, d.b);
      else if (d.kind == 'L') rollLeftFoot(d.a);
      else rollRightFoot(d.a);
      demoDanceUntil = millis() + d.ms;
    }
    break;

  default:
    break;
  }
}

// 프로토콜 동작 코드(a) → 라이브러리 함수
struct OttoAction {
  const char *code;
  OttoMode mode;
  void (*run)(); // walk/slide: 한 번 부르면 한 동작, roll: 호출 즉시 돌아오며 계속 굴린다
  void (*runV)(int); // 속도(%)를 받는 함수. 없으면 nullptr (걷기·굴리기의 전진/후진만 있다)
};

const OttoAction ottoActions[] = {
    {"wf", OTTO_WALK, walkFront, walkFront},   {"wb", OTTO_WALK, walkBack, walkBack},
    {"wl", OTTO_WALK, walkLeft, nullptr},      {"wr", OTTO_WALK, walkRight, nullptr},
    {"sf", OTTO_SLIDE, slideFront, nullptr},   {"sb", OTTO_SLIDE, slideBack, nullptr},
    {"sl", OTTO_SLIDE, slideLeft, nullptr},    {"sr", OTTO_SLIDE, slideTurnRight, nullptr},
    {"rf", OTTO_ROLL, rollingFront, rollingFront}, {"rb", OTTO_ROLL, rollingBack, rollingBack},
    {"rl", OTTO_ROLL, rollingLeft, nullptr},   {"rr", OTTO_ROLL, rollingRight, nullptr},
    {"dn", OTTO_DANCE, danceRun, nullptr},
    {"dl", OTTO_DANCE, danceRunLeft, nullptr},   // 댄스 시험: 왼발 서기만 (웹 [로봇 맞추기] 5번)
    {"dr", OTTO_DANCE, danceRunRight, nullptr},  // 댄스 시험: 오른발 서기만
    {"pt", OTTO_PATROL, patrolRun, nullptr},  // 순찰: 거리 센서로 장애물을 피하며 굴러다닌다 (patrol.ino)
    {"od", OTTO_DEMO, ottoDemoRun, nullptr}, // OTTO 데모: 걷기→미끄러짐→굴리기→댄스 반복 (+LED 데모)
};

struct Otto {
  const OttoAction *current = nullptr; // 진행 중인 동작 (없으면 정지)
  unsigned long until = 0;             // 이 시각(millis)까지 동작을 유지한다
  bool endless = false;                // true 이면 stop 명령이 올 때까지 계속한다 (du = 0)
  bool hipsSpread = false;             // 굴리기 자세(엉덩이 YR 0 / YL 180)인지
  int velocity = OTTO_SPEED_DEFAULT;   // 속도(%). 걷기·굴리기의 전진/후진에서만 쓴다 (100이 보통)

  void setup();
  void command(const String &code, int durationSec, int speedPercent = OTTO_SPEED_DEFAULT);
  void prepare(OttoMode mode);
  void stop();
  void loop();
  void ack(const String &code, bool ok, const char *err = nullptr);
} otto;

void Otto::setup() {
  // 보정값만 등록한다. 서보는 첫 동작 명령이 올 때 라이브러리가 초기화한다.
  ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, WALK_TILT_R, WALK_TILT_L,
                     RL_SPEED_US, RR_SPEED_US);
  if (ottoLoadCalibration()) { // NVS 에 저장된 보정값이 있으면 그 값을 쓴다
    OttoCalibration c;
    ottoGetCalibration(c);
    YL_PULSE_CENTER = c.ylCenter;
    YR_PULSE_CENTER = c.yrCenter;
    WALK_TILT_R = c.tiltR;
    WALK_TILT_L = c.tiltL;
    RL_SPEED_US = c.rlSpeed;
    RR_SPEED_US = c.rrSpeed;
    Serial.printf("[보정] 저장된 값 사용: 다리 %d/%d, 기울기 %d/%d, 속도 %d/%d\n", c.ylCenter, c.yrCenter, c.tiltR, c.tiltL, c.rlSpeed, c.rrSpeed);
  } else {
    Serial.println("[보정] 저장된 값 없음: 기본값 사용");
  }
}

void Otto::ack(const String &code, bool ok, const char *err) {
  DynamicJsonDocument res(200);
  res["c"] = "ot";
  res["m"] = wifi.mac;
  res["a"] = code;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// 동작 모드에 맞게 보정값과 엉덩이 자세를 준비한다.
void Otto::prepare(OttoMode mode) {
  switch (mode) {
  case OTTO_WALK:
    ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, WALK_TILT_R,
                       WALK_TILT_L, RL_SPEED_US, RR_SPEED_US);
    if (hipsSpread) { // 굴리기 자세에서 걷기로 바꾸면 엉덩이를 먼저 가운데로 돌린다
      moveHip(90, 90);
      delay(400);
      hipsSpread = false;
    }
    break;
  case OTTO_SLIDE:
    ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, SLIDE_TILT_R,
                       SLIDE_TILT_L, RL_SPEED_US, RR_SPEED_US);
    // 굴리기 자세에서 오면 라이브러리 함수가 엉덩이를 부드럽게 가운데로 옮긴다.
    // (slideTurnRight 는 hipsNeedNeutral 을 보고 직접 가운데로 맞춘다)
    hipsNeedNeutral = hipsSpread;
    hipsSpread = false;
    break;
  case OTTO_ROLL:
    if (!hipsSpread) { // 엉덩이를 끝까지 돌린 뒤 굴린다
      moveHip(0, 180);
      delay(1000);
      hipsSpread = true;
    }
    break;
  case OTTO_PATROL:
    if (!hipsSpread) { // 굴리기 자세로 만든 뒤 순찰을 시작한다
      moveHip(0, 180);
      delay(1000);
      hipsSpread = true;
    }
    patrolReset(); // 앞으로 가기부터 시작하고, 거리 센서를 준비한다
    break;
  case OTTO_DEMO:
    ottoDemoReset(); // 처음(걷기)부터 시작한다
    if (hipsSpread) { // 굴리기 자세에서 오면 엉덩이를 먼저 가운데로 돌린다
      moveHip(90, 90);
      delay(400);
      hipsSpread = false;
    }
    hipsNeedNeutral = false;
    lightDemoBegin(); // LED 데모도 같이 시작한다
    break;
  case OTTO_DANCE:
    danceReset(); // 처음 단계부터 시작한다
    if (hipsSpread) { // 굴리기 자세에서 오면 엉덩이를 먼저 가운데로 돌린다
      moveHip(90, 90);
      delay(400);
      hipsSpread = false;
    }
    break;
  }
}

// 모든 동작을 멈춘다. 발을 멈추고, 걷기·미끄러짐은 엉덩이가 이미 가운데이다.
// 굴리기는 엉덩이를 펼친 채로 두어 다시 시작할 때 바로 굴러간다.
void Otto::stop() {
  if (!current)
    return;
  OttoMode mode = current->mode;
  current = nullptr;
  endless = false;
  if (mode != OTTO_WALK) // 걷기는 한 걸음이 끝날 때마다 발을 스스로 멈춘다
    ottoRollFeet(0, 0);
  if (mode == OTTO_DANCE || mode == OTTO_DEMO) // 댄스·데모는 엉덩이가 기울거나 펼쳐진 채 멈출 수 있어 가운데로 세운다
    moveHip(90, 90);
  if (mode == OTTO_DEMO) { // 데모를 멈추면 LED 데모도 끈다
    hipsSpread = false;
    lightDemoEnd();
  }
  Serial.println("[Otto] 정지");
}

// 동작이 시작될 때 라이브러리에 실제로 들어 있는 보정값과 보관 중인 값(NVS에서 불러온 값, 웹에서 바꾼 값)을 비교해서 남긴다.
// 다리 중심값과 발 속도가 다르면 "<- 다름" 이 붙는다. 기울기는 걷기·굴리기에서만 비교한다. (미끄러짐, 데모는 기울기를 따로 쓴다)
void ottoLogCal(int modeValue) {
  OttoMode mode = (OttoMode)modeValue;
  OttoCalibration c;
  ottoGetCalibration(c);
  bool diff = c.ylCenter != YL_PULSE_CENTER || c.yrCenter != YR_PULSE_CENTER || c.rlSpeed != RL_SPEED_US || c.rrSpeed != RR_SPEED_US;
  if (mode == OTTO_WALK && (c.tiltR != WALK_TILT_R || c.tiltL != WALK_TILT_L))
    diff = true;
  Serial.printf("[보정 확인] 라이브러리 다리 %d/%d 기울기 %d/%d 속도 %d/%d | 보관값 다리 %d/%d 걷기기울기 %d/%d 속도 %d/%d | 저장됨 %s%s\n",
                c.ylCenter, c.yrCenter, c.tiltR, c.tiltL, c.rlSpeed, c.rrSpeed,
                YL_PULSE_CENTER, YR_PULSE_CENTER, WALK_TILT_R, WALK_TILT_L, RL_SPEED_US, RR_SPEED_US,
                ottoHasSavedCalibration() ? "예" : "아니오", diff ? " <- 다름" : "");
}

void Otto::command(const String &code, int durationSec, int speedPercent) {
  if (code == "stop") {
    stop();
    ack(code, true);
    return;
  }

  const OttoAction *found = nullptr;
  for (const OttoAction &a : ottoActions) {
    if (code == a.code) {
      found = &a;
      break;
    }
  }
  if (!found) {
    ack(code, false, "bad_action");
    return;
  }

  // du = 0 이면 stop 명령이 올 때까지 계속한다. 그 외에는 1~OTTO_MAX_SEC 초만 유지한다.
  bool wantEndless = (durationSec <= 0);
  if (!wantEndless)
    durationSec = constrain(durationSec, 1, OTTO_MAX_SEC);

  velocity = constrain(speedPercent, OTTO_SPEED_MIN, OTTO_SPEED_MAX); // 속도(%): 같은 동작이 계속되는 중에도 바뀐다

  if (current == found) { // 같은 동작이 다시 오면 시간과 속도만 갱신한다 (응답 생략)
    endless = wantEndless;
    until = wantEndless ? 0 : millis() + durationSec * 1000UL;
    return;
  }

  stop(); // 다른 동작이면 이전 동작을 멈추고 바꾼다
  prepare(found->mode);
  current = found;
  endless = wantEndless;
  until = wantEndless ? 0 : millis() + durationSec * 1000UL;
  if (wantEndless)
    Serial.printf("[Otto] 시작: %s (stop 까지 계속, 속도 %d%%)\n", found->code, velocity);
  else
    Serial.printf("[Otto] 시작: %s (%d초, 속도 %d%%)\n", found->code, durationSec, velocity);
  ottoLogCal((int)found->mode); // 동작이 시작될 때 지금 적용된 보정값을 한 줄 남긴다 (엉뚱하게 움직일 때 원인을 찾는 용도)
  ack(code, true);
}

void Otto::loop() {
  if (!current)
    return;
  // (계속 모드가 아닐 때) 시간이 지나면 멈춘다. 웹 화면이 말없이 사라진 경우는 i2r-otto.ino 의 webWatch() 가 멈춘다.
  if (!endless && (long)(millis() - until) >= 0) {
    stop();
    return;
  }
  if (current->runV)
    current->runV(velocity); // 걷기·굴리기 전진/후진: 속도(%)를 적용한다
  else
    current->run();
}

// ── 보정 (웹의 [굴림보정]/[걷기보정]) ─────────────────────────────
// 라이브러리에 지금 모드에 맞는 기울기로 보정값을 넣는다. (미끄러짐 중에는 미끄러짐 기울기를 유지한다)
void ottoApplyCal() {
  bool slide = otto.current && otto.current->mode == OTTO_SLIDE;
  ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, slide ? SLIDE_TILT_R : WALK_TILT_R,
                     slide ? SLIDE_TILT_L : WALK_TILT_L, RL_SPEED_US, RR_SPEED_US);
}

// 보정 응답: 현재 값 6개와 저장 여부(csv). 키 이름: cyl cyr ctr ctl crl crr csv
void ottoCalReply(const String &action, bool ok, const char *err = nullptr) {
  DynamicJsonDocument res(400);
  res["c"] = "oc";
  res["m"] = wifi.mac;
  res["a"] = action;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  res["cyl"] = YL_PULSE_CENTER;
  res["cyr"] = YR_PULSE_CENTER;
  res["ctr"] = WALK_TILT_R;
  res["ctl"] = WALK_TILT_L;
  res["crl"] = RL_SPEED_US;
  res["crr"] = RR_SPEED_US;
  res["csv"] = ottoHasSavedCalibration() ? 1 : 0;
  int dRYr, dRYl, dLYr, dLYl;
  getDanceTilts(dRYr, dRYl, dLYr, dLYl);
  res["d1"] = dRYr; // 댄스: 왼발 서기 YR / YL, 오른발 서기 YR / YL
  res["d2"] = dRYl;
  res["d3"] = dLYr;
  res["d4"] = dLYl;
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// oc: {"c":"oc","a":"get|set|save|clear|restore|p0|p90", "cyl":..,"cyr":..,"ctr":..,"ctl":..,"crl":..,"crr":.., "d1".."d4":..}
//   d1~d4 는 댄스 각도: 왼발 서기 YR/YL, 오른발 서기 YR/YL (set 은 바로 적용만, save 때 NVS 에 저장)
//   get   현재 값 조회 / set   보낸 값만 바꾼다(저장 안 함, 바로 적용) / save  NVS 에 저장 / clear  저장값 삭제하고 기본값으로
//   p0    조립 자세(엉덩이 YR 0 / YL 180) / p90   다리를 가운데(90도)로
void ottoCalCommand(JsonDocument &doc) {
  String a = doc["a"] | "get";

  if (a == "get") {
    ottoCalReply(a, true);
    return;
  }
  // restore: 웹이 이 컴퓨터에 저장해 둔 값을 보드에 되돌려 넣는다. 6개 값을 모두 받아 적용하고 저장한다. (다리는 움직이지 않는다)
  // 펌웨어를 다시 설치해서 NVS 가 지워졌을 때 쓴다.
  if (a == "restore") {
    if (doc["cyl"].isNull() || doc["cyr"].isNull() || doc["ctr"].isNull() || doc["ctl"].isNull() ||
        doc["crl"].isNull() || doc["crr"].isNull()) {
      ottoCalReply(a, false, "bad_param");
      return;
    }
    YL_PULSE_CENTER = constrain(doc["cyl"].as<int>(), 1000, 1900);
    YR_PULSE_CENTER = constrain(doc["cyr"].as<int>(), 1000, 1900);
    WALK_TILT_R = constrain(doc["ctr"].as<int>(), 0, 45);
    WALK_TILT_L = constrain(doc["ctl"].as<int>(), 0, 45);
    RL_SPEED_US = constrain(doc["crl"].as<int>(), 0, 600);
    RR_SPEED_US = constrain(doc["crr"].as<int>(), 0, 600);
    ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, WALK_TILT_R, WALK_TILT_L, RL_SPEED_US, RR_SPEED_US);
    bool ok = ottoSaveCalibration();
    ottoApplyCal();
    ottoCalReply(a, ok, ok ? nullptr : "nvs_fail");
    return;
  }
  if (a == "p0" || a == "p90" || a == "set") {
    if (chatBusy()) { // AI 대화 중에는 다리를 움직이지 않는다
      ottoCalReply(a, false, "busy");
      return;
    }
  }

  if (a == "p0") {
    otto.stop();
    moveHip(0, 180); // 굴리기 자세: 다리를 끼우고 나사를 조이는 자세
    otto.hipsSpread = true;
    ottoCalReply(a, true);
    return;
  }
  if (a == "p90") {
    otto.stop();
    moveHip(90, 90);
    otto.hipsSpread = false;
    ottoCalReply(a, true);
    return;
  }
  if (a == "set") {
    bool hipChanged = !doc["cyl"].isNull() || !doc["cyr"].isNull();
    // 보낸 값만 바꾼다. 범위는 라이브러리와 같게 제한한다. (잘못된 값으로 서보와 기구가 상하지 않게)
    if (!doc["cyl"].isNull()) YL_PULSE_CENTER = constrain(doc["cyl"].as<int>(), 1000, 1900);
    if (!doc["cyr"].isNull()) YR_PULSE_CENTER = constrain(doc["cyr"].as<int>(), 1000, 1900);
    if (!doc["ctr"].isNull()) WALK_TILT_R = constrain(doc["ctr"].as<int>(), 0, 45);
    if (!doc["ctl"].isNull()) WALK_TILT_L = constrain(doc["ctl"].as<int>(), 0, 45);
    if (!doc["crl"].isNull()) RL_SPEED_US = constrain(doc["crl"].as<int>(), 0, 600);
    if (!doc["crr"].isNull()) RR_SPEED_US = constrain(doc["crr"].as<int>(), 0, 600);
    if (!doc["d1"].isNull() || !doc["d2"].isNull() || !doc["d3"].isNull() || !doc["d4"].isNull()) {
      int dRYr, dRYl, dLYr, dLYl;
      getDanceTilts(dRYr, dRYl, dLYr, dLYl);
      applyDanceTilts(doc["d1"] | dRYr, doc["d2"] | dRYl, doc["d3"] | dLYr, doc["d4"] | dLYl); // 안전 범위는 라이브러리가 맞춘다
    }
    ottoApplyCal(); // 걷기·굴리기는 걸음마다 이 값을 읽어서 바로 반영된다
    if (hipChanged && !otto.current) { // 쉬는 중이면 다리를 가운데로 보내서 바뀐 것을 눈으로 확인하게 한다
      moveHip(90, 90);
      otto.hipsSpread = false;
    }
    ottoCalReply(a, true);
    return;
  }
  if (a == "save") {
    // 라이브러리에는 걷기 기울기 기준의 값을 넣고 저장한다 (미끄러짐 기울기가 저장되지 않게)
    ottoSetCalibration(YL_PULSE_CENTER, YR_PULSE_CENTER, WALK_TILT_R, WALK_TILT_L, RL_SPEED_US, RR_SPEED_US);
    bool ok = ottoSaveCalibration();
    ok = saveDanceTilts() && ok; // 댄스 각도도 함께 저장한다
    ottoApplyCal();
    ottoCalReply(a, ok, ok ? nullptr : "nvs_fail");
    return;
  }
  if (a == "clear") {
    ottoClearCalibration();
    clearDanceTilts();
    YL_PULSE_CENTER = DEF_YL_PULSE_CENTER;
    YR_PULSE_CENTER = DEF_YR_PULSE_CENTER;
    RL_SPEED_US = DEF_RL_SPEED_US;
    RR_SPEED_US = DEF_RR_SPEED_US;
    WALK_TILT_R = DEF_WALK_TILT_R;
    WALK_TILT_L = DEF_WALK_TILT_L;
    ottoApplyCal();
    ottoCalReply(a, true);
    return;
  }
  ottoCalReply(a, false, "bad_action");
}
