// ============================================================
// speak.ino - Otto 말하기: 웹에서 보낸 글자를 스피커로 반복해서 읽는다.
//
// AI 대화(chat.ino)의 TTS 와 같은 방법(Google 번역 TTS 주소를 Audio 로 재생)으로 읽는다.
// 스피커(MAX98357A)와 Audio 객체는 AI 대화, 라디오, 내 음악과 같이 쓴다. 말하기를 켜면 그것들은 꺼진다.
//
//   {"c":"sk","m":"..","a":"start","sx":"사랑아 생일 축하해","sw":5,"sq":0,"vl":18}
//        sx 읽을 글자, sw 다 읽은 뒤 쉬는 시간(초), sq 반복 횟수(0 = 멈출 때까지), vl 볼륨(0~21)
//   {"c":"sk","m":"..","a":"stop"}          멈춤
//   {"c":"sk","m":"..","a":"vol","vl":12}   볼륨
//   {"c":"sk","m":"..","a":"get"}           지금 상태 알려 달라는 요청
//
// 응답: {"c":"sk","m":"..","a":"<요청>","ok":1,"pl":1,"sd":3,"sw":5,"sq":0,"vl":18}
//        pl 말하는 중(1/0), sd 지금까지 읽은 횟수. 실패하면 "ok":0,"err":"...".
//        정해진 횟수를 다 읽으면 {"a":"done","pl":0} 을 보낸다.
//
// MQTT 메시지는 512바이트 이하여야 해서 글자는 한글 80자까지만 받는다.
// Arduino IDE 에서 Tools → PSRAM 을 "OPI PSRAM" 으로 선택해야 한다.
// ============================================================

const int SPEAK_MAX_CHARS = 80;                   // 읽을 글자 수 (한글 1자 = 1)
const int SPEAK_INTERVAL_MAX = 3600;              // 쉬는 시간 최대 (초)
const int SPEAK_COUNT_MAX = 1000;                 // 반복 횟수 최대
const unsigned long SPEAK_START_TIMEOUT_MS = 12000; // 이 시간 안에 소리가 안 나오면 실패로 본다
const unsigned long SPEAK_END_IDLE_MS = 2000;     // 재생 시간이 이만큼 안 늘면 다 읽은 것으로 본다
const int SPEAK_MAX_FAIL = 3;                    // 연달아 이만큼 실패하면 멈춘다

enum SpeakState { SPEAK_IDLE, SPEAK_TALKING, SPEAK_WAITING };

SpeakState speakState = SPEAK_IDLE;
String speakText;                  // 읽을 글자
int speakInterval = 5;             // 쉬는 시간 (초)
int speakCount = 0;                // 반복 횟수 (0 = 멈출 때까지)
int speakVolume = 18;              // 볼륨 (0 ~ 21)
int speakDone = 0;                 // 지금까지 읽은 횟수
int speakFail = 0;                 // 연달아 실패한 횟수
bool speakHeard = false;           // 이번에 소리가 나오기 시작했는지
unsigned long speakStartedAt = 0;  // 이번 읽기를 시작한 시각
unsigned long speakNextAt = 0;     // 다음 읽기를 시작할 시각
uint32_t speakLastTime = 0;        // 마지막으로 본 재생 시간 (초)
unsigned long speakLastChange = 0; // 재생 시간이 마지막으로 바뀐 시각

bool speakActive() { return speakState != SPEAK_IDLE; }

