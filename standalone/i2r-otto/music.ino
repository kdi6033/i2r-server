// ============================================================
// music.ino - Otto 내 음악 (USB 로 보드에 저장한 mp3 를 재생한다)
//
// 웹의 [내 음악] 패널이 하는 일:
//   - 저장(put): 사용자의 PC 에서 USB(COM 포트, Web Serial)로 mp3 를 보드의 SPIFFS 에 직접 쓴다.
//                서버를 거치지 않는다. (MQTT 로는 받지 않는다 = 아래 musicPut 참고)
//   - 재생/정지/볼륨/삭제/목록: MQTT 또는 USB 로 보낸다.
//
//   {"c":"mu","a":"put","sz":1234567,"tx":"곡 제목"}   저장 시작 (USB 만). 곡 번호 sn 은 비어 있는 번호를 보드가 정한다.
//        -> {"c":"mu","a":"put","ok":1,"sn":2,"rdy":1}  이 응답을 받으면 웹이 mp3 를 1024바이트씩 보낸다.
//           보드는 1024바이트를 받을 때마다 0x06(ACK) 한 바이트를 돌려준다.
//        -> {"c":"mu","a":"put","ok":1,"sn":2,"dn":1}   다 받았다.  실패하면 "ok":0,"err":"..."
//   {"c":"mu","m":"..","a":"play","sn":2}   2번 곡 재생. 끝나면 다음 곡으로 넘어가고 마지막 곡 다음은 처음으로 돈다.
//   {"c":"mu","m":"..","a":"stop"}          정지
//   {"c":"mu","m":"..","a":"vol","vl":12}   볼륨 (0 ~ 21)
//   {"c":"mu","m":"..","a":"del","sn":2}    삭제
//   {"c":"mu","m":"..","a":"list"}          곡 목록. 응답의 tx 에 곡을 탭 문자로 구분해 "번호|KB|제목" 으로 담는다. (제목은 20바이트까지만. 웹은 이 브라우저에서 저장한 전체 제목을 우선 쓴다)
//   {"c":"mu","m":"..","a":"get"}           지금 상태 요청
//
// 응답: {"c":"mu","m":"..","a":"<요청>","ok":1,"pl":1,"sn":2,"vl":12,"fr":남은 KB}
//        pl 재생 중(1/0), sn 곡 번호(1부터), vl 볼륨, fr 더 저장할 수 있는 KB. 실패하면 "ok":0,"err":"...".
//
// 곡 파일은 /m1.mp3 ~ /m6.mp3, 제목은 /m1.txt ~ /m6.txt. (한글 제목이 파일 이름 길이에 걸리지 않게 번호 이름을 쓴다)
// 스피커(MAX98357A)와 Audio 객체는 AI 대화(chat.ino)·라디오(radio.ino)와 같이 쓴다. 셋 중 하나를 켜면 나머지는 꺼진다.
// ============================================================

const int MUSIC_MAX = 6;                       // 저장할 수 있는 곡 수 (번호 1 ~ 6). 목록 응답이 MQTT 한 메시지(300바이트 이하)에 들어가야 한다
const int MUSIC_VOLUME_MAX = 21;
const size_t MUSIC_CHUNK = 1024;               // USB 로 한 번에 받는 크기. 웹의 MUSIC_CHUNK 와 같아야 한다
const unsigned long MUSIC_RX_TIMEOUT_MS = 5000; // 이 시간 동안 데이터가 안 오면 저장을 포기한다
const unsigned long MUSIC_START_GRACE_MS = 1500; // 재생을 시작하고 이 시간 동안은 "끝났는지" 보지 않는다
const uint8_t MUSIC_ACK = 0x06;

bool musicOn = false;       // 재생 중인지
int musicCur = 0;           // 지금(또는 마지막) 곡 번호 (1부터, 0 = 없음)
int musicVolume = 12;       // 볼륨 (0 ~ 21)
unsigned long musicStartedAt = 0;
bool musicReceiving = false; // USB 로 저장하는 중인지
uint32_t musicMaxSec = 0;    // 이 곡에서 소리가 나온 가장 긴 시간(초). 0 이면 곡을 읽지 못한 것이다
int musicFailStreak = 0;     // 연달아 읽지 못한 곡 수 (곡 수만큼 이어지면 멈춘다)

