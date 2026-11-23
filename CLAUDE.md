# i2r-server: Claude 작업 안내

이 저장소는 i2r 보드(IoT PLC i2r-02/03/04, Otto)와 같은 i2r 프로토콜로 동작하는 서버 쪽 프로그램입니다.
i2r 전체 안내판(다른 키트, 작업 폴더, i2r 스킬)은 https://raw.githubusercontent.com/kdi6033/i2r/main/AGENTS.md 에 있습니다.
폴더는 `protocol.md`(프로토콜 원본), `sim/`(시뮬레이터), 그리고 연결 방식 세 가지 `standalone/`(보드 혼자, ✅), `local/`(현장 서버, 만드는 중), `aws/`(아마존, 만드는 중)입니다.
사용자가 보드 웹 제어, 스케줄, 입력 규칙, 센서 트리거가 있는 PLC 프로그램을 만들려고 하거나 "업데이트해 줘"라고 하면 10장을 먼저 읽습니다.

사용자는 보통 이렇게 말합니다.
- "https://github.com/kdi6033/i2r 읽고 i2r 개발을 도와줘. 시뮬레이터 켜 줘" (안내판을 거쳐 옴)
- "https://github.com/kdi6033/i2r-server 저장소를 이 폴더에 내려받고, 그 안의 CLAUDE.md를 읽은 다음 시뮬레이터를 켜 줘."
- "시뮬레이터 켜 줘" / "꺼 줘" / "다시 켜 줘" / "프로토콜 시험해 줘" / "최신으로 업데이트해 줘"

사용자는 초보일 수 있습니다. 한국어로, 짧게, 다음에 무엇을 누르면 되는지 알려 주세요.

## 1. 처음 준비

1. 저장소가 없으면 받습니다: `git clone https://github.com/kdi6033/i2r-server`
   (Git이 없으면 https://git-scm.com 설치를 안내합니다.)
2. Python 확인: `python --version` (3.8 이상).
   - 없으면 Windows에서는 `winget install -e --id Python.Python.3.12`로 설치하고, 새 터미널에서 다시 확인합니다.
   - `python`이 Microsoft Store를 여는 경우도 설치가 안 된 것입니다.
3. 라이브러리 확인: `python -c "import paho.mqtt"`
   - 실패하면 `python -m pip install "paho-mqtt>=2.0"`을 실행합니다.

## 2. 서버 고르기 (켜기 전에 꼭 묻기)

켜기 전에 사용자에게 어느 MQTT 서버를 쓸지 묻습니다. AskUserQuestion 같은 선택 질문을 쓰면 좋습니다.

| 번호 | 서버 | 언제 |
|---|---|---|
| 1 | `test.mosquitto.org` | 기본, 교육용 공개 서버 |
| 2 | `broker.emqx.io` | 1번이 느리거나 안 될 때 |
| 3 | 직접 입력 | i2r-05 로컬 서버, 아마존 서버 (주소, 포트, 아이디) |

- 실제 보드와 같이 쓴다면 **보드와 같은 서버**여야 한다고 알려 줍니다. 보드 서버는 I2R_IoT의 `tools/send-si.ps1`로 바꿉니다.
- 목록은 `sim/servers.py`에 있습니다. 목록을 바꾸면 I2R_IoT `tools/send-si.ps1`의 목록도 같이 맞춥니다.

## 3. 켜기

`sim/` 폴더에서 **백그라운드로** 실행하고, 반드시 `--server`를 붙입니다. `--server`가 없으면 시뮬레이터가 번호 입력을 기다립니다.

```
cd i2r-server/sim
python sim.py --server test.mosquitto.org --boards 2
```

- Windows 한글이 깨지면 환경 변수 `PYTHONIOENCODING=utf-8`을 붙여 실행합니다.
- 아이디가 있는 서버(`--user`)는 비밀번호를 터미널에서 묻기 때문에 Claude가 대신 켤 수 없습니다. 사용자에게 `sim/start-sim.bat --server 주소 --user 아이디`를 직접 실행하도록 안내합니다.
- 보드 종류: `--kind i2r-02`, `i2r-03`(기본), `i2r-04`(입출력 8개). 공개 서버에는 가상 보드를 5대까지만 붙입니다.
- 켠 뒤 출력에 `화면: http://localhost:...` 줄이 있는지 확인합니다. `포트를 쓸 수 없습니다`가 나오면 **다른 시뮬레이터가 이미 켜져 있는 것**입니다. 그것은 사용자 것일 수 있으니 끄지 말고 `--http-port 8001`로 다시 켭니다.
- 켜졌는지 확인할 때는 포트만 보지 말고 `curl -s http://localhost:<포트>/api/boards`의 `group`이 방금 켠 그룹과 같은지 봅니다.

켠 뒤 출력에서 다음을 찾아 사용자에게 알려 줍니다.
- 서버와 그룹 (`가상 보드 N대를 ...에 붙입니다. 그룹 sim-xxxx`)
- 보드별 MAC
- 화면 주소 http://localhost:8000 (브라우저가 저절로 열립니다. 안 열리면 주소를 눌러 보라고 합니다.)

화면 사용법도 짧게 알려 줍니다.

| 누르는 곳 | 하는 일 |
|---|---|
| 가상 보드의 IN1 ~ IN4 | 가상 입력 켜기/끄기 |
| 제어 화면의 스위치 토글 | `so` 명령으로 출력 켜기/끄기 |
| 제어 화면의 "입력 N" | 입력 규칙 (`bio`) |
| 제어 화면의 "스위치 N" 이름 | 시간표 (`sch`) |
| 온도, 습도 카드 | 센서 트리거와 보정 (`bs`) |
| 오른쪽 위 "브로커" | MQTT 서버와 그룹 바꾸기 (`POST /api/server`, 아이디가 있는 서버는 안 됨) |
| 보드 목록의 "🔌 USB로 실제 보드 연결" | 브라우저(Web Serial)로 1) 가상보드 프로그램 쓰기(esptool-js, `sim/firmware/`), 2) Wi-Fi·서버 `si` 보내기(비밀번호는 USB로만), 3) 시작 메시지에서 MAC, 서버, 그룹을 읽어 서버를 맞추고 그 보드 고르기. 아래 시리얼 모니터는 포트를 열어 두고 계속 읽음(비밀번호, 이메일은 `***`), 창을 닫거나 멈춤을 누르면 포트를 놓음 |

