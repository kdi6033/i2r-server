// ============================================================
// chat.ino - Otto AI 음성 대화 (마이크 → Gemini STT → Gemini LLM → Google TTS → 스피커)
//
// chatbot/llm/llm.ino 의 대화 흐름을 Otto 에 옮긴 것이다.
//  - 웹의 [AI 대화] 버튼이 보내는 lm 명령(v=1 켜기 / v=0 끄기)으로 켜고 끈다.
//  - 켜져 있는 동안 MQTT 는 계속 처리하고(정지·종료 명령을 받는다), 대화가 한 번 끝날 때마다
//    lr 메시지(내가 한 말 / 로봇의 답)를 웹으로 보낸다.
//  - Gemini API 키는 펌웨어에 들어 있지 않다. 웹의 [API 키]에서 입력하면 ak 명령으로 보드에 전달되어
//    NVS("cfg" 네임스페이스)에 저장되고, 부팅할 때 읽어 온다. 키를 웹으로 되돌려 보내지는 않는다.
//
// 하드웨어 연결 (llm.ino 와 같다):
//   [마이크 INMP441]            [스피커 MAX98357A]
//   GPIO  9 → SD (데이터)      GPIO 13 → DIN  (데이터)
//   GPIO  8 → SCK (클럭)       GPIO 12 → BCLK (클럭)
//   GPIO 10 → WS  (채널선택)   GPIO 11 → LRCLK(채널선택)
//   3.3V    → VDD               GPIO 14 → SD   (앰프 ON/OFF)
//   GND     → GND + L/R        3.3V    → VIN
//
// 주의: Arduino IDE 에서 Tools → PSRAM 을 "OPI PSRAM" 으로 선택해야 한다. (녹음 버퍼가 PSRAM 에 잡힌다)
// ============================================================

#include "Audio.h" // ESP32-audioI2S (schreibfaul1)
#include "NetworkClientSecure.h"
#include "driver/i2s_std.h" // 새 I2S 드라이버 (마이크) - legacy 충돌 방지
#include "mbedtls/base64.h"

// ── Gemini API 키 (NVS 에 저장. 펌웨어 파일에는 키가 들어 있지 않다) ──
String geminiKey; // 비어 있으면 AI 대화를 켤 수 없다 (err = "no_key")

// 키 확인 결과: 1 정상 / 0 거부됨(잘못된 키) / -1 확인 못 함(키 없음·Wi-Fi 없음·통신 오류)
int chatKeyCheck();
extern bool chatOn, chatWant; // 아래에서 정의한다
bool chatKeyCheckPending = false; // 부팅 직후 한 번 확인하려고 기다리는 중

// 부팅할 때 NVS 에서 키를 읽는다.
void chatLoadKey() {
  preferences.begin("cfg", true);
  geminiKey = preferences.getString("gemini", "");
  preferences.end();
  Serial.printf("[API 키] %s\n", geminiKey.isEmpty() ? "저장된 키가 없습니다" : "저장된 키가 있습니다");
}

