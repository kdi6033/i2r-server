// ============================================================
// patrol.ino - Otto 순찰 (거리 센서 VL53L0X 로 장애물을 피하며 굴러다닌다)
//
// 웹의 [순찰] 버튼(동작 코드 "pt")을 누르면 시작하고, 정지 명령이 올 때까지 계속한다.
// patrol/patrol.ino 를 otto.ino 의 동작 방식(Otto::loop 가 계속 부르는 단계 방식)으로 옮긴 것이다.
//
// 동작: 거리 300mm 보다 멀다 -> 보통 속도로 앞으로 굴러간다 (rollingFront)
//       거리 300mm 이내     -> 속도를 줄인다
//       거리 100mm 이내     -> 제자리에서 돈다 (rollingRight/rollingLeft). 앞이 트이면(300mm 이상) 다시 앞으로 간다.
//       걸림(센서보다 낮은 장벽에 다리가 걸림) -> 잠깐 후진한 뒤 방향을 바꿔 돈다
//         - 앞으로 가는데 거리가 2초 동안 거의 안 변하면 걸린 것으로 본다
//         - 3초 돌아도 앞이 안 트이면 걸린 것으로 본다
//         - 거리가 범위 밖(무한대)으로 5초 동안 계속 나와도 같은 방법으로 방향을 바꾼다
// 눈: eyes.ino 의 눈이 동작 중에 알아서 움직인다. (patrol.ino 가 따로 그리지 않는다)
//
// 센서: VL53L0X (SDA = GPIO18, SCL = GPIO17). OLED 와 같은 I2C 선을 쓰므로, 눈 작업이 화면을 그리는 동안에는
//       eyesMutex 로 기다렸다가 읽는다. (같이 쓰면 I2C 가 깨진다)
// 센서가 없으면 순찰을 시작하지 않고 {"c":"ot","a":"pt","ok":0,"err":"no_sensor"} 를 보낸다.
// 튜닝: SLOW_MM, TURN_MM, PATROL_NORMAL_SPEED, PATROL_SLOW_SPEED, PATROL_TURN_MIN_MS ...
// ============================================================

#include <Adafruit_VL53L0X.h>

const int PATROL_SLOW_MM = 300;             // 이 거리 이내면 속도를 줄인다
const int PATROL_TURN_MM = 100;             // 이 거리 이내면 방향을 바꾼다
const int PATROL_NORMAL_SPEED = 50;         // 보통 속도(%) (20 ~ 200, 100 이 보정한 기본 속도)
const int PATROL_SLOW_SPEED = 25;           // 느린 속도(%)
const int PATROL_BACK_SPEED = 50;           // 후진 속도(%)

const unsigned long PATROL_TURN_MIN_MS = 800;   // 돌기 시작하면 앞이 트여 보여도 최소 이 시간은 돈다 (낮은 장벽은 센서에 안 보임)
const unsigned long PATROL_TURN_TIMEOUT_MS = 3000; // 이 시간 동안 돌아도 앞이 안 트이면 걸린 것으로 본다
const unsigned long PATROL_STUCK_MS = 2000;     // 앞으로 가는데 거리가 이 시간 동안 안 변하면 걸린 것으로 본다
const int PATROL_STUCK_TOLERANCE_MM = 10;       // 이 값 이하로만 변하면 "안 변했다"고 본다
const unsigned long PATROL_FAR_MS = 5000;       // 거리가 범위 밖(무한대)으로 이 시간 동안 계속되면 방향을 바꾼다
const unsigned long PATROL_BACK_MS = 1000;      // 걸렸을 때 후진하는 시간
const unsigned long PATROL_READ_MS = 50;        // 거리를 읽는 간격 (센서 한 번 측정에 약 30ms)

Adafruit_VL53L0X patrolLox = Adafruit_VL53L0X();
bool patrolSensorReady = false; // 센서를 찾아서 초기화했는지

enum PatrolState { PATROL_GO, PATROL_BACK, PATROL_TURN };
PatrolState patrolState = PATROL_GO;
unsigned long patrolStateStart = 0;
unsigned long patrolLastRead = 0;
int patrolLastMm = 9999;          // 마지막으로 읽은 거리

bool patrolTurnRight = true;      // 도는 방향 (true: 오른쪽)
int patrolLastSpeed = 0;          // 마지막으로 보낸 앞으로 굴리기 속도 (같은 속도는 다시 보내지 않는다)
int patrolStuckRefMm = -1;        // 걸림 확인용: 기준 거리 (-1 = 없음)
unsigned long patrolStuckRefTime = 0;
unsigned long patrolFarStart = 0; // 거리가 범위 밖으로 나오기 시작한 시간 (0 = 아님)

// 순찰을 처음 시작할 때(또는 센서를 아직 못 찾았을 때) 센서를 초기화한다.
static bool patrolInitSensor() {
  if (patrolSensorReady)
    return true;
  // 눈 작업과 같은 I2C 선을 쓰므로 눈 작업이 그리는 동안에는 기다린다.
  if (!xSemaphoreTake(eyesMutex, pdMS_TO_TICKS(500)))
    return false;
  patrolSensorReady = patrolLox.begin();
  xSemaphoreGive(eyesMutex);
  if (!patrolSensorReady)
    Serial.println("[순찰] VL53L0X 센서를 찾을 수 없습니다. 배선(SDA/SCL/전원)을 확인하세요.");
  return patrolSensorReady;
}