## 4. 프로토콜 시험

"프로토콜 시험해 줘"라고 하면, 켜져 있는 시뮬레이터의 그룹과 MAC으로 실행합니다. 서버는 시뮬레이터와 같게 합니다.

```
python test_protocol.py --server test.mosquitto.org --group sim-xxxx --mac 02:..:01
```

결과(`결과: N/N 통과`)와 실패한 항목을 표로 보여 줍니다.

실제 보드를 시험할 때는:
- 출력을 켜고 끄므로, **릴레이에 아무것도 연결하지 않은 상태**인지 먼저 확인합니다.
- 보드의 그룹과 MAC은 사용자에게 받습니다(보드 시리얼 출력에 나옵니다).

## 4.1 실제 보드에 프로그램 다운로드 ("연결했어 프로그램 다운로드 해줘")

사용자가 이렇게 말하면 I2R_IoT 예제 `02_PLC_Simulator`를 USB로 연결된 보드에 올립니다. 이 말이 곧 동의이므로 따로 묻지 않고 진행하되, "보드에 있던 프로그램은 지워집니다"를 한 줄로 알립니다.

1. **보드 종류**: 말하지 않았으면 **i2r-03**(기본). "i2r-02로", "i2r-04로"라고 하면 그 보드.
2. **포트 찾기**: Windows는 `Get-PnpDevice -Class Ports -PresentOnly`에서 `CH340`이 들어간 COM 포트. 없으면 USB 케이블(충전 전용이 아닌지)과 CH340 드라이버를 안내하고 멈춥니다. 여러 개면 사용자에게 묻습니다.
3. **arduino-cli**: PATH에 없으면 Arduino IDE 안의 것(`C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe`)을 씁니다. 둘 다 없으면 `winget install -e --id ArduinoSA.CLI`로 설치합니다.
4. **보드 패키지와 라이브러리** (없을 때만 설치):
   ```
   arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
   arduino-cli lib install I2R_PLC PubSubClient ArduinoJson
   ```
   **I2R_IoT는 아직 라이브러리 매니저에 등록 중**이라 `lib install I2R_IoT`가 안 됩니다. 먼저 `arduino-cli lib list`에서 I2R_IoT가 0.2.2 이상인지 보고, 없거나 낮을 때만 GitHub 태그로 설치합니다.
   ```
   arduino-cli config set library.enable_unsafe_install true
   arduino-cli lib install --git-url https://github.com/kdi6033/I2R_IoT.git#0.2.2
   arduino-cli config set library.enable_unsafe_install false
   ```
   - 설치 뒤에는 설정을 다시 `false`로 돌려 놓습니다 (다른 주소의 라이브러리가 몰래 설치되지 않게).
   - I2R_IoT 폴더가 링크(라이브러리를 직접 고치는 PC)이면 다시 설치하지 않습니다.
   - 등록이 끝나면(`arduino-cli lib search I2R_IoT`에 나오면) 이 단계는 `lib install I2R_IoT` 한 줄로 바꿉니다.
