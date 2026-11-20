"""i2r 프로토콜 규칙 시험 (i2r.md 5.5).

가상 보드와 실제 보드에 똑같이 쓴다. 대상의 그룹과 MAC만 알려 주면 된다.

  python test_protocol.py --group sim-7f3a --mac 02:AB:CD:EF:01:01
  python test_protocol.py --group aabbccddeeff --mac AA:BB:CC:DD:EE:FF     (실제 보드: 내 보드의 그룹과 MAC)

주의: 출력을 켜고 끈다. 실제 보드라면 릴레이에 아무것도 연결하지 않은 상태에서 한다.
"m":"*" 시험은 같은 그룹의 모든 보드 OUT2를 잠깐 켰다가 모두 끈다.
"""

import argparse
import json
import queue
import sys
import time
import uuid

import paho.mqtt.client as mqtt

import servers

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass


class Probe:
    def __init__(self, server, port, group, mac, user=None, password=None):
        self.topic_in = f"i2r/{group}/in"
        self.topic_out = f"i2r/{group}/out"
        self.mac = mac
        self.q = queue.Queue()
        # client id는 보드 MAC과 겹치지 않게 (겹치면 서버가 보드를 끊는다)
        self.cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="i2r-test-" + uuid.uuid4().hex[:8])
        if user:
            self.cli.username_pw_set(user, password or "")
        self.cli.on_message = lambda c, u, m: self.q.put(m.payload.decode("utf-8", "replace"))
        self.cli.connect(server, port, 30)
        self.cli.loop_start()
        time.sleep(0.5)
        self.cli.subscribe(self.topic_out)
        time.sleep(1.0)

    def send(self, obj):
        self.drain()
        self.cli.publish(self.topic_in, json.dumps(obj))

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def wait(self, pred, sec=4.0):
        end = time.time() + sec
        while time.time() < end:
            try:
                t = self.q.get(timeout=max(0.05, end - time.time()))
            except queue.Empty:
                break
            try:
                j = json.loads(t)
            except json.JSONDecodeError:
                continue
            if j.get("fr") == self.mac and pred(j):
                return j
        return None

    def state(self):
        self.send({"c": "gs", "m": self.mac})
        return self.wait(lambda j: "out" in j)

    def close(self):
        self.cli.loop_stop()
        self.cli.disconnect()