// ak 명령: 키를 저장/삭제한다.  {"c":"ak","m":"..","k":"키"} 저장, "k":"" 삭제, "a":"get" 이면 설정 여부만 알려 준다.
// 응답에는 키를 넣지 않고 설정 여부(has)만 넣는다.
void chatKeyCommand(JsonDocument &doc) {
  const char *err = nullptr;

  if (!doc["k"].isNull()) {
    String k = doc["k"] | "";
    k.trim();
    if (k.length() == 0) {
      preferences.begin("cfg", false);
      preferences.remove("gemini");
      preferences.end();
      geminiKey = "";
      Serial.println("[API 키] 삭제했습니다");
    } else if (k.length() < 20 || k.length() > 120 ||
               strspn(k.c_str(), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.") != k.length()) {
      err = "bad_param"; // 키는 영문자·숫자·-_. 만 쓰고 길이는 20~120자
    } else {
      preferences.begin("cfg", false);
      size_t n = preferences.putString("gemini", k);
      preferences.end();
      if (n == 0) {
        err = "nvs_fail";
      } else {
        geminiKey = k;
        Serial.printf("[API 키] 저장했습니다 (%d자)\n", (int)k.length()); // 키 자체는 출력하지 않는다
      }
    }
  }

  DynamicJsonDocument res(200);
  res["c"] = "ak";
  res["m"] = wifi.mac;
  res["ok"] = err ? 0 : 1;
  if (err)
    res["err"] = err;
  res["has"] = geminiKey.isEmpty() ? 0 : 1;
  if (!err && !geminiKey.isEmpty() && !doc["k"].isNull()) {
    if (chatOn || chatWant) {
      chatKeyCheckPending = true; // 대화 중에는 음성이 끊기지 않도록 확인을 미루고, 대화가 끝나면 한다 (응답에 kv 없음)
    } else {
      res["kv"] = chatKeyCheck(); // 방금 저장한 키가 Gemini 에서 받아들여지는지
    }
  }
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

#define CHAT_VOLUME 18 // AI 대화 음성 크기 (0~21)

// radio.ino (인터넷 라디오): 스피커를 같이 쓰므로 대화를 켜면 라디오를 끈다
void radioStopPlaying(bool notify);

// speak.ino (말하기): 스피커를 같이 쓰므로 대화를 켜면 말하기를 끈다
void speakStop(bool notify);

// light.ino (조명): 대화 상태를 LED 색으로 알린다. 녹색 = 목소리 입력 중, 빨간색 = 그 밖의 때
void lightChatColor(bool green);
void lightChatEnd();

#define GEMINI_HOST "generativelanguage.googleapis.com"

// 키 확인: 모델 목록 조회(GET) 한 번. 키 값은 로그에 남기지 않는다.
int chatKeyCheck() {
  if (geminiKey.isEmpty() || WiFi.status() != WL_CONNECTED)
    return -1;
  NetworkClientSecure *tls = new NetworkClientSecure;
  tls->setInsecure();
  tls->setTimeout(8);
  if (!tls->connect(GEMINI_HOST, 443)) {
    Serial.println("[API 키] 확인 실패 (네트워크) (통신 오류)");
    delete tls;
    return -1;
  }
  tls->printf("GET /v1beta/models?pageSize=1&key=%s HTTP/1.0\r\n", geminiKey.c_str());
  tls->printf("Host: %s\r\n\r\n", GEMINI_HOST);
  String status = "";
  unsigned long t = millis();
  while (millis() - t < 8000 && status.indexOf('\n') < 0) {
    while (tls->available() && status.indexOf('\n') < 0)
      status += (char)tls->read();
    if (!tls->connected() && !tls->available())
      break;
    delay(1);
  }
  tls->stop();
  delete tls;
  int sp = status.indexOf(' ');
  int code = sp > 0 ? status.substring(sp + 1, sp + 4).toInt() : 0;
  if (code == 200) {
    Serial.println("[API 키] 확인됨 (정상)");
    return 1;
  }
  if (code == 400 || code == 401 || code == 403) {
    Serial.printf("[API 키] 거부됨 (HTTP %d) (키 오류)\n", code);
    return 0;
  }
  Serial.println("[API 키] 확인 실패 (네트워크) (통신 오류)");
  return -1;
}
#define GEMINI_MODEL_STT "gemini-2.5-flash" // STT용
#define GEMINI_MODEL_LLM "gemini-2.5-flash" // LLM 대화용

#define API_RETRY_COUNT 3
#define API_RETRY_DELAY_MS 1500

// ── 마이크 핀 (INMP441) ──────────────────────────────────────
#define MIC_SD 9
#define MIC_SCK 8
#define MIC_WS 10

// ── 스피커 핀 (MAX98357A) ────────────────────────────────────
#define SPK_BCLK 12
#define SPK_LRC 11
#define SPK_DOUT 13
#define SPK_SD 14

// ── 녹음 및 VAD 설정 ─────────────────────────────────────────
#define SAMPLE_RATE 16000
#define MAX_REC_SECONDS 10 // 최대 10초 녹음
#define MAX_PCM_SIZE (SAMPLE_RATE * MAX_REC_SECONDS * 2)
#define MAX_WAV_SIZE (MAX_PCM_SIZE + 44)
#define MAX_B64_SIZE (MAX_WAV_SIZE * 4 / 3 + 8)

#define VAD_THRESHOLD 600        // 목소리 감지 기준 (너무 민감하면 올린다)
#define SILENCE_THRESHOLD 400    // 무음 판단 기준
#define SILENCE_DURATION_MS 750 // 0.75초 조용하면 녹음 종료

// ── 웹으로 보내는 대화 기록 크기 (MQTT 메시지는 300바이트 이하여야 한다) ──
#define LOG_USER_BYTES 90  // 내가 한 말 (한글 약 30자)
#define LOG_REPLY_BYTES 130 // 로봇의 답 (한글 약 43자)
#define TTS_MAX_CHARS 180   // 음성으로 읽는 최대 글자 수

// ── 대화 기록 (다중 턴) ───────────────────────────────────────
#define MAX_HISTORY 10 // 최대 대화 기록 턴 수
struct Turn {
  String user;
  String assistant;
};
Turn chatHistory[MAX_HISTORY];
int chatHistoryCount = 0;

// ── 버퍼 / 장치 ──────────────────────────────────────────────
uint8_t *wavBuf = nullptr;
char *b64Buf = nullptr;
Audio *chatAudio = nullptr; // 처음 대화를 켤 때 만든다 (평소에는 I2S 를 쓰지 않는다)
i2s_chan_handle_t mic_rx_chan = NULL;
bool chatHwReady = false; // 마이크/스피커를 한 번 초기화했는지

// ── 상태 ─────────────────────────────────────────────────────
enum ChatState { CHAT_RECORDING, CHAT_THINKING, CHAT_SPEAKING };
ChatState chatState = CHAT_SPEAKING;
bool chatOn = false;   // 대화 모드가 실제로 켜져 있는지
bool chatWant = false; // 웹이 요청한 상태 (lm 명령). 실제 전환은 chatLoop() 에서 한다.

// ============================================================
// 외부에서 쓰는 함수
// ============================================================
// lm 명령: 켜기/끄기 요청. 무거운 초기화는 chatLoop() 에서 한다 (MQTT 콜백 안에서 하지 않는다).
void chatSetEnabled(bool on) { chatWant = on; }

// 대화가 켜져 있거나 켜지는 중이면 true. (동작 명령 ot 를 막는 데 쓴다)
bool chatBusy() { return chatOn && chatWant; }

// ============================================================
// lm 응답 / lr 대화 기록을 웹으로 보낸다
// ============================================================
void chatAck(bool on, bool ok, const char *err = nullptr) {
  DynamicJsonDocument res(200);
  res["c"] = "lm";
  res["m"] = wifi.mac;
  res["v"] = on ? 1 : 0;
  res["ok"] = ok ? 1 : 0;
  if (err)
    res["err"] = err;
  String out;
  serializeJson(res, out);
  wifi.publishOut(out);
}

// UTF-8 문자를 자르지 않도록 maxBytes 이하로 줄인다. 잘렸으면 끝에 "…" 를 붙인다.
String utf8Truncate(const String &s, size_t maxBytes) {
  if (s.length() <= maxBytes)
    return s;
  size_t cut = maxBytes;
  while (cut > 0 && (((uint8_t)s[cut]) & 0xC0) == 0x80) // 문자 중간이면 앞으로 당긴다
    cut--;
  return s.substring(0, cut) + "…";
}

// 글자 수(한글 1자 = 1)로 줄인다. TTS 에 너무 긴 글을 보내지 않기 위해 쓴다.
String utf8TruncateChars(const String &s, int maxChars) {
  int chars = 0;
  for (size_t i = 0; i < s.length(); i++) {
    if ((((uint8_t)s[i]) & 0xC0) != 0x80) {
      if (chars == maxChars)
        return s.substring(0, i);
      chars++;
    }
  }
  return s;
}

// 대화 한 턴을 웹으로 알린다: {"c":"lr","m":"..","us":"내 말","tx":"로봇 답"}
void chatPublishLog(const String &user, const String &reply) {
  DynamicJsonDocument d(512);
  d["c"] = "lr";
  d["m"] = wifi.mac;
  d["us"] = utf8Truncate(user, LOG_USER_BYTES);
  d["tx"] = utf8Truncate(reply, LOG_REPLY_BYTES);
  String out;
  serializeJson(d, out);
  wifi.publishOut(out);
}

// ============================================================
// JSON 에 넣을 문자열 이스케이프 (따옴표, 줄바꿈 등이 들어 있어도 JSON 이 깨지지 않게)
// ============================================================
String jsonEscape(const String &s) {
  String r;
  r.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"') r += "\\\"";
    else if (c == '\\') r += "\\\\";
    else if (c == '\n') r += "\\n";
    else if (c == '\r') r += "\\r";
    else if (c == '\t') r += "\\t";
    else if ((uint8_t)c < 0x20) r += ' ';
    else r += c;
  }
  return r;
}

// ============================================================
// 마이크 초기화 (새 I2S 드라이버 - i2s_std.h)
// ============================================================
void setupMic() {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &mic_rx_chan));

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                  I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = (gpio_num_t)MIC_SCK,
              .ws = (gpio_num_t)MIC_WS,
              .dout = I2S_GPIO_UNUSED,
              .din = (gpio_num_t)MIC_SD,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; // INMP441: 왼쪽 채널

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(mic_rx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(mic_rx_chan));
  Serial.println("[OK] 마이크 초기화 완료");
}