5. **스케치**: 라이브러리 폴더의 `I2R_IoT/examples/02_PLC_Simulator` (I2R_IoT 0.2.x에는 예전 이름 `02_SimCheck`로 들어 있음). i2r-03이면 그대로 씁니다. 다른 보드면 **임시 폴더로 복사**한 뒤 `I2R_PLC plc(I2R_03);` 한 줄만 `I2R_02` 또는 `I2R_04`로 바꿉니다. 라이브러리 원본은 고치지 않습니다.
6. **컴파일과 업로드**:
   ```
   arduino-cli compile --fqbn esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app <스케치>
   arduino-cli upload -p <COM> --fqbn esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app <스케치>
   ```
   포트를 쓸 수 없다고 나오면 시리얼 모니터(Arduino IDE, i2r.link 웹)를 닫아 달라고 합니다.
7. **확인**: 시리얼(115200)을 10초쯤 읽어 `==== I2R_IoT 02_PLC_Simulator ====`(예전 이름 `02_SimCheck`도 같은 프로그램), 보드 종류, MAC을 확인합니다 (PowerShell `System.IO.Ports.SerialPort`, RTS로 리셋). 읽은 뒤 포트를 반드시 닫습니다.
   - 시리얼 출력에 Wi-Fi 비밀번호 같은 값이 보이면(예전 펌웨어) **옮겨 적지 않습니다.**
8. **다음 안내**: Wi-Fi와 MQTT 서버는 **사용자가 직접** 넣게 합니다 (README "실제 보드 연결하기" 방법 B 2번: `send-si.ps1` 또는 i2r.link 웹. 웹에서는 Email 비우고 Broker에 시뮬레이터와 같은 서버). 비밀번호를 채팅으로 받지 않습니다.
9. 사용자가 "설정 보냈어"라고 하면 시리얼에서 서버, 그룹(따로 정하지 않았으면 MAC에서 `:`를 뺀 소문자)을 확인하고, 같은 서버와 `--group <보드의 그룹>`으로 시뮬레이터를 켜고(이미 켜져 있으면 그것을 끄고 다시 켤지 묻기), 4장대로 시험합니다.
   - `02_PLC_Simulator`에는 아직 `bio`, `sch`, `bs`가 없어 그 시험은 실패가 정상이라고 함께 알려 줍니다.
   - 실제 보드의 MAC과 그룹은 대화에서만 쓰고 파일에 적지 않습니다.

## 5. 끄기, 다시 켜기

- 끄기: 백그라운드로 켠 작업을 멈춥니다. 사용자가 직접 켠 창이면 그 창에서 Ctrl+C 또는 창 닫기를 안내합니다.
- 권한 문제로 끄지 못하면 작업 관리자에서 python을 끝내도록 안내합니다.
- `board.py`, `sim.py`, `servers.py`를 고친 뒤에는 **다시 켜야** 바뀐 내용이 적용됩니다. `web/index.html`은 브라우저 새로고침만 하면 됩니다.

## 6. 최신으로 업데이트

`git pull --ff-only`을 하고, 시뮬레이터가 켜져 있었다면 다시 켭니다. 새로 들어온 것을 3줄 이내로 알립니다.
standalone을 쓰는 사용자(보드, `my_sketches/`)라면 10.4장 순서대로 보드와 사용자 프로그램까지 맞춥니다.

## 7. 안전 (꼭 지키기)

