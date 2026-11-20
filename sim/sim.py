"""i2r 시뮬레이터: 가상 i2r 보드 여러 대 + 웹 화면.

  python sim.py                                   가상 보드 1대, 서버는 목록에서 고르기, 화면 http://localhost:8000
  python sim.py --server test.mosquitto.org       목록 없이 바로 (Claude, 스크립트는 이렇게)
  python sim.py --boards 3                        3대
  python sim.py --server 192.168.0.50             i2r-05 로컬 서버
  python sim.py --server 내서버 --user 아이디     아이디·비밀번호가 있는 서버 (비밀번호는 묻는다)
  python sim.py --no-web                          화면 없이 (Claude의 자동 시험용)

화면에서 입력 스위치를 누르면 가상 입력이 켜지고, 서버에서 온 so 명령으로 출력 LED가 켜진다.
제어 화면에서 "입력 N"을 누르면 입력 규칙(bio), "스위치 N" 이름을 누르면 시간표(sch),
온도나 습도 카드를 누르면 센서 규칙과 보정(bs) 설정 창이 열린다.
"""

import argparse
import collections
import getpass
import json
import os
import random
import socket
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from board import VirtualBoard  # noqa: E402
import servers  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
PUBLIC_SERVERS = servers.PUBLIC_SERVERS
PUBLIC_MAX = 5

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass


class Log:
    """주고받은 메시지를 화면에 보여 주려고 최근 300개를 모아 둔다."""

    def __init__(self, quiet=False):
        self.items = collections.deque(maxlen=300)
        self.seq = 0
        self.lock = threading.Lock()
        self.quiet = quiet

    def __call__(self, board, kind, text):
        who = board.index if board else 0          # 0 = 제어 화면
        with self.lock:
            self.seq += 1
            self.items.append({"seq": self.seq, "t": time.strftime("%H:%M:%S"),
                               "board": who, "kind": kind, "text": text})
        if not self.quiet:
            arrow = {"tx": "→", "rx": "←", "info": "·", "ui": "⇒"}.get(kind, " ")
            name = f"보드{who}" if who else "제어화면"
            print(f"[{name}] {arrow} {text}", flush=True)

    def since(self, seq):
        with self.lock:
            return [x for x in self.items if x["seq"] > seq]