// ============================================================
// WAV 헤더 작성 (44바이트)
// ============================================================
void writeWavHeader(uint8_t *buf, uint32_t pcmSize) {
  uint32_t fileSize = pcmSize + 36;
  uint32_t byteRate = SAMPLE_RATE * 2;
  uint16_t blockAlign = 2, bitsPerSample = 16, numChannels = 1;
  uint16_t audioFormat = 1;
  uint32_t fmtSize = 16, sampleRate = SAMPLE_RATE;

  memcpy(buf, "RIFF", 4);
  memcpy(buf + 4, &fileSize, 4);
  memcpy(buf + 8, "WAVE", 4);
  memcpy(buf + 12, "fmt ", 4);
  memcpy(buf + 16, &fmtSize, 4);
  memcpy(buf + 20, &audioFormat, 2);
  memcpy(buf + 22, &numChannels, 2);
  memcpy(buf + 24, &sampleRate, 4);
  memcpy(buf + 28, &byteRate, 4);
  memcpy(buf + 32, &blockAlign, 2);
  memcpy(buf + 34, &bitsPerSample, 2);
  memcpy(buf + 36, "data", 4);
  memcpy(buf + 40, &pcmSize, 4);
}

// ============================================================
// ① 자동 녹음 (VAD: 목소리가 들리면 시작, 조용해지면 종료)
//    녹음 중에도 MQTT 를 처리한다. 대화를 끄는 명령이 오면 바로 빠져나온다. (반환 0)
// ============================================================
size_t chatRecordAudio() {
  Serial.println("[녹음 대기] 말을 시작하면 자동으로 녹음됩니다...");
  lightChatColor(true); // 듣는 중(대기 + 녹음): 녹색

  int32_t raw[128];
  bool isRecording = false;
  uint32_t written = 0;
  uint8_t *pcmPtr = wavBuf + 44;
  unsigned long silenceStartTime = 0;
  int activeChunks = 0; // 유효 소리 유지 프레임 카운트

  while (written < MAX_PCM_SIZE) {
    size_t bytesRead = 0;
    i2s_channel_read(mic_rx_chan, raw, sizeof(raw), &bytesRead, portMAX_DELAY);

    // 약 8ms 마다 MQTT 를 처리한다 (정지/종료 명령, 하트비트)
    wifi.loop();
    if (!chatWant)
      return 0;

    // 소리 크기(RMS) 계산
    int64_t sumSq = 0;
    int samples = bytesRead / 4;
    if (samples <= 0)
      continue;
    for (int i = 0; i < samples; i++) {
      int16_t sample = (int16_t)(raw[i] >> 14);
      sumSq += sample * sample;
    }
    int rms = sqrt(sumSq / samples);

    // 1. 녹음 대기 중 -> 목소리 감지 시 시작
    if (!isRecording && rms > VAD_THRESHOLD) {
      isRecording = true;
      Serial.println("[녹음 시작] 목소리를 듣고 있습니다...");
    }

    // 2. 녹음 진행 중
    if (isRecording) {
      if (rms > SILENCE_THRESHOLD)
        activeChunks++;

      for (int i = 0; i < samples && written < MAX_PCM_SIZE; i++) {
        int16_t sample = (int16_t)(raw[i] >> 14);
        memcpy(pcmPtr + written, &sample, 2);
        written += 2;
      }

      // 조용한지 확인
      if (rms < SILENCE_THRESHOLD) {
        if (silenceStartTime == 0) {
          silenceStartTime = millis();
        } else if (millis() - silenceStartTime > SILENCE_DURATION_MS) {
          Serial.println("[녹음 종료] 말이 끝난 것을 감지했습니다.");
          break;
        }
      } else {
        silenceStartTime = 0;
      }
    }
  }

  // 1 chunk 가 약 8ms 이므로 activeChunks 가 15 미만이면 짧은 충격음으로 보고 무시한다.
  // (무시하면 바로 다시 듣기 상태가 되므로 녹색을 유지한다)
  if (activeChunks < 15) {
    Serial.println("[녹음 취소] 짧은 노이즈로 판단되어 무시합니다.");
    return 0;
  }

  lightChatColor(false); // 녹음이 끝났다: 빨간색 (AI 가 생각하고 답하는 동안)

  writeWavHeader(wavBuf, written);
  return written + 44; // 헤더 포함 실제 파일 크기
}