- 공개 서버는 누구나 토픽을 보고 명령을 보낼 수 있습니다.
- **실제 보드의 MAC과 그룹을 파일에 적거나 커밋하지 않습니다.** 예시에는 `AA:BB:CC:DD:EE:FF`, `sim-xxxx`를 씁니다.
- 메시지에 개인 정보나 비밀번호를 넣지 않습니다.
- 사용자가 요청하지 않으면 코드를 고치지 않습니다.

## 8. 파일

| 파일 | 내용 |
|---|---|
| `sim/sim.py` | 시뮬레이터 실행, 웹 화면(HTTP API) |
| `sim/board.py` | 가상 보드 (실제 펌웨어와 같은 메시지) |
| `sim/servers.py` | MQTT 서버 목록과 고르기 메뉴 |
| `protocol.md` | i2r 통신 프로토콜 원본 (9장) |
| `sim/test_protocol.py` | 프로토콜 규칙 시험 |
| `sim/web/index.html` | 제어 화면 |
| `sim/start-sim.bat` | Windows에서 두 번 눌러 켜기 |
| `sim/README.md` | 옵션과 메시지 형식 |
| `standalone/` | ① 혼자 동작: 보드가 웹서버 (입력 규칙, 스케줄, 센서 트리거, 이름, NTP). 보드별 폴더 `i2r-02/03/04`. 10장 |
| `standalone/README.md` | standalone 설명 (쓰는 법, 화면, 동작 방식, 웹 API, 컴파일) |
| `standalone/CLAUDE.md` | 학생 요구를 분석해 standalone으로 프로그램 만드는 순서, 코드 지도 |
| `sim/firmware/` | 가상보드 프로그램 .bin (I2R_IoT 02_PLC_Simulator, i2r-02/03/04)과 `manifest.json`. I2R_IoT, I2R_PLC가 바뀌면 `python sim/firmware/build_firmware.py`로 다시 만들고 커밋 |

## 9. 프로토콜 (`protocol.md`)

- i2r 통신 프로토콜의 **원본은 이 저장소의 [protocol.md](protocol.md)** 입니다. 다른 저장소는 이 파일을 가리키기만 합니다.
- 프로토콜을 바꿀 때는 **`protocol.md`, `sim/board.py`, `sim/test_protocol.py`를 한 커밋에서 함께** 고치고, 시뮬레이터로 `test_protocol.py`가 모두 통과하는지 확인한 뒤 올립니다.
- 필드와 명령은 더하기만 합니다. 지우거나 뜻을 바꾸면 `pv`를 올리고 protocol.md "바뀐 내역"에 적습니다.
- 사용자 스케치는 JSON을 직접 만들지 않고 라이브러리 함수를 씁니다 (protocol.md 0.1).


## 10. standalone: 보드 혼자 동작 (`standalone/`)

i2r PLC(i2r-02/03/04)가 웹서버가 되어 같은 공유기의 휴대폰, PC에서 `http://i2r.local`로 제어하는 펌웨어입니다. 사람이 읽는 설명은 [standalone/README.md](standalone/README.md), Claude 작업 순서와 코드 지도는 [standalone/CLAUDE.md](standalone/CLAUDE.md)입니다.

이 저장소의 i2r 연결 방식은 세 가지이고, 폴더 이름이 곧 그룹 이름입니다. 각 그룹 안에서 보드마다 폴더를 나눕니다 (`standalone/i2r-02/`, `i2r-03/`, `i2r-04/`. 소스는 `#define I2R_BOARD` 한 줄만 다름).

| 그룹 | 폴더 | 구성 | 상태 |
|---|---|---|---|
| ① 혼자 동작 | `standalone/` | 보드가 웹서버. 서버, 인터넷 없이 동작 | ✅ `standalone-0.1.0` |
| ② 현장 서버 | `local/` | PLC 여러 대 + i2r-05(MQTT 브로커, 백엔드, 대시보드) | 만드는 중 |
| ③ 아마존 | `aws/` | 아마존(EC2) 개인 서버와 연결 | 만드는 중 |

사용자는 보통 이렇게 말합니다.
- "i2r-03을 휴대폰으로 제어하고 싶어" / "standalone 올려 줘" → 그 보드 폴더를 그대로 올립니다.
- "standalone 참고해서 ~하는 프로그램 만들어 줘", 학생이 PLC 프로그램을 요구할 때 → **`standalone/CLAUDE.md`를 먼저 읽고** 그대로 따릅니다 (요구 분석, 코드 없이 되는지, 코드 지도, 자주 나오는 요구, 확인).
- "업데이트해 줘", "새 버전 있어?" → 10.4장.

