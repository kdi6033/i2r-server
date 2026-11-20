# i2r-server

i2r 보드(IoT PLC i2r-02/03/04, Otto 로봇)와 **같은 i2r 프로토콜**로 동작하는 서버 쪽 프로그램을 모은 저장소입니다.

| 폴더 | 내용 | 상태 |
|---|---|:---:|
| [sim/](sim/) | **시뮬레이터**: PC에 가상 i2r 보드를 띄우고, 제어 화면과 프로토콜 시험을 함께 제공 | ✅ |
| `local/` | 로컬 서버: 내 PC나 i2r-05에 MQTT 브로커, 백엔드, 대시보드 설치 | 🛠️ 만드는 중 |
| `aws/` | 아마존(EC2) 개인 서버 설치 | 🛠️ 만드는 중 |
| `common/` | local과 aws가 함께 쓰는 코드 (브로커 설정, 백엔드, 대시보드 틀) | 🛠️ 만드는 중 |

## 시뮬레이터로 시작하기

보드 없이도 i2r 프로토콜을 확인할 수 있습니다. 가상 보드는 실제 보드 펌웨어와 같은 메시지를 주고받고, 릴레이 대신 화면의 LED가 켜지므로 안전합니다.

켜는 방법은 두 가지입니다.

### 방법 A: Claude Code로 한 줄 (추천)

내가 설치하는 것은 **Git, VS Code, Claude Code 확장** 3개뿐입니다. 빈 폴더를 VS Code로 열고 Claude에게 아래처럼 말합니다.

```
https://github.com/kdi6033/i2r-server 저장소를 이 폴더에 내려받고,
그 안의 CLAUDE.md를 읽은 다음 시뮬레이터를 켜 줘.
```

Python과 라이브러리 설치, 시뮬레이터 켜기, 화면 열기는 Claude가 [CLAUDE.md](CLAUDE.md)를 읽고 합니다. 나는 **MQTT 서버만 고르면** 됩니다.
그다음부터는 "프로토콜 시험해 줘", "다시 켜 줘", "최신으로 업데이트해 줘"처럼 말하면 됩니다.

### 방법 B: 직접 켜기

**1. 준비 (처음 한 번)**

- [Python 3](https://www.python.org/downloads/)을 설치합니다. Windows에서는 설치 화면의 **Add python.exe to PATH**를 꼭 체크합니다.
- 이 저장소를 받습니다: `git clone https://github.com/kdi6033/i2r-server` (또는 **Code → Download ZIP**)
- MQTT 라이브러리를 설치합니다: `python -m pip install "paho-mqtt>=2.0"`

**2. 켜기**

```
cd i2r-server/sim
python sim.py --boards 2
```

Windows에서는 `sim/start-sim.bat`을 두 번 눌러도 됩니다 (가상 보드 1대, 라이브러리가 없으면 저절로 설치).

켜면 서버를 고르는 목록이 나옵니다. 번호를 넣거나 Enter(1번)를 누릅니다.

```
MQTT 서버를 고르세요 (실제 보드와 같이 쓰면 보드와 같은 서버여야 합니다)
  1) test.mosquitto.org   기본, 교육용 공개 서버
  2) broker.emqx.io       1번이 느리거나 안 될 때
  3) 직접 입력             i2r-05 로컬 서버, 아마존 서버
번호 (Enter = 1):
```

`--server broker.emqx.io`처럼 서버를 붙여 실행하면 목록 없이 바로 켜집니다.

**3. 화면**

브라우저에서 **http://localhost:8000** 이 저절로 열립니다.

| 누르는 곳 | 하는 일 |
|---|---|
| 가상 보드의 **IN1 ~ IN4** | 가상 입력 켜기/끄기 (실제 단자에 신호를 넣은 것과 같음) |
| 제어 화면의 **스위치 토글** | `so` 명령으로 출력 켜기/끄기 |
| 제어 화면의 **입력 N** | 입력 규칙 (`bio`): 입력이 들어오면 다른 출력 켜기 |
| 제어 화면의 **스위치 N** 이름 | 시간표 (`sch`): 정한 시간에 켜고 끄기 |
| 제어 화면의 **온도, 습도** 카드 | 센서 트리거와 보정 (`bs`) |

**4. 끄기**: 시뮬레이터 창에서 **Ctrl+C** 또는 창 닫기.

**5. 프로토콜 시험**: 가상 보드나 실제 보드에 프로토콜 규칙 시험(`gs`, `so`, `bio`, `sch`, `bs` 등)을 돌립니다. 그룹과 MAC은 시뮬레이터를 켤 때 창에 나옵니다.

```
python test_protocol.py --server test.mosquitto.org --group sim-1a2b --mac 02:AB:CD:EF:01:01
```

자세한 옵션(서버 바꾸기, 보드 수, i2r-04)과 메시지 형식은 [sim/README.md](sim/README.md)에 있습니다.

## MQTT 서버

켤 때 목록에서 고릅니다. 기본은 공개 시험 서버 `test.mosquitto.org:1883`이고, 화면에 보드가 늦게 나타나거나 응답이 느리면 2번 `broker.emqx.io`를 고릅니다.

- 실제 보드와 같이 쓸 때는 **보드도 같은 서버**여야 합니다. 보드 서버는 I2R_IoT의 `tools/send-si.ps1`로 바꿉니다. 시뮬레이터만 바꾸면 화면에 보드가 나타나지 않습니다.

- 공개 서버는 **누구나 토픽을 볼 수 있습니다.** 개인 정보, 비밀번호를 메시지에 넣지 마세요.
- 그래서 시뮬레이터는 실행할 때마다 그룹 이름을 `sim-` 뒤 임의 코드로 바꾸고, 공개 서버에는 가상 보드를 5대까지만 붙입니다.
- 실제 보드를 시험할 때는 보드의 MAC과 그룹을 공개된 곳에 적지 마세요. 그 값을 아는 사람은 같은 서버에서 보드에 명령을 보낼 수 있습니다.

## 관련 저장소

| 저장소 | 내용 |
|---|---|
| [i2r](https://github.com/kdi6033/i2r) | i2r 통신 프로토콜 (MQTT 토픽, JSON 명령) |
| [I2R_IoT](https://github.com/kdi6033/I2R_IoT) | 보드 쪽 통신 라이브러리 (Wi-Fi, MQTT, 시간) |
| [I2R_PLC](https://github.com/kdi6033/I2R_PLC) | i2r-02/03/04 입출력 라이브러리 |
| [i2r-plc](https://github.com/kdi6033/i2r-plc) | IoT PLC AI 코딩 키트 |

## 라이선스

MIT License, i2r
