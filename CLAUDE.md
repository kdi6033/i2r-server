# i2r-server: Claude 작업 안내

이 저장소는 i2r 보드(IoT PLC i2r-02/03/04, Otto)와 같은 i2r 프로토콜로 동작하는 서버 쪽 프로그램입니다.
지금 완성된 것은 `sim/` 시뮬레이터입니다. `local/`, `aws/`, `common/`은 만드는 중이라 아직 없습니다.

사용자는 보통 이렇게 말합니다.
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
- 이미 켜져 있으면 8000번 포트 오류가 납니다. 먼저 끄거나 `--http-port 8001`을 씁니다.

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

## 4. 프로토콜 시험

"프로토콜 시험해 줘"라고 하면, 켜져 있는 시뮬레이터의 그룹과 MAC으로 실행합니다. 서버는 시뮬레이터와 같게 합니다.

```
python test_protocol.py --server test.mosquitto.org --group sim-xxxx --mac 02:..:01
```

결과(`결과: N/N 통과`)와 실패한 항목을 표로 보여 줍니다.

실제 보드를 시험할 때는:
- 출력을 켜고 끄므로, **릴레이에 아무것도 연결하지 않은 상태**인지 먼저 확인합니다.
- 보드의 그룹과 MAC은 사용자에게 받습니다(보드 시리얼 출력에 나옵니다).

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
| `sim/test_protocol.py` | 프로토콜 규칙 시험 |
| `sim/web/index.html` | 제어 화면 |
| `sim/start-sim.bat` | Windows에서 두 번 눌러 켜기 |
| `sim/README.md` | 옵션과 메시지 형식 |

프로토콜 원문은 https://github.com/kdi6033/i2r 의 protocol.md에 있습니다.
