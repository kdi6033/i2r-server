// ============================================================
// radio.ino - Otto 인터넷 라디오
//
// chatbot/radio/radio.ino 의 방송국 목록과 재생 방식을 Otto 에 옮긴 것이다.
// 시리얼 대신 웹의 [라디오] 패널이 보내는 rd 명령으로 조작한다.
//
//   {"c":"rd","m":"..","a":"play","sn":2}   2번 방송국(0부터) 재생
//   {"c":"rd","m":"..","a":"stop"}          라디오 끄기
//   {"c":"rd","m":"..","a":"vol","vl":12}   볼륨 (0 ~ 21)
//   {"c":"rd","m":"..","a":"get"}           지금 상태 알려 달라는 요청
//
// 응답: {"c":"rd","m":"..","a":"<요청>","ok":1,"pl":1,"sn":2,"vl":12}
//        pl 재생 중(1/0), sn 방송국 번호, vl 볼륨. 실패하면 "ok":0,"err":"...".
//        노래가 바뀌면 {"c":"rd","a":"title","tx":"가수 - 제목"} 도 보낸다.
//
// 스피커(MAX98357A)와 Audio 객체는 AI 대화(chat.ino)와 같이 쓴다. 대화를 켜면 라디오가 꺼진다.
// Arduino IDE 에서 Tools → PSRAM 을 "OPI PSRAM" 으로 선택해야 한다.
// ============================================================

struct RadioStation {
  const char *name;
  const char *url;
};

// 웹의 방송국 목록(OttoPad.tsx)과 순서가 같아야 한다.
const RadioStation radioStations[] = {
    {"Classic Praha (체코 프라하 클래식 라디오)", "http://icecast8.play.cz/classic128.mp3"},
    {"TBN 부산교통방송 (한국 트로트/7080 가요)", "http://radio2.tbn.or.kr:1935/busan/myStream/playlist.m3u8"},
    {"Power 181 (미국 Top 40 팝송)", "http://listen.181fm.com/181-power_128k.mp3"},
    {"ENERGY K-Pop (한국 가요/케이팝)", "http://frontend.streamonkey.net/energy-kpop/stream/mp3"},
    {"YourClassical Lullabies (미국 태교/자장가 클래식)", "http://lullabies.stream.publicradio.org/lullabies.mp3"},
    {"TBN 경인교통방송 (한국 트로트/7080 가요)", "http://radio2.tbn.or.kr:1935/gyeongin/myStream/playlist.m3u8"},
};
const int RADIO_STATION_COUNT = sizeof(radioStations) / sizeof(radioStations[0]);
const int RADIO_VOLUME_MAX = 21;
const unsigned long RADIO_STALL_MS = 10000; // 이 시간 동안 소리가 안 나오면 다시 연결한다
const int RADIO_MAX_RETRY = 3;              // 다시 연결하는 최대 횟수

bool radioOn = false;     // 라디오가 켜져 있는지
int radioStation = 0;     // 지금(또는 마지막) 방송국 번호
int radioVolume = 12;     // 볼륨 (0 ~ 21)
unsigned long radioLastRunning = 0; // 마지막으로 소리가 나온 시각 (끊김 감지용)
int radioRetry = 0;