// 파일 데이터를 버퍼에 채우는 Audio::loop() 는 별도 태스크가 부른다.
// 걷기처럼 loop() 를 1초씩 막는 동작이 있어도 소리가 끊기지 않게 하려는 것이다.
// 태스크가 라이브러리를 쓰는 동안 곡을 시작/정지하지 못하도록 뮤텍스로 감싼다.
SemaphoreHandle_t musicMutex = nullptr;
TaskHandle_t musicTaskHandle = nullptr;

void musicFeedTask(void *) {
  for (;;) {
    if (musicOn && !chatOn && !musicReceiving && chatAudio && xSemaphoreTake(musicMutex, 0) == pdTRUE) {
      chatAudio->loop();
      xSemaphoreGive(musicMutex);
    }
    vTaskDelay(1);
  }
}

void musicEnsureTask() {
  if (musicTaskHandle)
    return;
  musicMutex = xSemaphoreCreateMutex();
  // 메인 loop 와 다른 코어(0)에서 돌린다. 걷기의 delay() 와 상관없이 소리가 이어지고,
  // 고음질 곡(192~320kbps, 큰 앨범 이미지)을 디코딩해도 loop() 의 MQTT 처리가 막히지 않는다.
  // (같은 코어 1 에서 더 높은 우선순위로 돌렸을 때는 무거운 곡에서 MQTT 가 끊겼다)
  // Wi-Fi 태스크는 코어 0 에서 훨씬 높은 우선순위로 돌므로 이 태스크에 밀리지 않는다.
  xTaskCreatePinnedToCore(musicFeedTask, "musicFeed", 8192, nullptr, 2, &musicTaskHandle, 0);
}

// 라이브러리 상태를 바꾸는 동안(곡 시작/정지) 태스크가 끼어들지 못하게 한다.
struct MusicLock {
  MusicLock() { if (musicMutex) xSemaphoreTake(musicMutex, portMAX_DELAY); }
  ~MusicLock() { if (musicMutex) xSemaphoreGive(musicMutex); }
};

String musicPath(int n) { return "/m" + String(n) + ".mp3"; }
String musicTitlePath(int n) { return "/m" + String(n) + ".txt"; }
bool musicExists(int n) { return n >= 1 && n <= MUSIC_MAX && SPIFFS.exists(musicPath(n)); }

// 더 저장할 수 있는 바이트. SPIFFS 는 꽉 차면 아주 느려지므로 15% 는 남겨 둔다.
size_t musicFreeBytes() {
  size_t total = SPIFFS.totalBytes(), used = SPIFFS.usedBytes();
  size_t usable = total - total / 100 * 15;
  return used >= usable ? 0 : usable - used;
}

String musicTitle(int n) {
  File f = SPIFFS.open(musicTitlePath(n), FILE_READ);
  if (!f)
    return "곡 " + String(n);
  String t = f.readString();
  f.close();
  t.trim();
  return t.isEmpty() ? "곡 " + String(n) : t;
}