// ============================================================
// ② Base64 인코딩
// ============================================================
size_t chatEncodeBase64(size_t wavLen) {
  size_t outLen = 0;
  mbedtls_base64_encode((unsigned char *)b64Buf, MAX_B64_SIZE, &outLen, wavBuf,
                        wavLen);
  b64Buf[outLen] = '\0';
  Serial.printf("[Base64] 완료: %d KB\n", outLen / 1024);
  return outLen;
}

// ============================================================
// HTTPS POST 헬퍼 - 요청 전송 & 응답 본문 반환
// ============================================================
String chatHttpsPost(const String &path, const String &bodyHead,
                     const char *bodyMid, size_t midLen,
                     const String &bodyTail) {
  NetworkClientSecure *tls = new NetworkClientSecure;
  tls->setInsecure();
  if (!tls->connect(GEMINI_HOST, 443)) {
    Serial.println("[오류] Gemini 서버 연결 실패");
    delete tls;
    return "";
  }

  uint32_t totalLen = bodyHead.length() + midLen + bodyTail.length();

  tls->printf("POST %s HTTP/1.0\r\n", path.c_str());
  tls->printf("Host: %s\r\n", GEMINI_HOST);
  tls->println("Content-Type: application/json");
  tls->printf("Content-Length: %d\r\n\r\n", totalLen);

  tls->print(bodyHead);
  if (bodyMid && midLen > 0) {
    for (size_t i = 0; i < midLen; i += 1024) {
      size_t chunk = min((size_t)1024, midLen - i);
      tls->write((const uint8_t *)bodyMid + i, chunk);
    }
  }
  tls->print(bodyTail);

  // 응답 수신
  String response = "";
  unsigned long t = millis();
  while (millis() - t < 20000) {
    while (tls->available())
      response += (char)tls->read();
    if (!tls->connected() && !tls->available())
      break;
    delay(1);
  }
  tls->stop();
  delete tls; // 메모리 반환

  // HTTP 헤더 제거 → JSON 본문만 반환
  int bodyStart = response.indexOf("\r\n\r\n");
  return (bodyStart >= 0) ? response.substring(bodyStart + 4) : response;
}