def run(p):
    results = []

    def check(name, ok, note=""):
        results.append(ok)
        print(f"{'✅' if ok else '❌'} {name}" + (f"  ({note})" if note else ""), flush=True)

    s = p.state()
    check("gs (MAC 있음) → 상태", s is not None and "in" in s and "out" in s)
    if s is None:
        print("대상이 답하지 않습니다. 그룹, MAC, 서버를 확인하세요.")
        return False
    ports = len(s["out"])
    check("상태에 봉투 필드 (m, fr, t, e)", all(k in s for k in ("m", "fr", "t", "e")))

    p.send({"c": "gs"})
    check("gs (MAC 없음) → 상태", p.wait(lambda j: "out" in j) is not None)

    p.send({"c": "so", "m": p.mac, "n": 0, "v": 1})
    j = p.wait(lambda j: "out" in j)
    check("so 켜기 → 상태에 반영", j is not None and j["out"][0] == 1)

    p.send({"c": "so", "m": p.mac, "n": 0, "v": 0})
    j = p.wait(lambda j: "out" in j)
    check("so 끄기 → 상태에 반영", j is not None and j["out"][0] == 0)

    p.send({"c": "so", "n": 0, "v": 1})
    e = p.wait(lambda j: j.get("c") == "err")
    check("so (MAC 없음) → err no_m", e is not None and e.get("code") == "no_m")
    s = p.state()
    check("  … 출력은 움직이지 않음", s is not None and s["out"][0] == 0)

    p.send({"c": "so", "m": "AA:BB:CC:00:11:22", "n": 0, "v": 1})
    noisy = p.wait(lambda j: True, sec=1.5)
    s = p.state()
    check("so (다른 보드 MAC) → 무시", noisy is None and s is not None and s["out"][0] == 0)

    p.send({"c": "so", "m": "*", "n": 1, "v": 1})
    j = p.wait(lambda j: "out" in j)
    check('so ("m":"*") → 실행', j is not None and j["out"][1] == 1)
    # "*"는 같은 그룹의 모든 보드를 켰으므로, 되돌릴 때도 "*"로 모두 끈다
    p.send({"c": "so", "m": "*", "n": 1, "v": 0})
    p.wait(lambda j: "out" in j)

    p.send({"c": "so", "m": p.mac, "n": ports + 3, "v": 1})
    e = p.wait(lambda j: j.get("c") == "err")
    check("so (없는 번호) → err bad_n", e is not None and e.get("code") == "bad_n")

    p.send({"c": "so", "m": p.mac, "n": 0, "v": 5})
    e = p.wait(lambda j: j.get("c") == "err")
    check("so (값 범위 밖) → err bad_v", e is not None and e.get("code") == "bad_v")

    p.send({"c": "so", "m": p.mac, "n": 2, "v": 1, "id": 42})
    j = p.wait(lambda j: "out" in j)
    check("id 붙인 명령 → 답에 같은 id", j is not None and j.get("id") == 42)
    p.send({"c": "so", "m": p.mac, "n": 2, "v": 0})
    p.wait(lambda j: "out" in j)

    p.send({"c": "nope", "m": p.mac})
    e = p.wait(lambda j: j.get("c") == "err")
    check("모르는 명령 → err unknown", e is not None and e.get("code") == "unknown")

    p.send({"c": "so", "m": p.mac, "n": 3, "v": 1, "d": 2})
    j = p.wait(lambda j: "out" in j)
    on_ok = j is not None and j["out"][3] == 1
    j2 = p.wait(lambda j: "out" in j and j["out"][3] == 0, sec=5.0)
    check("so d=2 → 켜졌다가 2초 뒤 저절로 꺼짐", on_ok and j2 is not None)

    # ---- 입력 규칙 (bio) ----
    # 마지막 입력에 규칙을 넣고 지운다. 입력을 직접 넣을 수는 없으므로 실행은 시험하지 않는다.
    port = len(s["in"]) - 1
    p.send({"c": "bio", "o": "deleteAll", "m": p.mac, "n": port})
    time.sleep(0.5)

    def bio_list():
        p.send({"c": "bio", "o": "list", "m": p.mac, "n": port})
        lines = []
        while True:
            j = p.wait(lambda j: j.get("c") == "bio" and j.get("o") == "list", sec=1.5)
            if j is None:
                return lines
            lines.extend(j.get("ps") or [])

    p.send({"c": "bio", "o": "insert", "m": p.mac, "n": port, "tr": 1, "d": 3, "du": 5,
            "ps": [{"m": p.mac, "n": 0, "v": 1}]})
    time.sleep(0.5)
    rows = bio_list()
    ok = (len(rows) == 1 and rows[0].get("n") == 0 and rows[0].get("d") == 3
          and rows[0].get("du") == 5 and rows[0].get("tr") in (1, True) and rows[0].get("v") in (1, True))
    check("bio insert → list에 한 줄 (tr, d, du, n, v)", ok, f"{len(rows)}줄")

    if rows:
        si = rows[0].get("sI")
        p.send({"c": "bio", "o": "delete", "m": p.mac, "n": port, "sI": si})
        time.sleep(0.5)
        check("bio delete → 목록에서 빠짐", bio_list() == [])

    p.send({"c": "bio", "o": "insert", "m": p.mac, "n": port, "tr": 0, "d": 0, "du": 0,
            "ps": [{"m": p.mac, "n": 0, "v": 0}]})
    p.send({"c": "bio", "o": "insert", "m": p.mac, "n": port, "tr": 1, "d": 0, "du": 0,
            "ps": [{"m": p.mac, "n": 1, "v": 1}]})
    time.sleep(0.5)
    two = len(bio_list())
    p.send({"c": "bio", "o": "deleteAll", "m": p.mac, "n": port})
    time.sleep(0.5)
    check("bio deleteAll → 목록이 빔", two == 2 and bio_list() == [], f"지우기 전 {two}줄")

    # ---- 시간표 (sch) ----
    # 마지막 출력에 새벽 3시 시간표를 넣고 지운다 (시험 중에 출력이 켜지지 않는 시간)
    pi = ports - 1
    p.send({"c": "sch", "o": "deleteAll", "m": p.mac, "pi": pi})
    time.sleep(0.5)

    def sch_lines(sec=1.5):
        out = []
        while True:
            j = p.wait(lambda j: j.get("c") == "sch" and j.get("o") == "list", sec=sec)
            if j is None:
                return out
            out.append(j)

    p.send({"c": "sch", "o": "insert", "m": p.mac, "pi": pi, "start": 10800, "end": 11100, "rm": "daily", "dw": 0})
    rows = sch_lines()
    check("sch insert → 목록을 다시 보냄 (start, end, rm)", len(rows) == 1 and rows[0].get("start") == 10800
          and rows[0].get("end") == 11100 and rows[0].get("rm") == "daily", f"{len(rows)}줄")

    p.send({"c": "sch", "o": "insert", "m": p.mac, "pi": pi, "start": 10800, "end": 11100, "rm": "weekly", "dw": [1, 3]})
    rows = sch_lines()
    check("sch weekly 요일 2개 → 2줄 더 (모두 3줄)", len(rows) == 3, f"{len(rows)}줄")

    p.send({"c": "sch", "o": "delete", "m": p.mac, "pi": pi, "sI": 0})
    rows = sch_lines()
    check("sch delete → 한 줄 빠짐", len(rows) == 2, f"{len(rows)}줄")

    p.send({"c": "sch", "o": "deleteAll", "m": p.mac, "pi": pi})
    time.sleep(0.5)
    p.send({"c": "sch", "o": "list", "m": p.mac, "pi": pi})
    check("sch deleteAll → 목록이 빔", sch_lines() == [])

    # ---- 센서 규칙 (bs) ----
    # 온도 센서가 있는 보드(i2r-03)만. 기준값을 아주 높게(80도) 잡아 시험 중에 실행되지 않게 한다
    if "temp" in s:
        p.send({"c": "bs", "o": "deleteAll", "m": p.mac, "ts": "temp"})
        time.sleep(0.5)

        def bs_list():
            p.send({"c": "bs", "o": "list", "m": p.mac, "ts": "temp"})
            out = []
            while True:
                j = p.wait(lambda j: j.get("c") == "bs" and j.get("o") == "list", sec=1.5)
                if j is None:
                    return out
                out.append(j)

        p.send({"c": "bs", "o": "insert", "m": p.mac, "ts": "temp", "tr": 1, "tv": 80, "d": 2, "du": 5,
                "ps": [{"m": p.mac, "n": 0, "v": 1}]})
        time.sleep(0.5)
        rows = bs_list()
        ok = (len(rows) == 1 and rows[0].get("tv") == 80 and rows[0].get("tr") in (1, True)
              and rows[0].get("d") == 2 and rows[0].get("du") == 5 and (rows[0].get("ps") or [{}])[0].get("n") == 0)
        check("bs insert → list에 한 줄 (tr, tv, d, du, ps)", ok, f"{len(rows)}줄")
        if rows:
            p.send({"c": "bs", "o": "delete", "m": p.mac, "ts": "temp", "sI": rows[0].get("sI")})
            time.sleep(0.5)
            check("bs delete → 목록에서 빠짐", bs_list() == [])
        p.send({"c": "bs", "o": "insert", "m": p.mac, "ts": "temp", "tr": 0, "tv": -30, "d": 0, "du": 0,
                "ps": [{"m": p.mac, "n": 0, "v": 0}]})
        time.sleep(0.5)
        p.send({"c": "bs", "o": "deleteAll", "m": p.mac, "ts": "temp"})
        time.sleep(0.5)
        check("bs deleteAll → 목록이 빔", bs_list() == [])

        # 보정은 보드에 저장되는 값을 바꾸므로 가상 보드(02:로 시작)에서만 시험한다
        if p.mac.startswith("02:"):
            before = s.get("temp")
            p.send({"c": "bs", "o": "cali", "m": p.mac, "ts": "temp", "v": 25.0})
            j = p.wait(lambda j: "temp" in j and "out" in j)
            check("bs 보정 (cali v=25) → 상태의 온도가 25.0", j is not None and abs(j["temp"] - 25.0) < 0.15,
                  f"보정 전 {before}")
            p.send({"c": "bs", "o": "cali", "m": p.mac, "ts": "temp", "v": before})   # 되돌리기
            p.wait(lambda j: "temp" in j and "out" in j)

    p.cli.publish(p.topic_in, json.dumps({"c": "so", "m": p.mac, "fr": p.mac, "n": 0, "v": 1}))
    time.sleep(1.0)
    s = p.state()
    check("fr이 자기 MAC인 명령 → 무시", s is not None and s["out"][0] == 0)

    passed = sum(results)
    print(f"\n결과: {passed}/{len(results)} 통과")
    return passed == len(results)


def main():
    ap = argparse.ArgumentParser(description="i2r 프로토콜 규칙 시험")
    ap.add_argument("--server", default=None, help="MQTT 서버 (없으면 목록에서 고르기, 입력할 수 없으면 test.mosquitto.org)")
    ap.add_argument("--port", type=int, default=None)
    ap.add_argument("--group", required=True)
    ap.add_argument("--mac", required=True)
    ap.add_argument("--user", default=None)
    a = ap.parse_args()
    a.server, a.port, a.user = servers.choose(a.server, a.port, a.user)
    pw = None
    if a.user:
        import getpass
        pw = getpass.getpass("MQTT 비밀번호: ")
    p = Probe(a.server, a.port, a.group, a.mac.upper(), a.user, pw)
    try:
        ok = run(p)
    finally:
        p.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