class UIClient:
    """제어 화면(i2r.link의 보드 화면과 같은 역할)이 쓰는 MQTT 접속.

    보드들이 보내는 상태를 받아 두고, 화면에서 스위치를 누르면 so 명령을 보낸다.
    가상 보드든 실제 보드든 같은 서버, 같은 그룹이면 똑같이 보이고 제어된다.
    client id는 보드 MAC과 겹치지 않게 i2r-sim-ui-로 시작한다.
    """

    def __init__(self, server, port, group, user, password, log):
        import paho.mqtt.client as mqtt
        self.topic_in = f"i2r/{group}/in"
        self.topic_out = f"i2r/{group}/out"
        self.log = log
        self.devices = {}          # MAC → 마지막으로 받은 상태와 정보
        self.sent = collections.deque(maxlen=30)   # 제어 화면이 보낸 명령 (화면에 글자로 보여 줌)
        self.lock = threading.Lock()
        self.seq = 0
        self.cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                               client_id="i2r-sim-ui-" + "".join(random.choice("0123456789abcdef") for _ in range(6)))
        if user:
            self.cli.username_pw_set(user, password or "")
        self.cli.on_connect = self._on_connect
        self.cli.on_message = self._on_message
        self.cli.connect_async(server, port, keepalive=60)
        self.cli.loop_start()

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            return
        client.subscribe(self.topic_out)
        # 보드는 접속 직후 5초 동안 받은 명령을 무시하므로, 조금 기다렸다가 이미 있는 보드를 찾는다
        threading.Timer(6.0, self.ask_all).start()

    def _on_message(self, client, userdata, msg):
        try:
            j = json.loads(msg.payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        if not isinstance(j, dict):
            return
        mac = j.get("fr") or j.get("m")
        if not mac:
            return
        with self.lock:
            d = self.devices.setdefault(mac, {"mac": mac, "online": True, "in": [], "out": []})
            d["seen"] = time.time()
            rx = d.setdefault("rx", collections.deque(maxlen=8))
            rx.appendleft({"t": time.strftime("%H:%M:%S"), "topic": msg.topic, "text": msg.payload.decode("utf-8")})
            c = j.get("c")
            if c == "lw":
                d["online"] = bool(j.get("on"))
            elif c == "hello":
                d.update({k: j[k] for k in ("t", "ver", "sensors", "sim") if k in j})
                d["online"] = True
            elif c == "err":
                d["err"] = {"code": j.get("code"), "re": j.get("re"), "at": time.time()}
            elif c == "bio" and j.get("o") == "list":
                # 입력 규칙 목록: 보드가 규칙마다 한 줄씩 보낸다. 입력 번호별로 모아 둔다
                port = j.get("n")
                for p in j.get("ps") or []:
                    if isinstance(p, dict) and isinstance(port, int):
                        si = p.get("sI", p.get("si"))
                        d.setdefault("bio", {}).setdefault(port, {})[si] = p
            elif c == "sch" and j.get("o") == "list":
                # 시간표 목록: 한 줄씩 온다. pn이 "1/N"이면 새 목록의 첫 줄이라 그 출력 목록을 비운다
                pi, idx = j.get("pi"), j.get("index")
                if isinstance(pi, int) and isinstance(idx, int):
                    slots = d.setdefault("sch", {})
                    if str(j.get("pn", "")).startswith("1/"):
                        slots[pi] = {}
                    slots.setdefault(pi, {})[idx] = {k: j.get(k) for k in ("index", "start", "end", "rm", "dw", "pn")}
            elif c == "bs" and j.get("o") == "list":
                # 센서 규칙 목록: 규칙마다 한 줄씩 온다. 센서 종류(temp, humi)별로 모아 둔다
                ts = {"temperature": "temp", "humidity": "humi"}.get(j.get("ts"), j.get("ts"))
                if isinstance(ts, str):
                    d.setdefault("bs", {}).setdefault(ts, {})[j.get("sI")] = {
                        k: j.get(k) for k in ("sI", "tr", "tv", "d", "du", "ps")}
            elif "out" in j and "in" in j:
                d.update({"in": j["in"], "out": j["out"], "t": j.get("t", d.get("t")), "online": True,
                          "state_at": time.time()})
                for k in ("temp", "humi"):
                    if k in j:
                        d[k] = j[k]

    def send(self, obj):
        text = json.dumps(obj, ensure_ascii=False, separators=(",", ":"))
        if len(text) > 512:
            return False
        if obj.get("c") == "bio" and obj.get("o") == "list" and obj.get("m"):
            # 새로 물어보면 그 입력의 목록을 비우고 답을 다시 모은다
            with self.lock:
                d = self.devices.get(obj["m"])
                if d is not None:
                    d.setdefault("bio", {})[obj.get("n")] = {}
        if obj.get("c") == "sch" and obj.get("m"):
            # 시간표를 바꾸거나 물어보면 그 출력 목록을 비운다 (보드가 목록을 다시 보내거나, 비었으면 안 보냄)
            with self.lock:
                d = self.devices.get(obj["m"])
                if d is not None:
                    d.setdefault("sch", {})[obj.get("pi")] = {}
        if obj.get("c") == "bs" and obj.get("o") == "list" and obj.get("m"):
            # 새로 물어보면 그 센서의 목록을 비우고 답을 다시 모은다
            with self.lock:
                d = self.devices.get(obj["m"])
                if d is not None:
                    d.setdefault("bs", {})[obj.get("ts")] = {}
        self.cli.publish(self.topic_in, text, qos=0, retain=False)
        self.log(None, "ui", text)
        with self.lock:
            self.sent.appendleft({"t": time.strftime("%H:%M:%S"), "topic": self.topic_in,
                                  "m": obj.get("m"), "text": text})
        return True

    def ask_all(self):
        self.send({"c": "gs"})

    def snapshot(self):
        with self.lock:
            now = time.time()
            out = []
            for d in self.devices.values():
                x = dict(d)
                x["age"] = round(now - d.get("seen", now), 1)
                x["rx"] = list(d.get("rx", []))
                x["bio"] = {str(port): sorted(items.values(), key=lambda p: p.get("sI") or 0)
                            for port, items in d.get("bio", {}).items()}
                x["sch"] = {str(pi): [items[k] for k in sorted(items)]
                            for pi, items in d.get("sch", {}).items()}
                x["bs"] = {ts: sorted(items.values(), key=lambda p: p.get("sI") or 0)
                           for ts, items in d.get("bs", {}).items()}
                # 이 보드에 간 명령: m이 이 보드, "*", 또는 없음(gs)
                x["tx"] = [s for s in self.sent if s["m"] in (d["mac"], "*", None)][:8]
                out.append(x)
            return sorted(out, key=lambda x: x["mac"])

    def stop(self):
        self.cli.loop_stop()
        self.cli.disconnect()


def make_mac(prefix_bytes, i):
    # 02로 시작하는 MAC은 "직접 정한 주소"라 실제 기기와 겹치지 않는다
    b = [0x02] + prefix_bytes + [i]
    return ":".join(f"{x:02X}" for x in b)


def lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return None
    finally:
        s.close()


def make_handler(sim):
    index_path = os.path.join(HERE, "web", "index.html")

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            pass  # 웹 요청 기록은 찍지 않는다

        def _json(self, obj, code=200):
            body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            u = urlparse(self.path)
            if u.path in ("/", "/index.html"):
                with open(index_path, "rb") as f:
                    body = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            elif u.path == "/api/boards":
                self._json({"server": sim["server"], "port": sim["port"], "group": sim["group"],
                            "topic_in": f"i2r/{sim['group']}/in", "topic_out": f"i2r/{sim['group']}/out",
                            "boards": [b.snapshot() for b in sim["boards"]]})
            elif u.path == "/api/devices":
                self._json({"devices": sim["ui"].snapshot() if sim.get("ui") else []})
            elif u.path == "/api/log":
                q = parse_qs(u.query)
                since = int(q.get("since", ["0"])[0] or 0)
                self._json({"items": sim["log"].since(since)})
            else:
                self._json({"error": "not found"}, 404)

        def do_POST(self):
            u = urlparse(self.path)
            try:
                n = int(self.headers.get("Content-Length", "0"))
                if n > 2048:
                    return self._json({"error": "too long"}, 413)
                j = json.loads(self.rfile.read(n).decode("utf-8") or "{}")
            except (ValueError, TypeError):
                return self._json({"error": "bad request"}, 400)
            if u.path == "/api/cmd":
                # 제어 화면이 보내는 i2r 명령 (그대로 MQTT in 토픽으로)
                if not sim.get("ui") or not isinstance(j, dict) or not j.get("c"):
                    return self._json({"error": "bad request"}, 400)
                ok = sim["ui"].send(j)
                return self._json({"ok": ok})
            try:
                b = sim["boards"][int(j["board"]) - 1]
            except (ValueError, KeyError, IndexError, TypeError):
                return self._json({"error": "bad request"}, 400)
            if u.path == "/api/input":
                b.set_input(int(j.get("n", -1)), int(j.get("v", 0)))
            elif u.path == "/api/temp":
                if "temp" in j:
                    b.set_temp(float(j["temp"]))
                else:
                    b.set_temp(b.temp + float(j.get("delta", 0)))
            elif u.path == "/api/humi":
                if "humi" in j:
                    b.set_humi(float(j["humi"]))
                else:
                    b.set_humi(b.humi + float(j.get("delta", 0)))
            else:
                return self._json({"error": "not found"}, 404)
            self._json({"ok": True, "board": b.snapshot()})

    return Handler


def main():
    ap = argparse.ArgumentParser(description="i2r 시뮬레이터 (가상 i2r 보드 + 웹 화면)")
    ap.add_argument("--boards", type=int, default=1, help="가상 보드 수 (기본 1)")
    ap.add_argument("--server", default=None, help="MQTT 서버 (없으면 목록에서 고르기, 입력할 수 없으면 test.mosquitto.org)")
    ap.add_argument("--port", type=int, default=None, help="MQTT 포트 (기본 1883)")
    ap.add_argument("--user", default=None, help="MQTT 아이디 (있으면 비밀번호를 묻는다)")
    ap.add_argument("--group", default=None, help="토픽 그룹 (기본: 실행할 때마다 sim-임의코드)")
    ap.add_argument("--kind", default="i2r-03", choices=["i2r-02", "i2r-03", "i2r-04"], help="보드 종류")
    ap.add_argument("--http-port", type=int, default=8000, help="화면 포트 (기본 8000)")
    ap.add_argument("--lan", action="store_true", help="같은 공유기의 휴대폰에서도 화면 열기")
    ap.add_argument("--no-web", action="store_true", help="화면 없이 실행")
    ap.add_argument("--no-browser", action="store_true", help="브라우저를 저절로 열지 않기")
    ap.add_argument("--quiet", action="store_true", help="메시지를 터미널에 찍지 않기")
    a = ap.parse_args()
    a.server, a.port, a.user = servers.choose(a.server, a.port, a.user)

    if a.server in PUBLIC_SERVERS and a.boards > PUBLIC_MAX:
        print(f"[시뮬레이터] 공개 서버({a.server})에는 가상 보드를 {PUBLIC_MAX}대까지만 붙입니다. "
              f"많은 대수 시험은 i2r-05나 내 서버에서 하세요.")
        a.boards = PUBLIC_MAX
    a.boards = max(1, min(a.boards, 50))
    password = getpass.getpass("MQTT 비밀번호: ") if a.user else None
    group = a.group or f"sim-{random.randrange(16**4):04x}"
    prefix = [random.randrange(256) for _ in range(4)]
    ports = 8 if a.kind == "i2r-04" else 4

    log = Log(quiet=a.quiet)
    boards = [VirtualBoard(i + 1, make_mac(prefix, i + 1), group, a.server, a.port, a.user, password,
                           kind=a.kind, ports=ports, log=log) for i in range(a.boards)]
    sim = {"boards": boards, "log": log, "server": a.server, "port": a.port, "group": group}

    print(f"[시뮬레이터] 가상 보드 {len(boards)}대를 {a.server}:{a.port}에 붙입니다. 그룹 {group}")
    for b in boards:
        print(f"  보드{b.index}  {b.mac}  {b.kind}")
    print(f"  보드가 보내는 토픽 i2r/{group}/out  /  명령을 보낼 토픽 i2r/{group}/in")
    if a.server in PUBLIC_SERVERS:
        print("  공개 서버입니다. 누구나 이 토픽을 볼 수 있으니 개인 정보를 넣지 마세요.")
    print(f"  붙여 넣어 보세요: {json.dumps({'c': 'so', 'm': boards[0].mac, 'n': 0, 'v': 1})}")

    for b in boards:
        b.start()

    httpd = None
    if not a.no_web:
        sim["ui"] = UIClient(a.server, a.port, group, a.user, password, log)
        host = "0.0.0.0" if a.lan else "127.0.0.1"
        try:
            httpd = ThreadingHTTPServer((host, a.http_port), make_handler(sim))
        except OSError:
            print(f"[시뮬레이터] {a.http_port}번 포트를 쓸 수 없습니다. --http-port 8001 처럼 다른 번호로 실행하세요.")
            for b in boards:
                b.stop()
            return 1
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
        url = f"http://localhost:{a.http_port}"
        print(f"[시뮬레이터] 화면: {url}")
        if a.lan:
            ip = lan_ip()
            if ip:
                print(f"[시뮬레이터] 휴대폰 (같은 공유기): http://{ip}:{a.http_port}")
        if not a.no_browser:
            threading.Timer(1.0, lambda: webbrowser.open(url)).start()
    print("[시뮬레이터] 끄려면 Ctrl+C 를 누르거나 이 창을 닫으세요.", flush=True)

    try:
        while True:
            for b in boards:
                b.tick()
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        print("\n[시뮬레이터] 끄는 중...")
        for b in boards:
            b.stop()
        if sim.get("ui"):
            sim["ui"].stop()
        if httpd:
            httpd.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
