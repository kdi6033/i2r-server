# standalone 예제로 프로그램 만들기 (Claude 작업 안내)

사람이 읽는 설명은 [README.md](README.md)에 있습니다. 이 파일은 Claude가 따르는 작업 순서입니다.

학생이나 사용자가 "~하는 PLC 프로그램 만들어 줘"라고 하면, 이 예제를 바탕으로 요구를 분석해 프로그램을 만듭니다.
이 폴더의 `i2r-02/`, `i2r-03/`, `i2r-04/`는 같은 프로그램이고 `#define I2R_BOARD` 한 줄만 다릅니다.

사용자는 초보 학생일 수 있습니다. 한국어로, 짧게, 다음에 무엇을 하면 되는지 알려 주세요.

## 1. 요구 분석 (코드를 쓰기 전에)

1. **보드 확인**: 말하지 않았으면 i2r-02, i2r-03, i2r-04 중 무엇인지 묻습니다.
   - i2r-02: 입력 4(무전압 접점), 출력 4
   - i2r-03: 입력 4(전압 입력), 출력 4, 온습도 센서(AHT21)
   - i2r-04: 입력 8(무전압 접점), 출력 8
2. **코드 없이 되는지 먼저 봅니다.** 이 예제의 화면만으로 되는 요구가 많습니다. 되면 화면에서 하는 법만 알려 주고, 코드를 고치지 않습니다.

   | 요구 예 | 화면에서 하는 법 |
   |---|---|
   | "IN1이 켜지면 3초 뒤 OUT2를 10초 켜기" | 입력 1 이름 누름 → 입력 규칙 (트리거 켜짐, 포트 출력 2, 켜짐, 지연 3, 동작시간 10) |
   | "매일 저녁 6시~10시 전등 켜기", "평일만" | 출력 이름 누름 → 스케줄 (매일 / 매주 + 요일) |
   | "온도 30도 넘으면 팬 켜기, 25도 아래면 끄기" | 온도 카드 → 센서 트리거 두 개 (⬆️ 30 켜짐, ⬇️ 25 꺼짐) |
   | "휴대폰으로 켜고 끄기", "이름을 전등으로" | 스위치, ⚙ 이름 변경 |

3. **코드가 필요하면** 무엇을 할지 2~3줄로 말하고 진행합니다. 예: "OUT1을 1초마다 깜박이는 버튼을 화면에 더하고, 보드에 blink 명령을 만들겠습니다."
   - 요구가 애매하면(몇 초? 어느 출력? 끌 때는?) 한 번에 모아서 묻습니다.
   - 위험한 요구(220V 장치를 빠르게 켰다 끄기, 모터 정역 동시 켜기 등)는 이유를 말하고 안전한 방법을 제안합니다.

## 2. 작업 폴더 (예제 원본은 고치지 않음)

- 예제를 고쳐 달라는 말이 없으면 이 폴더(`standalone/`)는 고치지 않습니다.
- 보드 폴더를 통째로 사용자 작업 폴더로 복사합니다: `my_sketches/<영문_이름>/` (이름은 영문 소문자, 숫자, `_`).
- 폴더 안의 `i2r-0X.ino` 이름을 폴더 이름에 맞춰 `<영문_이름>.ino`로 바꿉니다 (Arduino 규칙).
- `VERSION`을 바꿔 구별합니다: `"standalone-0.1.0"` → `"<영문_이름>-0.1.0"`.

## 3. 코드 지도: 무엇을 어디서 고치나

### 보드 프로그램 (`.ino`)

