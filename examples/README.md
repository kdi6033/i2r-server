# i2r-server 보드 펌웨어 예제

i2r PLC(i2r-02, i2r-03, i2r-04)에 올리는 펌웨어 예제입니다. 어떻게 연결해서 쓰는지에 따라 세 그룹으로 나눕니다.

| 그룹 | 폴더 | 구성 | 상태 |
|---|---|---|:---:|
| ① 혼자 동작 | [standalone/](standalone/) | 보드 혼자 동작합니다. 보드가 웹서버가 되어, 같은 공유기의 휴대폰이나 PC에서 `http://i2r.local`로 제어합니다 | ✅ |
| ② 현장 서버 | `local/` | PLC 여러 대(10대 정도)와 i2r-05(MQTT 서버)를 현장(집, 공장) 안에서 묶습니다 | 🛠️ 만드는 중 |
| ③ 클라우드 | `cloud/` | 아마존 서버와 연결해 인터넷 너머에서 제어합니다 | 🛠️ 만드는 중 |

- 그룹 안에서 보드 종류마다 폴더를 나눕니다: `standalone/i2r-02/`, `i2r-03/`, `i2r-04/`. 세 폴더의 소스는 `#define I2R_BOARD` 한 줄만 다릅니다 (고칠 때는 함께 고칩니다).
- 이 예제로 내 프로그램을 만들려면 Claude에게 "standalone 참고해서 ~하는 프로그램 만들어 줘"라고 하면 됩니다. Claude는 [standalone/CLAUDE.md](standalone/CLAUDE.md)를 따릅니다.
- 버전에 그룹이 붙습니다 (`standalone-0.1.0`). 같은 이름의 파일이어도 시리얼 첫 줄과 웹페이지에서 구별할 수 있습니다.
- 명령 JSON은 [protocol.md](../protocol.md)의 `so`, `bio`, `sch`, `bs`, `st`와 같은 모양입니다.

## 설치된 펌웨어로 바로 쓰기 (프로그램 없이)

1. https://kdi.i2r.link/firmware 맨 아래 **i2r_server 펌웨어 → ① 혼자 동작**에서 내 보드를 누르고 "COM 포트 선택하고 펌웨어 설치 시작"을 누릅니다.
2. 설치가 끝나면 바로 아래 **Wi-Fi 정보 보내기**에 SSID와 PW를 넣고 보냅니다. (기기연결의 COM 포트 화면도 됩니다. Email, Broker는 저장하지 않습니다)
3. 같은 Wi-Fi의 휴대폰이나 PC에서 `http://i2r.local`을 엽니다.

## ① standalone: 보드 혼자 동작

i2r.link 장치 화면과 같은 기능을 보드가 직접 띄웁니다.

| 화면 | 하는 일 | 명령 |
|---|---|---|
| 입력 상태 | 입력 이름을 누르면 입력 규칙: 켜질 때/꺼질 때, 지연 뒤 출력 켜기/끄기, 동작 시간 | `bio` |
| 출력 제어 | 스위치로 켜고 끄기. 이름을 누르면 스케줄: 기기 시간, 시간대, 동기화, 매일/매주 | `so`, `sch`, `st` |
| 온도, 습도 (i2r-03) | 센서 트리거: 올라갈 때/내려갈 때 출력 바꾸기, 보정 | `bs` |
| ⚙ | 기기 이름, 입력과 출력 이름 바꾸기 | `name` (이 예제만) |

- **시간**: 켜질 때와 1시간마다 인터넷 시간(NTP)으로 맞춥니다. 인터넷이 없으면 보드 시계로 계속 돌고, 10분마다 시각을 저장해 다시 켜도 이어 갑니다.
- **저장**: 규칙, 스케줄, 센서 트리거, 보정, 이름은 보드(NVS)에 저장되어 전원이 꺼져도 남습니다.
- **Wi-Fi**: USB로 받은 `{"c":"si",...}`에서 SSID와 PW만 I2R_IoT와 같은 자리(NVS `i2riot`)에 저장합니다. Email, Broker는 저장하지 않습니다.

### 직접 컴파일하기

필요한 라이브러리: I2R_PLC, I2R_IoT, ArduinoJson, PubSubClient

```
arduino-cli compile --fqbn esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app examples/standalone/i2r-03
```

- 다른 보드는 그 폴더를 씁니다: `examples/standalone/i2r-02`, `examples/standalone/i2r-04`
- 배포용(Wi-Fi 값 없음): `--build-property "compiler.cpp.extra_flags=-DI2R_RELEASE"`
- 개발할 때 Wi-Fi를 소스에 넣고 싶으면 `secrets.example.h`를 `secrets.h`로 복사해 적습니다. `secrets.h`는 GitHub에 올라가지 않습니다.

### 주의

- 같은 공유기에 붙은 사람은 누구나 이 화면으로 릴레이를 켤 수 있습니다. 공용 Wi-Fi에서 쓰지 않습니다.
- 같은 공유기에 i2r 보드가 여러 대면 `i2r.local` 이름이 겹칩니다. `HOST_NAME`을 보드마다 다르게 바꿉니다.
- i2r-02, i2r-04 입력은 무전압 접점입니다. 입력 단자에 전압을 넣지 않습니다.