// 앞쪽 거리(mm)를 읽는다. 측정 범위 밖이면 아주 먼 값(9999)을 돌려준다.
// 눈 작업이 화면을 그리는 중이면 이번에는 읽지 않고 마지막 값을 쓴다.
static int patrolReadDistance() {
  if (!xSemaphoreTake(eyesMutex, pdMS_TO_TICKS(100)))
    return patrolLastMm;
  VL53L0X_RangingMeasurementData_t measure;
  patrolLox.rangingTest(&measure, false);
  xSemaphoreGive(eyesMutex);
  if (measure.RangeStatus == 4) // 4 = 범위 밖 (앞이 트여 있다)
    return 9999;
  return measure.RangeMilliMeter;
}

// 순찰을 처음(앞으로 가기)부터 시작한다. Otto::prepare 가 부른다.
void patrolReset() {
  patrolState = PATROL_GO;
  patrolStateStart = millis();
  patrolLastRead = 0;
  patrolLastMm = 9999;
  patrolLastSpeed = 0;
  patrolStuckRefMm = -1;
  patrolFarStart = 0;
  patrolInitSensor(); // 못 찾으면 patrolRun() 이 알려 주고 멈춘다
}

// 걸림: 잠깐 후진한다. 후진이 끝나면 방향을 바꿔 돈다.
static void patrolStartBack() {
  Serial.println("[순찰] 걸림 감지: 후진");
  patrolState = PATROL_BACK;
  patrolStateStart = millis();
  patrolLastSpeed = 0;
}

// 방향을 바꿔(왼쪽/오른쪽 번갈아) 돌기 시작한다.
static void patrolStartTurn() {
  patrolTurnRight = !patrolTurnRight;
  patrolState = PATROL_TURN;
  patrolStateStart = millis();
}

// Otto::loop() 가 계속 부른다. 한 번 불릴 때 짧게 처리하고 돌아온다.
void patrolRun() {
  if (!patrolSensorReady && !patrolInitSensor()) {
    otto.stop();
    otto.ack("pt", false, "no_sensor");
    return;
  }

  unsigned long now = millis();

  // 1. 후진 중: 정해진 시간 동안 뒤로 굴러간 뒤 방향을 바꿔 돈다
  if (patrolState == PATROL_BACK) {
    rollingBack(PATROL_BACK_SPEED);
    if (now - patrolStateStart > PATROL_BACK_MS)
      patrolStartTurn();
    return;
  }

  // 거리는 PATROL_READ_MS 마다 한 번만 읽는다. (그 사이에는 하던 동작을 계속한다)
  if (patrolLastRead != 0 && now - patrolLastRead < PATROL_READ_MS)
    return;
  patrolLastRead = now;
  int mm = patrolReadDistance();
  patrolLastMm = mm;

  // 2. 돌기: 앞이 트일 때까지 돈다. 오래 돌아도 막혀 있으면 다리가 걸린 것이므로 후진한다.
  if (patrolState == PATROL_TURN) {
    if (mm > PATROL_SLOW_MM && now - patrolStateStart > PATROL_TURN_MIN_MS) { // 최소 시간은 돌았고 앞이 트였다 -> 다시 앞으로
      patrolState = PATROL_GO;
      patrolLastSpeed = 0;
      patrolStuckRefMm = -1;
      patrolFarStart = 0;
    } else if (now - patrolStateStart > PATROL_TURN_TIMEOUT_MS) { // 오래 돌아도 막혀 있다 -> 걸림
      patrolStartBack();
      return;
    } else {
      if (patrolTurnRight)
        rollingRight();
      else
        rollingLeft();
      return;
    }
  }

  // 3. 앞으로 가는 중: 아주 가까우면 돌기 시작한다
  if (mm <= PATROL_TURN_MM) {
    patrolStartTurn();
    return;
  }

  // 4. 걸림 확인: 거리가 읽히는데 PATROL_STUCK_MS 동안 거의 안 변하면 낮은 장벽에 걸린 것이다.
  //    (범위 밖(9999)이면 앞이 트인 것으로 보고 확인하지 않는다)
  if (mm < 9999) {
    if (patrolStuckRefMm < 0 || abs(mm - patrolStuckRefMm) > PATROL_STUCK_TOLERANCE_MM) {
      patrolStuckRefMm = mm; // 거리가 변했다 -> 기준을 새로 잡는다
      patrolStuckRefTime = now;
    } else if (now - patrolStuckRefTime > PATROL_STUCK_MS) {
      patrolStuckRefMm = -1;
      patrolStartBack();
      return;
    }
    patrolFarStart = 0;
  } else {
    patrolStuckRefMm = -1;
    // 범위 밖(무한대)이 PATROL_FAR_MS 동안 계속되면 낮은 장벽에 걸려 헛도는 것일 수 있으므로 후진 후 방향을 바꾼다
    if (patrolFarStart == 0) {
      patrolFarStart = now;
    } else if (now - patrolFarStart > PATROL_FAR_MS) {
      patrolFarStart = 0;
      patrolStartBack();
      return;
    }
  }

  // 5. 가까우면 느리게, 멀면 보통 속도로 앞으로 굴러간다
  int speed = (mm <= PATROL_SLOW_MM) ? PATROL_SLOW_SPEED : PATROL_NORMAL_SPEED;
  if (speed != patrolLastSpeed) {
    rollingFront(speed);
    patrolLastSpeed = speed;
  }
}