// JSON 문자열에서 특정 키의 문자열 값 추출
String chatExtractJsonString(const String &json, const String &key) {
  int idx = json.indexOf("\"" + key + "\":");
  if (idx < 0)
    return "";
  int qs = json.indexOf("\"", idx + key.length() + 3);
  if (qs < 0)
    return "";
  qs++;
  String result = "";
  while (qs < (int)json.length()) {
    char c = json[qs];
    if (c == '\\' && qs + 1 < (int)json.length()) {
      char next = json[qs + 1];
      if (next == '"') {
        result += '"';
        qs += 2;
      } else if (next == '\\') {
        result += '\\';
        qs += 2;
      } else if (next == 'n') {
        result += '\n';
        qs += 2;
      } else if (next == 't') {
        result += '\t';
        qs += 2;
      } else {
        result += c;
        qs++;
      }
    } else if (c == '"') {
      break;
    } else {
      result += c;
      qs++;
    }
  }
  return result;
}

// ============================================================
// Gemini 호출 (재시도 포함). 성공하면 응답 본문, 실패하면 "[...]" 형태의 오류 문구를 반환한다.
//  failText: 일반 실패 시 돌려줄 문구 ([인식 실패] 또는 [응답 실패])
// ============================================================
String chatGeminiPost(const char *label, const String &path, const String &head,
                      const char *mid, size_t midLen, const String &tail,
                      const char *failText, bool &ok) {
  ok = false;
  for (int attempt = 1; attempt <= API_RETRY_COUNT; attempt++) {
    if (!chatWant)
      return failText; // 대화가 꺼졌으면 더 시도하지 않는다
    String body = chatHttpsPost(path, head, mid, midLen, tail);
    if (body.length() == 0) {
      if (attempt < API_RETRY_COUNT) {
        Serial.printf("[%s] 전송 실패. 재시도 (%d/%d)...\n", label, attempt, API_RETRY_COUNT);
        delay(API_RETRY_DELAY_MS);
        continue;
      }
      return failText;
    }

    if (body.indexOf("\"error\":") >= 0) {
      if (body.indexOf("RESOURCE_EXHAUSTED") >= 0 || body.indexOf("429") >= 0) {
        Serial.printf("[%s] 할당량 초과(429)\n", label);
        if (attempt < API_RETRY_COUNT) { delay(3000); continue; }
        return "[할당량 초과]";
      } else if (body.indexOf("UNAVAILABLE") >= 0 || body.indexOf("503") >= 0) {
        Serial.printf("[%s] 서버 과부하(503)\n", label);
        if (attempt < API_RETRY_COUNT) { delay(3000); continue; }
        return "[서버 과부하]";
      } else {
        Serial.printf("[%s] API 오류: %s\n", label, body.substring(0, 300).c_str());
        if (attempt < API_RETRY_COUNT) { delay(API_RETRY_DELAY_MS); continue; }
        return failText;
      }
    }

    String text = chatExtractJsonString(body, "text");
    if (text.length() == 0) {
      Serial.printf("[%s] 응답에 text 없음: %s\n", label, body.substring(0, 300).c_str());
      if (attempt < API_RETRY_COUNT) { delay(API_RETRY_DELAY_MS); continue; }
      return failText;
    }
    text.trim();
    ok = true;
    return text;
  }
  return failText;
}

