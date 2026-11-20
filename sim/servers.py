"""MQTT 서버 고르기 (sim.py, test_protocol.py가 같이 쓴다).

--server 없이 실행하면 아래 목록을 보여 주고 번호를 받는다.
키보드로 입력할 수 없는 곳(Claude, 스크립트, 백그라운드 실행)에서는 묻지 않고 기본값을 쓴다.
목록을 바꾸면 I2R_IoT/tools/send-si.ps1의 목록도 같이 맞춘다.
"""

import sys

DEFAULT_SERVER = "test.mosquitto.org"
DEFAULT_PORT = 1883

# (주소, 설명). 맨 앞이 기본값.
CHOICES = [
    ("test.mosquitto.org", "기본, 교육용 공개 서버"),
    ("broker.emqx.io", "1번이 느리거나 안 될 때"),
]

PUBLIC_SERVERS = {"test.mosquitto.org", "broker.emqx.io", "broker.hivemq.com"}


def can_ask():
    try:
        return sys.stdin is not None and sys.stdin.isatty()
    except Exception:
        return False


def choose(server, port, user):
    """server가 None이면 고르게 한다. (server, port, user)를 돌려준다."""
    if server:
        return server, port or DEFAULT_PORT, user
    if not can_ask():
        return DEFAULT_SERVER, port or DEFAULT_PORT, user

    print("MQTT 서버를 고르세요 (실제 보드와 같이 쓰면 보드와 같은 서버여야 합니다)")
    for i, (host, note) in enumerate(CHOICES, 1):
        print(f"  {i}) {host:<20} {note}")
    print(f"  {len(CHOICES) + 1}) 직접 입력             i2r-05 로컬 서버, 아마존 서버")
    while True:
        try:
            pick = input("번호 (Enter = 1): ").strip()
        except EOFError:
            pick = ""
        if pick == "":
            pick = "1"
        if pick.isdigit() and 1 <= int(pick) <= len(CHOICES):
            return CHOICES[int(pick) - 1][0], port or DEFAULT_PORT, user
        if pick == str(len(CHOICES) + 1):
            break
        print("  목록의 번호를 넣으세요.")

    while True:
        host = input("서버 주소 (예: 192.168.0.50): ").strip()
        if host:
            break
    p = input(f"포트 (Enter = {port or DEFAULT_PORT}): ").strip()
    port = int(p) if p.isdigit() else (port or DEFAULT_PORT)
    if user is None:
        u = input("아이디 (없으면 Enter): ").strip()
        user = u or None
    return host, port, user