// 지금 상태를 알린다. action 은 요청받은 동작 이름(그대로 돌려준다). extra 는 "rdy"/"dn" 같은 표시.
void musicAck(const char *action, bool ok, const char *err = nullptr, const char *extra = nullptr) {
  DynamicJsonDocument res(256);
  res["c"] = "mu";
  res["m"] = wifi.mac;
  res["a"] = action;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  if (extra)
    res[extra] = 1;
  res["pl"] = musicOn ? 1 : 0;
  res["sn"] = musicCur;
  res["vl"] = musicVolume;
  res["fr"] = (int)(musicFreeBytes() / 1024);
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// 곡 목록을 한 메시지로 보낸다. (웹이 받은 메시지를 연달아 보내면 앞의 것이 사라질 수 있어서 하나로 모은다)
void musicList() {
  String list;
  for (int n = 1; n <= MUSIC_MAX; n++) {
    if (!musicExists(n))
      continue;
    File f = SPIFFS.open(musicPath(n), FILE_READ);
    size_t size = f ? f.size() : 0;
    if (f)
      f.close();
    String title = utf8Truncate(musicTitle(n), 20);
    title.replace("|", " ");
    title.replace("\t", " ");
    if (!list.isEmpty())
      list += "\t";
    list += String(n) + "|" + String((unsigned long)((size + 1023) / 1024)) + "|" + title;
  }
  DynamicJsonDocument res(512);
  res["c"] = "mu";
  res["m"] = wifi.mac;
  res["a"] = "list";
  res["ok"] = 1;
  res["tx"] = list;
  res["pl"] = musicOn ? 1 : 0;
  res["sn"] = musicCur;
  res["vl"] = musicVolume;
  res["fr"] = (int)(musicFreeBytes() / 1024);
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// 다음 곡 번호 (마지막 다음은 처음으로). 곡이 없으면 0.
int musicNext(int from) {
  for (int i = 1; i <= MUSIC_MAX; i++) {
    int n = (from - 1 + i) % MUSIC_MAX + 1;
    if (musicExists(n))
      return n;
  }
  return 0;
}

int musicCount() {
  int c = 0;
  for (int n = 1; n <= MUSIC_MAX; n++)
    if (musicExists(n))
      c++;
  return c;
}

bool musicStartFile(int n, const char *&err) {
  // 오디오 라이브러리가 알려 주는 메시지(헤더를 못 읽었다 등)를 시리얼에 남긴다.
  // (함수가 아니라 람다로 쓴다: Arduino 가 만드는 함수 선언이 Audio.h 보다 앞에 놓여 컴파일이 안 된다)
  Audio::audio_info_callback = [](Audio::msg_t i) {
    if (i.msg)
      Serial.printf("[Audio] %s\n", i.msg);
  };
  musicMaxSec = 0;
  if (!musicExists(n)) {
    err = "no_song";
    return false;
  }
  MusicLock lock;
  chatAudio->stopSong();
  if (!chatAudio->connecttoFS(SPIFFS, musicPath(n).c_str())) {
    err = "play_fail";
    return false;
  }
  musicCur = n;
  musicStartedAt = millis();
  Serial.printf("[내 음악] %d번 재생: %s\n", n, musicTitle(n).c_str());
  return true;
}

bool musicPlay(int n, const char *&err) {
  if (!musicExists(n)) {
    err = "no_song";
    return false;
  }
  chatForceStop();        // 대화 중이면 먼저 끈다
  radioStopPlaying(true); // 라디오가 나오고 있으면 끈다
  speakStop(true);        // 말하기가 나오고 있으면 끈다
  if (!audioHwInit(err))
    return false;
  digitalWrite(SPK_SD, HIGH); // 앰프 켜기
  chatAudio->setVolume(musicVolume);
  musicFailStreak = 0;
  musicEnsureTask();
  if (!musicStartFile(n, err))
    return false;
  musicOn = true;
  return true;
}

// 음악을 멈춘다. notify 이면 웹에도 알린다. (라디오나 대화를 켜서 음악이 꺼질 때 쓴다)
void musicStop(bool notify) {
  if (!musicOn)
    return;
  musicOn = false;
  if (chatAudio) {
    MusicLock lock;
    chatAudio->stopSong();
  }
  if (!chatOn)
    digitalWrite(SPK_SD, LOW); // 앰프 끄기
  Serial.println("[내 음악] 정지");
  if (notify)
    musicAck("stop", true);
}

bool musicDelete(int n) {
  if (!musicExists(n))
    return false;
  if (musicOn && musicCur == n)
    musicStop(false);
  SPIFFS.remove(musicPath(n));
  SPIFFS.remove(musicTitlePath(n));
  return true;
}

// USB 로 mp3 를 받아 저장한다. (MQTT 로는 받지 않는다: 서버를 거치지 않고 사용자의 PC 에서 보드로 바로 저장)
// 시작 응답(rdy) 뒤에 웹이 MUSIC_CHUNK 바이트씩 보내고, 한 덩어리를 쓸 때마다 ACK 한 바이트를 돌려준다.
// 받는 동안은 loop() 의 다른 일(Otto 동작, MQTT)을 하지 않는다. 몇 분 걸리면 MQTT 가 끊겼다가 끝난 뒤 다시 붙는다.
void musicPut(JsonDocument &doc) {
  if (commandViaMqtt) {
    musicAck("put", false, "usb_only");
    return;
  }
  size_t size = doc["sz"] | 0;
  if (size == 0) {
    musicAck("put", false, "bad_param");
    return;
  }
  int slot = doc["sn"] | 0;
  if (slot < 1 || slot > MUSIC_MAX) { // 비어 있는 번호를 찾는다
    slot = 0;
    for (int n = 1; n <= MUSIC_MAX; n++) {
      if (!musicExists(n)) {
        slot = n;
        break;
      }
    }
    if (slot == 0) {
      musicAck("put", false, "full");
      return;
    }
  } else if (musicExists(slot)) {
    musicDelete(slot); // 같은 번호에 다시 저장하면 바꾼다
  }
  if (size > musicFreeBytes()) {
    musicAck("put", false, "no_space");
    return;
  }

  musicStop(false);
  radioStopPlaying(false);
  speakStop(false);
  chatForceStop();

  File f = SPIFFS.open(musicPath(slot), FILE_WRITE);
  if (!f) {
    musicAck("put", false, "open_fail");
    return;
  }
  String title = doc["tx"] | "";
  title = utf8Truncate(title, 60);

  musicReceiving = true;
  musicAck("put", true, nullptr, "rdy");

  static uint8_t buf[MUSIC_CHUNK];
  size_t got = 0;
  const char *fail = nullptr;
  unsigned long last = millis();
  while (got < size && !fail) {
    size_t want = min(MUSIC_CHUNK, size - got);
    size_t n = 0;
    while (n < want) {
      int avail = Serial.available();
      if (avail > 0) {
        n += Serial.readBytes(buf + n, min((size_t)avail, want - n));
        last = millis();
      } else if (millis() - last > MUSIC_RX_TIMEOUT_MS) {
        fail = "timeout";
        break;
      } else {
        delay(1);
      }
    }
    if (fail)
      break;
    if (f.write(buf, want) != want) {
      fail = "write_fail";
      break;
    }
    got += want;
    Serial.write(MUSIC_ACK);
  }
  f.close();
  musicReceiving = false;

  if (fail) {
    SPIFFS.remove(musicPath(slot));
    while (Serial.available())
      Serial.read(); // 남은 데이터를 버린다
    Serial.println();
    musicCur = 0;
    musicAck("put", false, fail);
    return;
  }

  File t = SPIFFS.open(musicTitlePath(slot), FILE_WRITE);
  if (t) {
    t.print(title);
    t.close();
  }
  Serial.println();
  musicCur = slot;
  Serial.printf("[내 음악] %d번 저장 완료 (%u바이트): %s\n", slot, (unsigned)size, title.c_str());
  musicAck("put", true, nullptr, "dn");
}

// mu 명령 처리
void musicCommand(JsonDocument &doc) {
  String action = doc["a"] | "";

  if (action == "put") {
    musicPut(doc);
  } else if (action == "play") {
    int n = doc["sn"] | musicCur;
    const char *err = nullptr;
    if (musicPlay(n, err)) {
      musicAck("play", true);
    } else {
      Serial.printf("[내 음악] 시작 실패: %s\n", err);
      musicOn = false;
      musicAck("play", false, err);
    }
  } else if (action == "stop") {
    musicStop(false);
    musicAck("stop", true);
  } else if (action == "vol") {
    int v = doc["vl"] | musicVolume;
    musicVolume = constrain(v, 0, MUSIC_VOLUME_MAX);
    if (chatAudio && musicOn && !chatOn) {
      MusicLock lock; // 피드 태스크가 다른 코어에서 돌고 있으므로 잠그고 바꾼다
      chatAudio->setVolume(musicVolume);
    }
    musicAck("vol", true);
  } else if (action == "del") {
    int n = doc["sn"] | 0;
    if (musicDelete(n))
      musicAck("del", true);
    else
      musicAck("del", false, "no_song");
  } else if (action == "list") {
    musicList();
  } else if (action == "get") {
    musicAck("get", true);
  } else {
    musicAck(action.c_str(), false, "bad_action");
  }
}

// loop() 에서 계속 불린다. 소리를 디코딩하고, 곡이 끝나면 다음 곡으로 넘어간다.
void musicLoop() {
  if (!musicOn || chatOn || musicReceiving)
    return;

  bool running;
  uint32_t t = 0;
  {
    MusicLock lock; // 피드 태스크가 다른 코어에서 디코딩하는 중에 읽지 않게 한다
    running = chatAudio->isRunning();
    if (running)
      t = chatAudio->getAudioCurrentTime();
  }
  if (running) {
    if (t > musicMaxSec)
      musicMaxSec = t;
    if (musicMaxSec >= 1)
      musicFailStreak = 0; // 소리가 나왔다
    return;
  }
  if (millis() - musicStartedAt < MUSIC_START_GRACE_MS)
    return;

  // 곡이 끝났다. 소리가 한 번도 안 나왔으면 읽지 못한 곡이다. 모든 곡을 연달아 못 읽으면 멈춘다.
  if (musicMaxSec == 0) {
    musicFailStreak++;
    Serial.printf("[내 음악] %d번 곡을 읽지 못했습니다 (연속 %d)\n", musicCur, musicFailStreak);
  } else {
    musicFailStreak = 0;
  }
  if (musicFailStreak >= musicCount()) {
    Serial.println("[내 음악] 재생할 수 있는 곡이 없어 멈춥니다");
    musicFailStreak = 0;
    musicOn = false;
    {
      MusicLock lock;
      chatAudio->stopSong();
    }
    digitalWrite(SPK_SD, LOW);
    musicAck("play", false, "play_fail");
    return;
  }

  // 다음 곡 (곡이 하나뿐이면 그 곡을 다시)
  int next = musicNext(musicCur);
  const char *err = nullptr;
  if (next == 0 || !musicStartFile(next, err)) {
    Serial.println("[내 음악] 이어서 재생할 수 없어 멈춥니다");
    musicOn = false;
    {
      MusicLock lock;
      chatAudio->stopSong();
    }
    digitalWrite(SPK_SD, LOW);
    musicAck("play", false, err ? err : "no_song");
    return;
  }
  musicAck("get", true); // 곡이 바뀐 것을 웹에 알린다
}

// ============================================================
// 웹으로 저장하기 (standalone)
// 보드가 띄운 화면(http://otto-xxxx.local)에서는 브라우저가 USB(Web Serial)를 쓸 수 없어서,
// [내 음악] → [보드에 저장]은 mp3 를 POST /upload?sz=바이트&tx=제목 으로 보낸다. (i2r-otto.ino 의 setupWeb)
//   musicWebBegin → musicWebWrite (여러 번) → musicWebEnd.  결과는 USB 와 같은 mu put 응답(dn 또는 err)
// ============================================================
File musicWebFile;
int musicWebSlot = 0;
String musicWebTitle;
const char *musicWebErr = nullptr; // nullptr 이면 문제없음

// 저장을 시작한다. 빈 번호를 찾고, 재생 중인 소리를 모두 끈다. 실패하면 false
bool musicWebBegin(size_t size, const String &title) {
  musicWebErr = nullptr;
  musicWebSlot = 0;
  for (int n = 1; n <= MUSIC_MAX; n++) {
    if (!musicExists(n)) {
      musicWebSlot = n;
      break;
    }
  }
  if (musicWebSlot == 0)
    musicWebErr = "full";
  else if (size == 0)
    musicWebErr = "bad_param";
  else if (size > musicFreeBytes())
    musicWebErr = "no_space";
  if (musicWebErr)
    return false;

  musicStop(false);
  radioStopPlaying(false);
  speakStop(false);
  chatForceStop();
  musicWebFile = SPIFFS.open(musicPath(musicWebSlot), FILE_WRITE);
  if (!musicWebFile) {
    musicWebErr = "open_fail";
    return false;
  }
  musicWebTitle = utf8Truncate(title, 60);
  musicReceiving = true;
  Serial.printf("[내 음악] 웹으로 %d번 저장 시작 (%u바이트)\n", musicWebSlot, (unsigned)size);
  return true;
}

void musicWebWrite(const uint8_t *buf, size_t n) {
  if (musicWebErr || !musicWebFile)
    return;
  if (musicWebFile.write(buf, n) != n)
    musicWebErr = "write_fail";
}

// 끝내기. ok 가 false 이면(중간에 끊김) 지운다. 결과를 웹에 알린다.
void musicWebEnd(bool ok) {
  if (musicWebFile)
    musicWebFile.close();
  musicReceiving = false;
  if (!ok && !musicWebErr)
    musicWebErr = "timeout";
  if (musicWebErr) {
    if (musicWebSlot > 0)
      SPIFFS.remove(musicPath(musicWebSlot));
    musicAck("put", false, musicWebErr);
    return;
  }
  File t = SPIFFS.open(musicTitlePath(musicWebSlot), FILE_WRITE);
  if (t) {
    t.print(musicWebTitle);
    t.close();
  }
  musicCur = musicWebSlot;
  Serial.printf("[내 음악] %d번 저장 완료: %s\n", musicWebSlot, musicWebTitle.c_str());
  musicAck("put", true, nullptr, "dn");
}
