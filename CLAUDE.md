# i2r-server: Claude 작업 안내

이 저장소는 i2r 보드(IoT PLC i2r-02/03/04, Otto)와 같은 i2r 프로토콜로 동작하는 서버 쪽 프로그램입니다.
i2r 전체 안내판(다른 키트, 작업 폴더, i2r 스킬)은 https://raw.githubusercontent.com/kdi6033/i2r/main/AGENTS.md 에 있습니다.
지금 완성된 것은 `sim/` 시뮬레이터입니다. `local/`, `aws/`, `common/`은 만드는 중이라 아직 없습니다.

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

`git pull`을 하고, 시뮬레이터가 켜져 있었다면 다시 켭니다.

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
| `sim/firmware/` | 가상보드 프로그램 .bin (I2R_IoT 02_PLC_Simulator, i2r-02/03/04)과 `manifest.json`. I2R_IoT, I2R_PLC가 바뀌면 `python sim/firmware/build_firmware.py`로 다시 만들고 커밋 |

## 9. 프로토콜 (`protocol.md`)

- i2r 통신 프로토콜의 **원본은 이 저장소의 [protocol.md](protocol.md)** 입니다. 다른 저장소는 이 파일을 가리키기만 합니다.
- 프로토콜을 바꿀 때는 **`protocol.md`, `sim/board.py`, `sim/test_protocol.py`를 한 커밋에서 함께** 고치고, 시뮬레이터로 `test_protocol.py`가 모두 통과하는지 확인한 뒤 올립니다.
- 필드와 명령은 더하기만 합니다. 지우거나 뜻을 바꾸면 `pv`를 올리고 protocol.md "바뀐 내역"에 적습니다.
- 사용자 스케치는 JSON을 직접 만들지 않고 라이브러리 함수를 씁니다 (protocol.md 0.1).


## 안내판을 거쳐 왔다면 (처음 한 번 끝날 때)

사용자가 "https://github.com/kdi6033/i2r 읽고 ..."로 시작했거나 작업 폴더 맨 위에 이 키트 폴더가 있다면, 환경 만들기가 끝날 때 아래 두 가지를 **빠뜨리지 말고** 합니다. 자세한 내용은 안내판 원문(`curl -sL https://raw.githubusercontent.com/kdi6033/i2r/main/AGENTS.md`) 4.3장과 6장입니다.

1. 작업 폴더 맨 위에 `CLAUDE.md`(작업 폴더 안내)가 없으면 만듭니다. 받은 키트 폴더와 "먼저 읽을 파일"을 표로 적습니다.
2. "다음부터 주소 없이 말할 수 있게 이 PC에 i2r 스킬을 설치할까요?"라고 묻고, 좋다고 하면 `https://raw.githubusercontent.com/kdi6033/i2r/main/skills/i2r/SKILL.md`를 사용자 폴더의 `.claude/skills/i2r/SKILL.md`로 받습니다.