| 부분 | 함수, 이름 | 고칠 때 |
|---|---|---|
| 보드, 저장 자리 | `I2R_BOARD`, `NVS_NS="i2rweb"`, `MAX_RULES=40` | 보드는 폴더마다 정해져 있음 |
| 저장 | `saveBio/saveSch/saveBs/saveCal/saveNames`, `loadAll()` | 새 설정을 저장하려면 `Preferences`로 같은 `i2rweb`에 키를 더하고 `loadAll()`에서 읽음 |
| 입력 규칙 | `BioRule`, `bioUpdate()` | 입력이 바뀌는 순간 `plc.rising(n)`/`plc.falling(n)` |
| 스케줄 | `SchSlot`, `schUpdate()` (0.5초마다, 시간을 알 때만) | 시각은 `itime.local(t)`, `itime.valid()` |
| 센서 트리거 | `BsRule`, `bsUpdate()`, 보정 `cal[]`, `sensorValue(ts)` | 센서 값은 보정 포함 `sensorValue(0)` 온도, `(1)` 습도 |
| 출력 실행 | `runTarget(n, v, du)` → `plc.output(n+1, v, du)` | **JSON은 0부터, I2R_PLC는 1부터** |
| 상태 (화면이 1초마다 읽음) | `fillState()` | 화면에 새 값을 보이려면 여기에 `s["키"]=값` 추가 |
| 명령 처리 | `handleCmd()` → `cmdSo/cmdSt/cmdBio/cmdSch/cmdBs/cmdName` | 새 명령은 `cmdXxx()`를 만들고 `handleCmd()`에 한 줄. 잘못된 값은 `setErr(res, "xxx", "bad_v")` |
| 웹 주소 | `setupWeb()`: `GET /`, `GET /state`, `POST /cmd`, `GET /logo.png` | 새 주소보다 `POST /cmd`의 새 명령을 권함 |
| 시간 | `timeBegin()`, `timeUpdate()` (1시간마다 NTP, 10분마다 저장) | 인터넷이 없어도 보드 시계로 계속 |
| Wi-Fi | `loadWifi()`, `applySi()` (USB `si`: SSID, PW만 저장), `wifiUpdate()` | 비밀번호를 코드에 적지 않음 (`secrets.h`) |
| 반복 | `loop()`: `plc.update` → `timeUpdate` → `bioUpdate` → `schUpdate` → `bsUpdate` → 웹, 시리얼, LED | **`delay()`를 쓰지 않습니다.** 시간은 `millis()`로 잼 (웹이 멈춤) |

I2R_PLC 함수: `plc.input(n)`, `plc.rising(n)`, `plc.falling(n)`, `plc.output(n, ON, 초)`, `plc.output(n, ON, 지연초, 동작초)`, `plc.toggle(n)`, `plc.blink(n, 켜짐ms, 꺼짐ms)`, `plc.allOff()`, `plc.onDelay(n, ms)`, `plc.offDelay(n, ms)`, `plc.count(n)`, `plc.temperature()`, `plc.humidity()`, `plc.sensorOk()`. 자세히는 https://github.com/kdi6033/I2R_PLC

### 웹 화면 (`page.h`, HTML·CSS·JS 한 파일)

| 부분 | 함수 | 고칠 때 |
|---|---|---|
| 보드와 통신 | `api({c:..})` → `POST /cmd`, `poll()` → `GET /state` 1초마다 | 새 명령은 `api({c:'xxx', m:S.m, ...})` |
| 처음 그리기 | `build()` | 새 카드, 버튼은 HTML에 넣거나 여기서 만듦 |
| 매초 갱신 | `render()` | `fillState()`에 더한 값 `S.키`를 표시 |
| 대화창 | `openBio/openSch/openBs`, `openM/closeM`, `table()` | 새 대화창은 같은 모양(`.ov` > `.modal`)으로 |
| 이름 | `inName(i)`, `outName(i)` | 화면 글자는 사람 번호(1부터) |

- 화면 글자는 한국어로 씁니다. 휴대폰(폭 400px 안팎)에서 겹치지 않게 합니다.
- 오류 코드의 한국어 설명은 `ERR` 표에 더합니다.

## 4. 자주 나오는 요구와 만드는 법