void speakAck(const char *action, bool ok, const char *err = nullptr) {
  DynamicJsonDocument res(256);
  res["c"] = "sk";
  res["m"] = wifi.mac;
  res["a"] = action;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  res["pl"] = speakActive() ? 1 : 0;
  res["sd"] = speakDone;
  res["sw"] = speakInterval;
  res["sq"] = speakCount;
  res["vl"] = speakVolume;
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// 글자 하나를 읽기 시작한다. (끝날 때까지 기다리지 않는다)
void speakBegin() {
  String url = "http://translate.google.com/translate_tts"
               "?ie=UTF-8&client=tw-ob&tl=ko&q=" +
               chatUrlEncode(speakText);
  Serial.printf("[말하기] %d번째: \"%s\"\n", speakDone + 1, speakText.c_str());
  chatAudio->stopSong();
  speakHeard = false;
  speakStartedAt = millis();
  speakLastTime = 0;
  speakLastChange = speakStartedAt;
  speakState = SPEAK_TALKING;
  if (!chatAudio->connecttohost(url.c_str()))
    speakStartedAt = millis() - SPEAK_START_TIMEOUT_MS; // 연결 실패: 바로 실패로 처리한다
}

// 말하기를 끈다. notify 이면 웹에도 알린다. (다른 소리를 켜서 말하기가 꺼질 때 쓴다)
void speakStop(bool notify) {
  if (!speakActive())
    return;
  speakState = SPEAK_IDLE;
  if (chatAudio)
    chatAudio->stopSong();
  if (!chatOn)
    digitalWrite(SPK_SD, LOW); // 앰프 끄기
  Serial.println("[말하기] 정지");
  if (notify)
    speakAck("stop", true);
}

bool speakStart(const String &text, const char *&err) {
  String t = utf8TruncateChars(text, SPEAK_MAX_CHARS);
  t.trim();
  if (t.isEmpty()) {
    err = "no_text";
    return false;
  }
  chatForceStop();        // 대화 중이면 먼저 끈다
  radioStopPlaying(true); // 라디오가 나오고 있으면 끈다 (스피커를 같이 쓴다)
  musicStop(true);        // 내 음악이 나오고 있으면 끈다
  if (!audioHwInit(err))
    return false;

  speakText = t;
  speakDone = 0;
  speakFail = 0;
  digitalWrite(SPK_SD, HIGH); // 앰프 켜기
  chatAudio->setVolume(speakVolume);
  speakBegin();
  return true;
}

// sk 명령 처리
void speakCommand(JsonDocument &doc) {
  String action = doc["a"] | "";

  if (action == "start") {
    // 쉬는 시간, 횟수, 볼륨은 범위 안으로 맞춘다. 안 보내면 지난번 값을 쓴다.
    speakInterval = constrain((int)(doc["sw"] | speakInterval), 0, SPEAK_INTERVAL_MAX);
    speakCount = constrain((int)(doc["sq"] | speakCount), 0, SPEAK_COUNT_MAX);
    speakVolume = constrain((int)(doc["vl"] | speakVolume), 0, RADIO_VOLUME_MAX);
    const char *err = nullptr;
    String text = doc["sx"] | "";
    if (speakStart(text, err)) {
      speakAck("start", true);
    } else {
      Serial.printf("[말하기] 시작 실패: %s\n", err);
      speakState = SPEAK_IDLE;
      speakAck("start", false, err);
    }
  } else if (action == "stop") {
    speakStop(false);
    speakAck("stop", true);
  } else if (action == "vol") {
    speakVolume = constrain((int)(doc["vl"] | speakVolume), 0, RADIO_VOLUME_MAX);
    if (chatAudio && speakActive() && !chatOn)
      chatAudio->setVolume(speakVolume);
    Serial.printf("[말하기] 볼륨 %d / %d\n", speakVolume, RADIO_VOLUME_MAX);
    speakAck("vol", true);
  } else if (action == "get") {
    speakAck("get", true);
  } else {
    speakAck(action.c_str(), false, "bad_action");
  }
}

// loop() 에서 계속 불린다. 소리를 디코딩하고, 다 읽으면 쉬었다가 다시 읽는다.
void speakLoop() {
  if (!speakActive() || chatOn)
    return;

  unsigned long now = millis();

  if (speakState == SPEAK_WAITING) {
    if ((long)(now - speakNextAt) >= 0)
      speakBegin();
    return;
  }

  chatAudio->loop(); // 스트리밍 데이터를 계속 받아서 재생한다 (필수)

  if (chatAudio->isRunning()) {
    // Google TTS 스트림은 다 읽어도 isRunning() 이 꺼지지 않을 때가 있다 (chat.ino 와 같은 이유).
    // 재생 시간이 더 늘지 않으면 끝난 것으로 보고 직접 멈춘다.
    uint32_t t = chatAudio->getAudioCurrentTime();
    if (t != speakLastTime) {
      speakLastTime = t;
      speakLastChange = now;
    }
    if (t > 0)
      speakHeard = true;
    unsigned long idleLimit = speakHeard ? SPEAK_END_IDLE_MS : SPEAK_START_TIMEOUT_MS;
    if (now - speakLastChange <= idleLimit)
      return;
    chatAudio->stopSong(); // 끝났거나 멈춰 버렸다: 아래에서 판단한다
  }

  if (speakHeard) { // 소리가 나왔다가 끝났다: 한 번 읽기를 마쳤다
    speakDone++;
    speakFail = 0;
    if (speakCount > 0 && speakDone >= speakCount) { // 정해진 횟수를 다 읽었다
      speakState = SPEAK_IDLE;
      digitalWrite(SPK_SD, LOW);
      Serial.printf("[말하기] %d번 읽고 끝냅니다\n", speakDone);
      speakAck("done", true);
      return;
    }
    speakState = SPEAK_WAITING;
    speakNextAt = now + (unsigned long)speakInterval * 1000UL;
    speakAck("tick", true); // 몇 번째까지 읽었는지 웹에 알린다
    return;
  }

  if (now - speakStartedAt < SPEAK_START_TIMEOUT_MS)
    return; // 아직 연결하는 중이다

  // 소리가 안 나온다: 몇 번 다시 해 보고, 계속 안 되면 멈춘다.
  speakFail++;
  Serial.printf("[말하기] 소리가 나지 않습니다 (%d/%d)\n", speakFail, SPEAK_MAX_FAIL);
  if (speakFail >= SPEAK_MAX_FAIL) {
    speakState = SPEAK_IDLE;
    chatAudio->stopSong();
    digitalWrite(SPK_SD, LOW);
    speakAck("start", false, "tts_fail");
  } else {
    speakBegin();
  }
}
