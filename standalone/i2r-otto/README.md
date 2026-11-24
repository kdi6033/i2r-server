# i2r-otto: Otto 로봇 혼자 동작 (보드가 웹서버)

Otto I2R 로봇을 **서버 없이** 같은 공유기의 휴대폰, PC에서 조종하는 펌웨어입니다.
로봇이 직접 웹페이지를 띄우므로, 브라우저에서 `http://otto.local`을 열면 i2r.link의 로봇 화면과 같은 화면이 나옵니다.

```
 휴대폰, PC (같은 Wi-Fi) ── http://otto.local ──▶ Otto (웹서버 + 서보, 스피커, 마이크, LED, OLED)
                                                    └─ AI 대화, 라디오, 말하기는 공유기의 인터넷을 씀
```

| 버전 | `standalone-0.1.0` |
|---|---|
| 보드 | Otto I2R (ESP32-S3, 16MB flash, 8MB PSRAM) |
| 주소 | `http://otto.local` 또는 시리얼 모니터, OLED에 나온 `http://192.168.x.x` |

## 1. 화면에서 하는 일

| 버튼 | 하는 일 | 인터넷 |
|---|---|:---:|
| 걷기 / 굴리기 / 미끄러짐 | 조종판의 전진, 후진, 좌회전, 우회전. 정지할 때까지 계속 움직임. 전진·후진은 가운데에 가까울수록 느리게(40), 바깥일수록 빠르게(200) | |
| 댄스 | 정지할 때까지 춤. "댄스할 때 내 음악 재생"을 켜면 저장한 곡도 함께 | |
| 순찰 | 거리 센서로 장애물을 피하며 굴러다님 (LED 데모도 켬) | |
| AI 대화 | 로봇 마이크로 듣고 스피커로 대답 (Gemini API 키를 화면에서 저장) | ✅ |
| 라디오 | 인터넷 라디오 6개 방송국, 볼륨 | ✅ |
| 말하기 | 적은 글(80자)을 정한 간격, 횟수로 읽기 | ✅ |
| 받아쓰기 | 내 말을 브라우저가 글자로 바꿔 보여 줌. 로봇 LED 녹색 = 말하기, 빨간색 = 바꾸는 중 (아래 주의 참고) | ✅ |
| 시계 | OLED에 큰 시계 | |
| 책읽기 | 이솝우화 4편 또는 직접 넣은 글을 70자씩 나눠 끝까지 읽기 | ✅ |
| 예약 | 정한 시각, 요일에 조명, 라디오, 내 음악을 켜고 끄기 (8개) | |
| 내 음악 | mp3를 화면에서 보드에 저장(6곡), 재생, 삭제, 볼륨 | |
| 조명, LED 데모 | LED 16개 색과 밝기, 무지개·점멸·흐름 데모 | |
| OTTO 데모 | 걷기 → 미끄러짐 → 굴리기 → 댄스 반복 | |
| 로봇 맞추기 | 조립 자세, 다리 수직, 굴리기 속도, 걷기 기울기, 댄스 각도를 맞추고 보드에 저장 | |

- 화면을 열 때 로봇 시계가 맞지 않았으면 휴대폰 시각을 보내 맞춥니다. 인터넷이 없어도 시계와 예약이 돕니다.
- 화면이 3초 넘게 말이 없으면(휴대폰 화면 꺼짐, Wi-Fi 끊김) 화면에서 시작한 동작을 로봇이 스스로 멈춥니다.

## 2. Wi-Fi 넣기

둘 중 하나입니다. 저장 자리는 I2R_IoT와 같은 NVS `i2riot`이라, 나중에 ② local, ③ aws 펌웨어로 바꿔도 Wi-Fi가 이어집니다.

1. **USB**: i2r.link → 연결 → COM 포트에서 Wi-Fi 이름, 비밀번호를 보냅니다. (Email, Broker 칸은 쓰지 않음)
   시리얼 모니터(115200, 새 줄)에 한 줄을 넣어도 됩니다: `{"c":"si","ssid":"공유기","password":"비밀번호"}`