// ③ Gemini STT - 오디오 → 텍스트
String chatGeminiSTT(size_t b64Len, bool &ok) {
  Serial.println("[STT] Gemini API 전송 중...");
  String path = String("/v1beta/models/") + GEMINI_MODEL_STT +
                ":generateContent?key=" + geminiKey;
  String head = "{\"contents\":[{\"parts\":["
                "{\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"";
  String tail = "\"}},"
                "{\"text\":\"이 오디오의 한국어 내용을 텍스트로만 출력해줘. "
                "설명 없이 말한 내용만.\"}]"
                "}],\"generationConfig\":{\"thinkingConfig\":{\"thinkingBudget\":0}}}"; // 생각 시간을 꺼서 응답을 빠르게
  return chatGeminiPost("STT", path, head, b64Buf, b64Len, tail, "[인식 실패]", ok);
}

// ④ Gemini LLM - 텍스트 → AI 응답 (다중 턴 대화)
String chatGeminiLLM(const String &userText, bool &ok) {
  Serial.println("[LLM] Gemini LLM 전송 중...");
  String path = String("/v1beta/models/") + GEMINI_MODEL_LLM +
                ":generateContent?key=" + geminiKey;

  String body = "{"
                "\"system_instruction\":{"
                "\"parts\":[{\"text\":\"당신은 사람과 대화하는 Otto 로봇입니다. "
                "친절하고 유쾌한 한국어로 대답하세요. "
                "답변은 2~3문장 이내로 간략하게 해주세요.\"}]"
                "},"
                "\"contents\":[";
  for (int i = 0; i < chatHistoryCount; i++) {
    body += "{\"role\":\"user\",\"parts\":[{\"text\":\"";
    body += jsonEscape(chatHistory[i].user);
    body += "\"}]},";
    body += "{\"role\":\"model\",\"parts\":[{\"text\":\"";
    body += jsonEscape(chatHistory[i].assistant);
    body += "\"}]},";
  }
  body += "{\"role\":\"user\",\"parts\":[{\"text\":\"";
  body += jsonEscape(userText);
  body += "\"}]}";
  body += "],\"generationConfig\":{\"thinkingConfig\":{\"thinkingBudget\":0}}}"; // 생각 시간을 꺼서 응답을 빠르게

  String answer = chatGeminiPost("LLM", path, body, nullptr, 0, "", "[응답 실패]", ok);
  if (!ok)
    return answer;

  // 대화 기록 저장 (가득 차면 가장 오래된 것을 민다)
  if (chatHistoryCount < MAX_HISTORY) {
    chatHistory[chatHistoryCount].user = userText;
    chatHistory[chatHistoryCount].assistant = answer;
    chatHistoryCount++;
  } else {
    for (int i = 0; i < MAX_HISTORY - 1; i++)
      chatHistory[i] = chatHistory[i + 1];
    chatHistory[MAX_HISTORY - 1].user = userText;
    chatHistory[MAX_HISTORY - 1].assistant = answer;
  }
  return answer;
}