// 지금 상태를 웹으로 알린다. action 은 요청받은 동작 이름(그대로 돌려준다).
void radioAck(const char *action, bool ok, const char *err = nullptr) {
  DynamicJsonDocument res(256);
  res["c"] = "rd";
  res["m"] = wifi.mac;
  res["a"] = action;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  res["pl"] = radioOn ? 1 : 0;
  res["sn"] = radioStation;
  res["vl"] = radioVolume;
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// 노래 제목(ICY 메타데이터)을 웹으로 알린다. (MQTT 메시지는 300바이트 이하여야 해서 줄인다)
void radioPublishTitle(const char *title) {
  DynamicJsonDocument d(384);
  d["c"] = "rd";
  d["m"] = wifi.mac;
  d["a"] = "title";
  d["tx"] = utf8Truncate(String(title), 110);
  String out;
  serializeJson(d, out);
  wifi.publishOut(out);
}

// 방송국에 연결해서 재생을 시작한다.
bool radioConnect(int idx) {
  Serial.printf("[라디오] 채널 %d: %s\n", idx + 1, radioStations[idx].name);
  Serial.printf("[라디오] 주소: %s\n", radioStations[idx].url);
  chatAudio->stopSong();
  return chatAudio->connecttohost(radioStations[idx].url);
}

bool radioStart(int idx, const char *&err) {
  if (idx < 0 || idx >= RADIO_STATION_COUNT) {
    err = "bad_param";
    return false;
  }
  chatForceStop(); // 대화 중이면 먼저 끈다
  musicStop(true); // 내 음악이 나오고 있으면 끈다 (스피커를 같이 쓴다)
  speakStop(true); // 말하기가 나오고 있으면 끈다
  if (!audioHwInit(err))
    return false;

  digitalWrite(SPK_SD, HIGH); // 앰프 켜기
  chatAudio->setVolume(radioVolume);
  radioStation = idx;
  radioRetry = 0;
  radioLastRunning = millis();
  if (!radioConnect(idx)) {
    err = "connect_fail";
    return false;
  }
  radioOn = true;
  return true;
}

// 라디오를 끈다. notify 이면 웹에도 알린다. (대화를 켜서 라디오가 꺼질 때 쓴다)
void radioStopPlaying(bool notify) {
  if (!radioOn)
    return;
  radioOn = false;
  if (chatAudio)
    chatAudio->stopSong();
  if (!chatOn)
    digitalWrite(SPK_SD, LOW); // 앰프 끄기
  Serial.println("[라디오] 정지");
  if (notify)
    radioAck("stop", true);
}

// rd 명령 처리
void radioCommand(JsonDocument &doc) {
  String action = doc["a"] | "";

  if (action == "play") {
    int idx = doc["sn"] | radioStation;
    const char *err = nullptr;
    if (radioStart(idx, err)) {
      radioAck("play", true);
    } else {
      Serial.printf("[라디오] 시작 실패: %s\n", err);
      radioOn = false;
      radioAck("play", false, err);
    }
  } else if (action == "stop") {
    radioStopPlaying(false);
    radioAck("stop", true);
  } else if (action == "vol") {
    int v = doc["vl"] | radioVolume;
    radioVolume = constrain(v, 0, RADIO_VOLUME_MAX);
    if (chatAudio && radioOn && !chatOn)
      chatAudio->setVolume(radioVolume);
    Serial.printf("[라디오] 볼륨 %d / %d\n", radioVolume, RADIO_VOLUME_MAX);
    radioAck("vol", true);
  } else if (action == "get") {
    radioAck("get", true);
  } else {
    radioAck(action.c_str(), false, "bad_action");
  }
}

// loop() 에서 계속 불린다. 스트림 데이터를 디코딩하고, 끊기면 다시 연결한다.
void radioLoop() {
  if (!radioOn || chatOn)
    return;

  chatAudio->loop(); // 스트리밍 데이터를 계속 받아서 재생한다 (필수)

  unsigned long now = millis();
  if (chatAudio->isRunning()) {
    radioLastRunning = now;
    return;
  }
  if (now - radioLastRunning < RADIO_STALL_MS)
    return;

  // 소리가 오래 안 나오면 다시 연결한다. 몇 번 해도 안 되면 포기하고 웹에 알린다.
  if (radioRetry < RADIO_MAX_RETRY) {
    radioRetry++;
    Serial.printf("[라디오] 소리가 끊겨 다시 연결합니다 (%d/%d)\n", radioRetry, RADIO_MAX_RETRY);
    radioLastRunning = now;
    radioConnect(radioStation);
  } else {
    Serial.println("[라디오] 연결할 수 없어 멈춥니다");
    radioOn = false;
    chatAudio->stopSong();
    digitalWrite(SPK_SD, LOW);
    radioAck("play", false, "stream_fail");
  }
}

// ESP32-audioI2S 콜백: 방송국 이름 / 노래 제목
void audio_showstation(const char *info) { Serial.printf("[방송] %s\n", info); }
void audio_showstreamtitle(const char *info) {
  Serial.printf("[노래] %s\n", info);
  if (radioOn && strlen(info) > 0)
    radioPublishTitle(info);
}