2. **개발용**: `secrets.example.h`를 `secrets.h`로 복사해 적습니다 (GitHub에 올라가지 않음).

## 3. 소스

| 파일 | 내용 |
|---|---|
| `i2r-otto.ino` | Wi-Fi, 웹서버, 명령 나누기, setup/loop |
| `page.h` | 웹 화면 (HTML, JavaScript 한 파일) |
| `motion.h` | Otto 동작과 보정 (OttoI2R 라이브러리) |
| `chat.ino` `radio.ino` `speak.ino` `music.ino` `light.ino` `eyes.ino` `clock.ino` `schedule.ino` `patrol.ino` | AI 대화, 라디오, 말하기, 내 음악, 조명, OLED 눈, 시계, 예약, 순찰 |
| `partitions.csv` | 16MB: 앱 4MB + 내 음악 저장(SPIFFS) |

## 4. 웹 API

명령은 [protocol.md](../../protocol.md) 10장(Otto, 초안)과 같은 JSON입니다.

| 주소 | 내용 |
|---|---|
| `GET /` | 화면 |
| `GET /info` | `{"m":MAC,"t":"i2r-otto","ver","ip","host"}` |
| `POST /cmd` | 명령 JSON 한 줄 (본문 `text/plain`). 답 `{"ok":1}` 또는 `{"ok":0,"err":".."}` |
| `GET /ev?s=번호` | 번호부터의 새 알림(보드의 답, 라디오 제목, 대화 기록, 말하기 끝 등)과 지금 상태 `{"s","a","tm","ts","ev":[..]}` |
| `POST /upload?sz=바이트&tx=제목` | 내 음악 mp3 저장 (multipart 파일 하나) |

명령 예: `{"c":"ot","a":"wf","du":0,"sp":100}` 걷기 전진 계속, `{"c":"ot","a":"stop"}` 정지, `{"c":"ol","ef":"sd","co":"00ff00","br":120}` 조명 녹색.
동작 코드: 걷기 `wf wb wl wr`, 굴리기 `rf rb rl rr`, 미끄러짐 `sf sb sl sr`, 댄스 `dn`, 순찰 `pt`, OTTO 데모 `od`.
시리얼 모니터에 같은 JSON을 넣어도 됩니다 (답은 시리얼로).

## 5. 컴파일, 올리기

라이브러리: OttoI2R, I2R_IoT, ArduinoJson 7, PubSubClient, ESP32-audioI2S(schreibfaul1), U8g2, FastLED, Adafruit_VL53L0X

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi standalone/i2r-otto
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,UploadSpeed=115200 standalone/i2r-otto
```

- USB 칩(CH340, COM 포트)으로 연결하면 위처럼 쓰고, 보드 자체 USB로 연결하면 `CDCOnBoot=cdc`를 더합니다. 시리얼이 안 보이면 이것을 바꿔 봅니다.
- 921600 속도에서 업로드가 0%에서 멈추면 `UploadSpeed=115200`을 씁니다.
- 처음 올리면 내 음악 저장 자리를 포맷하느라 1~2분 걸립니다.
- 올리기 전에 로봇을 바닥에 놓고 배터리를 켭니다. 걷다가 멈추면 USB 전원이 모자란 것입니다.

## 6. 주의

- 같은 공유기에 붙은 사람은 누구나 이 화면으로 로봇을 움직일 수 있습니다. 공용 Wi-Fi에서 쓰지 않습니다.
- 같은 공유기에 Otto가 여러 대면 `otto.local` 이름이 겹칩니다. 그때는 IP 주소로 엽니다. 안드로이드는 `.local`이 안 될 수 있습니다.
- **받아쓰기**: 브라우저는 `http://` 주소에서 마이크를 막을 수 있습니다. 막히면 화면에 나오는 안내대로 PC Chrome의 `chrome://flags/#unsafely-treat-insecure-origin-as-secure`에 로봇 주소를 넣습니다.
- 공장 초기화: SW3를 짧게 누르면 Wi-Fi와 API 키, 3초 이상 누르면 내 음악까지 지웁니다.