// ============================================================
// URL 인코딩 (TTS용)
// ============================================================
String chatUrlEncode(const String &text) {
  String encoded = "";
  const char *src = text.c_str();
  while (*src) {
    uint8_t c = (uint8_t)*src;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      encoded += (char)c;
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", c);
      encoded += hex;
    }
    src++;
  }
  return encoded;
}

// ⑤ TTS 재생 시작 (끝날 때까지 기다리지 않는다)
void chatStartTTS(const String &text) {
  String ttsText = text;
  if (text == "[인식 실패]") {
    ttsText = "잘 들리지 않았습니다. 다시 말씀해 주세요.";
  } else if (text == "[할당량 초과]") {
    ttsText = "API 사용량이 초과되었습니다. 잠시 후에 다시 대화해 주세요.";
  } else if (text == "[서버 과부하]") {
    ttsText = "서버가 혼잡합니다. 잠시 후에 다시 시도해 주세요.";
  } else if (text == "[응답 실패]") {
    ttsText = "죄송합니다. 오류가 발생하여 응답을 받을 수 없습니다.";
  }
  ttsText = utf8TruncateChars(ttsText, TTS_MAX_CHARS);

  // Google TTS (메모리를 아끼기 위해 http 사용)
  String url = "http://translate.google.com/translate_tts"
               "?ie=UTF-8&client=tw-ob&tl=ko&q=" +
               chatUrlEncode(ttsText);
  Serial.printf("[TTS] 음성 출력: \"%s\"\n", ttsText.c_str());
  chatAudio->connecttohost(url.c_str());
  chatState = CHAT_SPEAKING;
}

// ============================================================
// 대화 켜기 / 끄기
// ============================================================
// 마이크·스피커·버퍼를 준비하고 인사말로 시작한다. 실패하면 false 와 이유를 돌려준다.
// 마이크·스피커를 한 번만 초기화한다. (AI 대화와 라디오가 같이 쓴다)
// 마이크를 Audio 라이브러리보다 먼저 초기화해야 한다. 실패하면 false 와 이유를 돌려준다.
bool audioHwInit(const char *&err) {
  if (!psramFound()) {
    err = "no_psram"; // Tools → PSRAM → OPI PSRAM
    return false;
  }
  if (!chatHwReady) {
    setupMic();
    delay(200);
    chatAudio = new Audio();
    pinMode(SPK_SD, OUTPUT);
    digitalWrite(SPK_SD, HIGH); // 앰프 켜기
    delay(200);
    chatAudio->setPinout(SPK_BCLK, SPK_LRC, SPK_DOUT);
    chatAudio->setVolume(CHAT_VOLUME);
    chatHwReady = true;
    Serial.println("[OK] 마이크/스피커 초기화 완료");
  }
  return true;
}

bool chatBegin(const char *&err) {
  if (geminiKey.isEmpty()) {
    err = "no_key"; // 웹의 [API 키]에서 Gemini API 키를 먼저 입력해야 한다
    return false;
  }
  if (!audioHwInit(err))
    return false;
  if (!wavBuf)
    wavBuf = (uint8_t *)ps_malloc(MAX_WAV_SIZE);
  if (!b64Buf)
    b64Buf = (char *)ps_malloc(MAX_B64_SIZE);
  if (!wavBuf || !b64Buf) {
    err = "no_mem";
    return false;
  }

  radioStopPlaying(true); // 라디오가 나오고 있으면 끈다 (스피커를 같이 쓴다)
  musicStop(true);        // 내 음악이 나오고 있으면 끈다
  speakStop(true);        // 말하기가 나오고 있으면 끈다
  digitalWrite(SPK_SD, HIGH);
  chatAudio->setVolume(CHAT_VOLUME);

  otto.stop();            // 움직이던 동작은 멈춘다
  chatHistoryCount = 0;   // 새 대화
  chatOn = true;
  lightChatColor(false); // 대화 시작: 빨간색 (인사말이 끝나고 듣기 시작하면 녹색)
  chatStartTTS("안녕하세요! 저는 오토 로봇입니다. 녹색불이 켜지면 말씀하세요!");
  return true;
}