| 요구 | 만드는 법 |
|---|---|
| 화면에 "모두 끄기" 버튼 | `page.h`에 버튼 → `api({c:'off', m:S.m})`, `.ino`에 `cmdOff()`: `plc.allOff(); fillState(res);` |
| 출력 깜박이기 | `cmdBlink()`: `plc.blink(n+1, onMs, offMs)`, 화면에 버튼 |
| 입력 횟수 세기(카운터) 표시 | `fillState()`에 `s["count"]` 배열 (`plc.count(n)`), `render()`에 표시, 초기화 명령 |
| 온도 기록 그래프 | `.ino`에 최근 값 배열(링 버퍼, 예: 5분마다 288개=하루)과 `GET /log` 대신 명령 `{"c":"log"}`, 화면은 `<canvas>`로 그리기 (외부 라이브러리 없이. 인터넷 없이도 보여야 함) |
| 입력 두 개 조건(AND/OR) | 화면 규칙으로 안 되면 `loop()`에 함수를 더함: `if (plc.input(1) && plc.input(2)) ...` (켜는 순간만 실행하도록 이전 값 기억) |
| 암호로 화면 잠그기 | `POST /cmd`에서 비밀번호 확인. 비밀번호는 `secrets.h`나 화면에서 정해 NVS에 저장. HTTP라 같은 Wi-Fi에서는 엿볼 수 있다고 알려 줌 |
| 보드 여러 대 | 보드마다 `HOST_NAME`을 다르게 (`i2r-kitchen` → `http://i2r-kitchen.local`). 여러 대를 한 화면에서 묶으려면 ② `local/`(MQTT) 그룹을 안내 |

## 5. 컴파일, 업로드, 확인

1. **컴파일** (성공할 때까지 스스로 고칩니다. 사용자에게 오류를 고쳐 달라고 하지 않습니다)
   ```
   arduino-cli compile --fqbn esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app my_sketches/<영문_이름>
   ```
   라이브러리: I2R_PLC, I2R_IoT, ArduinoJson, PubSubClient. 웹 JS를 고쳤으면 `<script>` 부분을 떼어 `node --check`로 문법을 봅니다.
2. **업로드 전**: "릴레이 출력에 무엇이 연결되어 있나요? 켜져도 괜찮은가요?"라고 묻습니다. 저장된 스케줄, 규칙이 켜자마자 동작할 수 있습니다.
   ```
   arduino-cli upload -p <COM> --fqbn esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app my_sketches/<영문_이름>
   ```
   일반 업로드는 보드에 저장된 Wi-Fi, 규칙, 이름을 지우지 않습니다. 포트를 쓸 수 없으면 시리얼 모니터를 닫아 달라고 합니다.
3. **Wi-Fi가 없으면** 사용자가 직접 넣게 합니다: https://kdi.i2r.link/firmware 의 i2r_server 칸 "Wi-Fi 정보 보내기", 또는 기기연결(COM 포트). 비밀번호를 채팅으로 받지 않습니다.
4. **확인**
   - 시리얼(115200): `==== i2r-server standalone ...`, `[Wi-Fi] 연결됨`, `웹 주소: http://...`
   - 시리얼에 명령 JSON 한 줄을 보내 시험할 수 있습니다: `{"c":"gs"}`, `{"c":"so","n":0,"v":1}`
   - 같은 Wi-Fi의 PC에서 `curl http://i2r.local/state`, 휴대폰이 adb로 연결되어 있으면 `adb shell am start -a android.intent.action.VIEW -d http://<IP>/` 후 `adb exec-out screencap -p`로 화면 확인
5. **결과 묻기**: 어떤 릴레이가 딸깍거렸는지, 화면이 어떻게 바뀌었는지 구체적으로 묻고 고칩니다.
   입력 시험은 i2r-03이면 "IN1 단자에 전압을 넣어 보세요", i2r-02/04이면 "IN1 접점을 연결해 보세요"라고 합니다. **i2r-02/04 입력에는 전압을 넣으라고 하지 않습니다** (보드 고장).
6. **정리**: 무엇을 만들었는지 3줄 이내로 말하고, 다음에 해 볼 만한 것 하나를 제안합니다.

## 6. 지킬 것

- 번호: 사람(화면, 설명, `plc.output(1)`)은 1부터, JSON(`"n":0`)과 배열은 0부터.
- `loop()`를 막지 않습니다 (`delay()`, 오래 걸리는 `while` 금지). 웹과 규칙이 함께 멈춥니다.
- 비밀번호, 실제 MAC, IP를 코드나 GitHub에 올리는 파일에 적지 않습니다.
- 같은 Wi-Fi의 누구나 이 화면으로 릴레이를 켤 수 있다는 점을 처음 한 번 알려 줍니다.
- 프로토콜(`../protocol.md`)에 있는 명령(`so`, `bio`, `sch`, `bs`, `st`, `gs`)의 모양은 바꾸지 않습니다. 새 기능은 새 명령 이름으로 더합니다.