### 10.1 일하는 순서

1. **먼저 읽기**: `standalone/CLAUDE.md`, `standalone/README.md`, 그다음 보드 폴더의 `.ino` 맨 위 설명. 웹 화면은 `page.h`(HTML, JS 한 파일), 로고는 `logo.h`입니다.
2. **보드 확인**: 말하지 않았으면 묻고 그 보드 폴더를 씁니다. i2r-02/04는 온습도 센서가 없어 화면에서 온도·습도 카드가 숨겨집니다.
3. **사용자 프로그램은 복사본에서**: 예제를 고쳐 달라는 말이 없으면 `standalone/`을 고치지 않습니다. 보드 폴더를 사용자 작업 폴더(예: `my_sketches/<영문_이름>/`)로 통째로 복사하고, `.ino` 이름을 폴더 이름에 맞춘 뒤 고칩니다.
   예제 자체를 고칠 때는 세 보드 폴더를 함께 고칩니다.
4. **컴파일**: 라이브러리 I2R_PLC, I2R_IoT, ArduinoJson, PubSubClient (설치는 4.1장 4번과 같음). 성공할 때까지 스스로 고칩니다.
   ```
   arduino-cli compile --fqbn esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app <스케치 폴더>
   ```
5. **업로드 전**: 릴레이에 무엇이 연결되어 있는지, 켜져도 괜찮은지 묻습니다. 저장된 스케줄이 켜자마자 출력을 켤 수 있습니다.
6. **Wi-Fi**: 사용자가 **직접** 넣게 합니다. 비밀번호를 채팅으로 받지 않고 파일에 옮겨 적지 않습니다.
   - https://kdi.i2r.link/firmware 의 i2r_server 칸 **Wi-Fi 정보 보내기**, 또는 기기연결 **COM 포트(USB)**. 보드는 SSID, PW만 저장하고 `{"c":"si","ok":true}`로 답한 뒤 다시 시작합니다.
   - 또는 사용자가 `secrets.example.h`를 `secrets.h`로 복사해 직접 적습니다 (`.gitignore`에 있음).
7. **확인**: 시리얼(115200)에서 `==== i2r-server standalone ...`, `[Wi-Fi] 연결됨`, `웹 주소: http://...`를 봅니다. 같은 Wi-Fi의 PC면 `curl http://i2r.local/state`. 휴대폰이 adb로 연결되어 있으면 `adb shell am start -a android.intent.action.VIEW -d http://<IP>/`로 열고 `adb exec-out screencap -p`로 화면을 확인할 수 있습니다.

### 10.2 standalone의 구조 (고칠 때 알아 둘 것)

| 부분 | 내용 |
|---|---|
| 웹 API | `GET /` 화면, `GET /state` 상태(1초마다), `POST /cmd` 명령 JSON, `GET /logo.png` |
| 명령 | protocol.md의 `so`, `bio`, `sch`, `bs`, `st`, `gs`와 같은 모양. 다른 점: 목록은 `items` 배열 하나로 답하고, insert/delete 뒤에도 목록을 돌려줌. `name`은 이 펌웨어만 |
| 시리얼 | 같은 JSON 한 줄을 받아 같은 답. `si`(Wi-Fi 설정)는 **USB에서만** 받음 (웹 `/cmd`로 온 si는 unknown) |
| 저장 (NVS) | `i2rweb`: 규칙, 스케줄, 센서 트리거, 보정, 이름 / `i2riot`: Wi-Fi (I2R_IoT와 같은 자리) / `i2r`: 시간대, 저장된 시각 (I2R_Time) |
| 시간 | I2R_Time + 1시간마다 NTP 다시 요청, 10분마다 시각 저장. 인터넷이 없어도 규칙, 스케줄은 보드 시계로 계속 |
| 번호 | 사람은 1부터(OUT1), JSON은 0부터(`"n":0`) |

### 10.3 배포용 펌웨어 만들기 (관리자가 요청할 때만)