void chatEnd() {
  if (chatAudio)
    chatAudio->stopSong();
  digitalWrite(SPK_SD, LOW); // 앰프 끄기
  chatOn = false;
  lightChatEnd(); // LED 를 대화 전 상태로 되돌린다
  Serial.println("[AI 대화] 종료");
}

// 라디오를 켜려고 할 때 대화가 아직 끝나는 중이면 지금 바로 끝낸다.
// (끝나는 중에 라디오를 시작하면, 뒤늦게 실행되는 chatEnd() 가 라디오를 멈춰 버린다)
void chatForceStop() {
  chatWant = false;
  if (chatOn) {
    chatEnd();
    chatAck(false, true);
  }
}

// ============================================================
// 대화 한 단계 진행 (loop() 에서 계속 불린다)
// ============================================================
void chatStep() {
  chatAudio->loop(); // TTS 스트리밍은 항상 처리한다

  switch (chatState) {
  case CHAT_RECORDING: {
    size_t wavSize = chatRecordAudio();
    if (wavSize == 0)
      break; // 노이즈이거나 대화가 꺼짐 → 다시 대기 (꺼졌으면 chatLoop 가 정리한다)

    size_t b64Len = chatEncodeBase64(wavSize);

    chatState = CHAT_THINKING;
    bool sttOk = false;
    String userText = chatGeminiSTT(b64Len, sttOk);
    Serial.println("[사용자] " + userText);
    if (!chatWant)
      break;
    if (!sttOk) { // 인식 실패 등은 안내 음성만 들려주고 다시 듣는다
      chatStartTTS(userText);
      break;
    }

    bool llmOk = false;
    String answer = chatGeminiLLM(userText, llmOk);
    Serial.println("[AI 답변] " + answer);
    if (!chatWant)
      break;

    if (llmOk)
      chatPublishLog(userText, answer); // 웹 화면에 대화 기록을 보여준다
    chatStartTTS(answer);
    break;
  }

  case CHAT_SPEAKING: {
    static unsigned long lastAudioChange = 0;
    static uint32_t lastAudioTime = 0;
    static bool tracking = false;

    if (!tracking) { // 방금 재생을 시작했다
      tracking = true;
      lastAudioTime = 0;
      lastAudioChange = millis();
    }

    uint32_t currTime = chatAudio->getAudioCurrentTime();
    if (currTime != lastAudioTime) {
      lastAudioTime = currTime;
      lastAudioChange = millis();
    }
    uint32_t timeoutLimit = (lastAudioTime == 0) ? 5000 : 2000;

    bool finished = !chatAudio->isRunning();
    bool stalled = !finished && (millis() - lastAudioChange > timeoutLimit);
    if (finished || stalled) {
      if (stalled)
        chatAudio->stopSong();
      tracking = false;
      chatState = CHAT_RECORDING; // 바로 다음 질문을 듣는다
      Serial.println("[AI 대화] 다음 질문을 말씀해 주세요...");
    }
    break;
  }

  case CHAT_THINKING:
    // STT/LLM 은 동기 처리라서 이 상태로 돌아오지 않는다
    chatState = CHAT_RECORDING;
    break;
  }
}

// loop() 에서 호출한다. 요청된 상태(chatWant)와 실제 상태(chatOn)를 맞추고 대화를 진행한다.
void chatLoop() {
  // 부팅 후 Wi-Fi 가 연결되면 저장된 키를 한 번 확인한다 (대화 중에는 하지 않는다)
  if (chatKeyCheckPending && !chatOn && !chatWant && WiFi.status() == WL_CONNECTED && millis() > 8000) {
    chatKeyCheckPending = false;
    chatKeyCheck();
  }
  if (chatWant && !chatOn) {
    const char *err = nullptr;
    if (chatBegin(err)) {
      chatAck(true, true);
    } else {
      Serial.printf("[AI 대화] 시작 실패: %s\n", err);
      chatWant = false;
      chatAck(false, false, err);
    }
  } else if (!chatWant && chatOn) {
    chatEnd();
    chatAck(false, true);
  }

  if (chatOn)
    chatStep();
}

// ESP32-audioI2S 콜백
void audio_eof_mp3(const char *info) {
  Serial.println("[TTS] 스트림 재생 완료");
}
void audio_info(const char *info) { Serial.printf("[Audio] %s\n", info); }