- 배포용은 `-DI2R_RELEASE`를 넣어 `secrets.h`를 읽지 않게 만듭니다. `--output-dir`의 `.ino.merged.bin`(8MB)을 `i2r-0X.ino.merged.bin`으로 바꿔 씁니다. 보드마다 `--build-path`를 따로 줍니다 (같이 만들면 빌드 폴더가 겹쳐 실패할 수 있음).
  ```
  arduino-cli compile --fqbn esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app --build-property "compiler.cpp.extra_flags=-DI2R_RELEASE" --build-path <보드마다 다른 임시 폴더> --output-dir <임시 폴더> standalone/i2r-03
  ```
- 배포 위치는 `kdi6033/download`의 `i2r_server/standalone/i2r-0X.ino.merged.bin`과 `i2r_server/versions.json`입니다 (`i2r_server/local/`, `i2r_server/aws/`도 같은 규칙. 폴더 이름은 밑줄). https://kdi.i2r.link/firmware 가 이 폴더를 읽어 카드로 보여 줍니다. 올리는 일(push)은 공개되므로 반드시 먼저 묻습니다.
- 고친 뒤에는 세 `.ino`의 `VERSION`(`standalone-x.y.z`)과 `i2r_server/versions.json`, `standalone/README.md`의 버전을 함께 올립니다.
- 공개하기 전에 `secrets.h`, `build/` 폴더, 실제 Wi-Fi 이름·비밀번호, 실제 MAC, IP가 들어가지 않았는지 확인합니다.

### 10.4 사용자가 "업데이트해 줘"라고 하면

사용자의 보드와 사용자가 만든 프로그램을 최신으로 맞춰 주는 일입니다. 순서대로 하고, 단계마다 한 줄로 알립니다.

1. **저장소 받기**: 이 저장소 폴더에서 `git pull --ff-only`. 사용자가 고친 파일 때문에 받지 못하면 **지우거나 되돌리지 말고** 어떤 파일인지 알리고, 원하면 `my_sketches/`로 옮긴 뒤 다시 받습니다.
2. **새로 들어온 것 알리기**: `git log --oneline <이전>..<새>`로 3줄 이내. standalone이 바뀌었으면 `standalone/README.md`의 버전과 바뀐 기능을 함께 알립니다.
3. **보드 버전 확인**: 보드가 연결되어 있으면 시리얼 첫 줄 `버전 standalone-x.y.z` 또는 `curl http://i2r.local/state`의 `ver`를 읽어 저장소의 `VERSION`과 비교합니다.
4. **보드 업데이트** (보드가 예전 버전일 때, 사용자에게 먼저 묻기)
   - 보드 설정(Wi-Fi, 규칙, 스케줄, 이름)을 **지키면서** 바꾸려면 `arduino-cli upload`(일반 업로드) 또는 펌웨어 페이지의 **앱만 설치**를 씁니다. 전체 설치는 설정을 모두 지웁니다.
   - 업로드 전에 릴레이 안전을 묻습니다 (10.1장 5번).
5. **사용자 프로그램(`my_sketches/`)**: 예제를 복사해 고친 프로그램은 자동으로 바뀌지 않습니다. 새 예제에서 바뀐 부분(`git diff <이전>..<새> -- standalone/i2r-0X/`)을 보여 주고, 원하면 사용자 변경을 지키면서 옮겨 넣은 뒤 컴파일, 업로드합니다.
6. **확인**: 업로드했으면 시리얼이나 `/state`의 `ver`가 새 버전인지 확인합니다.

## 안내판을 거쳐 왔다면 (처음 한 번 끝날 때)

사용자가 "https://github.com/kdi6033/i2r 읽고 ..."로 시작했거나 작업 폴더 맨 위에 이 키트 폴더가 있다면, 환경 만들기가 끝날 때 아래 두 가지를 **빠뜨리지 말고** 합니다. 자세한 내용은 안내판 원문(`curl -sL https://raw.githubusercontent.com/kdi6033/i2r/main/AGENTS.md`) 4.3장과 6장입니다.

1. 작업 폴더 맨 위에 `CLAUDE.md`(작업 폴더 안내)가 없으면 만듭니다. 받은 키트 폴더와 "먼저 읽을 파일"을 표로 적습니다.
2. "다음부터 주소 없이 말할 수 있게 이 PC에 i2r 스킬을 설치할까요?"라고 묻고, 좋다고 하면 `https://raw.githubusercontent.com/kdi6033/i2r/main/skills/i2r/SKILL.md`를 사용자 폴더의 `.claude/skills/i2r/SKILL.md`로 받습니다.
