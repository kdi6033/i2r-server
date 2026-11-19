"""가상 i2r 보드 한 대.

실제 보드(I2R_IoT)와 같은 방식으로 MQTT 서버에 붙어 i2r 프로토콜을 주고받는다.
규칙은 i2r.md 5.5 "여러 보드와 센서를 붙여도 오류가 없게"를 따른다.

  토픽      i2r/{그룹}/in (받기), i2r/{그룹}/out (보내기)
  봉투      m = 받는 보드 MAC, fr = 보낸 보드 MAC, t = 보드 종류, e = 그룹
  MAC 규칙  출력을 바꾸는 명령(so, st 등)은 m 필수. 여러 대는 "m":"*". gs는 m이 없어도 됨
  유언      끊기면 서버가 {"c":"lw","fr":MAC,"on":0}을 대신 보냄
  접속하면  lw on:1, hello, 상태를 보냄
  보내는 양 바뀔 때만 (0.2초 묶음), 60초마다 살아 있음 표시, 온도는 0.2도 이상 바뀔 때
  오류      실행하지 못한 명령은 {"c":"err","re":명령,"code":..}로 알림
  입력 규칙 bio (실제 보드 펌웨어의 BioManager와 같은 형식)
            insert  {"c":"bio","o":"insert","m":이보드,"n":입력,"tr":1,"d":지연초,"du":동작초,
                     "ps":[{"m":대상보드,"n":출력,"v":1}]}
            list    {"c":"bio","o":"list","m":이보드,"n":입력} → 규칙마다 한 줄씩
                    {"c":"bio","o":"list","n":입력,"ps":[{"tr","d","du","m","n","v","sI"}]}
            delete  {"c":"bio","o":"delete","m":이보드,"n":입력,"sI":번호}
            deleteAll {"c":"bio","o":"deleteAll","m":이보드,"n":입력}
            입력 n이 tr과 같아지면 d초 뒤 대상 출력을 v로 바꾸고, du초가 지나면 끈다 (0 = 계속).
            대상이 다른 보드면 so 명령을 in 토픽으로 보낸다. 규칙은 메모리에만 있다 (끄면 사라짐).
  시간표 sch (실제 보드 펌웨어의 TimeSlotManager와 같은 형식, 시간은 0시부터 센 초)
            insert  {"c":"sch","o":"insert","m":이보드,"pi":출력,"start":32400,"end":61200,
                     "rm":"daily"}  또는  "rm":"weekly","dw":[1,3] (0 = 일요일, 요일마다 한 줄)
            list    {"c":"sch","o":"list","m":이보드,"pi":출력} → 한 줄씩
                    {"c":"sch","o":"list","pi":출력,"index":0,"start","end","rm","dw","pn":"1/2"}
            delete  {"c":"sch","o":"delete","m":이보드,"pi":출력,"sI":index}
            deleteAll {"c":"sch","o":"deleteAll","m":이보드,"pi":출력}
            insert, delete 뒤에는 실제 보드처럼 목록을 다시 보낸다.
            시간대에 들어가는 순간 출력을 켜고, 벗어나는 순간 끈다. 시각은 PC 시계 + tz(기본 한국 시간).
  센서 규칙 bs (실제 보드 펌웨어의 SensorManager와 같은 형식, ts = "temp" 또는 "humi")
            insert  {"c":"bs","o":"insert","m":이보드,"ts":"temp","tr":1,"tv":30,"d":지연초,"du":동작초,
                     "ps":[{"m":대상보드,"n":출력,"v":1}]}   tr 1 = 올라갈 때, 0 = 내려갈 때
            list    {"c":"bs","o":"list","m":이보드,"ts":"temp"} → 규칙마다 한 줄씩
                    {"c":"bs","o":"list","ts":"temp","tr","tv","d","du","sI","ps":[{"m","n","v"}]}
            delete  {"c":"bs","o":"delete","m":이보드,"ts":"temp","sI":번호}
            deleteAll {"c":"bs","o":"deleteAll","m":이보드,"ts":"temp"}
            보정    {"c":"bs","o":"cali","m":이보드,"ts":"temp","v":28.1}  지금 값이 v가 되도록 보정값을 정한다
            올라갈 때 = 값이 tv 아래에서 tv 이상이 되는 순간, 내려갈 때 = tv 위에서 tv 이하가 되는 순간.
            실제 보드처럼 처음 값은 -1로 보아서, 등록할 때 이미 tv 이상이면 "올라갈 때" 규칙이 곧바로 실행된다.
"""

import collections
import json
import random
import threading
import time

import paho.mqtt.client as mqtt

PROTOCOL_VERSION = 1
SIM_VERSION = "0.1.0"
MSG_MAX = 512             # 한 메시지 최대 길이 (바이트)
IGNORE_SEC = 5.0          # 접속 직후 받은 메시지는 무시 (서버에 남아 있던 옛 명령)
COALESCE_SEC = 0.2        # 이 시간 안의 여러 변화는 상태 한 번으로 묶음
HEARTBEAT_SEC = 60.0      # 바뀌지 않아도 이 간격으로 상태를 보냄
TEMP_STEP = 0.2           # 온도가 이만큼 바뀌면 상태를 보냄
CONTROL_CMDS = {"so", "st", "setTime", "bio", "bs", "sch"}   # MAC이 꼭 있어야 하는 명령
SENSORS = ("temp", "humi")
SENSOR_ALIAS = {"temperature": "temp", "humidity": "humi"}   # 실제 보드처럼 긴 이름도 받는다
MAX_RULES = 40            # 보드 하나에 넣을 수 있는 입력 규칙 수
MAX_SEC = 86400           # 지연, 동작 시간의 최대값 (초)
LIST_GAP_SEC = 0.2        # bio list 줄 사이 간격 (실제 보드와 같게)


def _is_int(x):
    return isinstance(x, int) and not isinstance(x, bool)


def _is_sec(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) and 0 <= x <= MAX_SEC


class VirtualBoard:
    def __init__(self, index, mac, group, server, port=1883, user=None, password=None,
                 kind="i2r-03", ports=4, log=None):
        self.index = index
        self.mac = mac
        self.group = group
        self.kind = kind
        self.ports = ports
        self.server = server
        self.port = port
        self.topic_in = f"i2r/{group}/in"
        self.topic_out = f"i2r/{group}/out"
        self.log = log or (lambda *a: None)
        self.hist_rx = collections.deque(maxlen=8)   # 이 보드가 받은 명령 (화면에 글자로)
        self.hist_tx = collections.deque(maxlen=8)   # 이 보드가 보낸 메시지

        self.inp = [0] * ports
        self.out = [0] * ports
        self.off_at = [0.0] * ports          # so의 d(유지 시간): 이 시각에 끔 (0 = 없음)
        self.temp = round(random.uniform(21.0, 25.0), 1)
        self.humi = round(random.uniform(35.0, 50.0), 1)
        self.tz = 9 * 3600
        self.rules = []                      # 입력 규칙 (bio): sI, port, tr, d, du, m, n, v, due
        self.next_si = 1
        self.slots = [[] for _ in range(ports)]   # 시간표 (sch): 출력마다 {start, end, rm, dw}
        self.sched_prev = [False] * ports         # 지난번에 시간표가 켜져 있었는지 (바뀔 때만 출력을 바꿈)
        self.cal = {"temp": 0.0, "humi": 0.0}     # 센서 보정값 (보내는 값 = 측정값 + 보정값)
        self.srules = []                          # 센서 규칙 (bs): sI, ts, tr, tv, d, du, m, n, v, last, due
        self.next_ssi = 0                         # 실제 보드처럼 0부터

        self.connected = False
        self.connected_at = 0.0
        self.last_sent_state = None
        self.last_sent_temp = None
        self.last_sent_humi = None
        self.last_state_time = 0.0
        self.dirty_since = 0.0               # 상태가 바뀌어 보내야 하는 때 (0 = 없음)
        self.pending_id = None               # 명령의 id를 상태에 붙여 돌려줌
        self.lock = threading.RLock()

        self.cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=mac)
        if user:
            self.cli.username_pw_set(user, password or "")
        self.cli.will_set(self.topic_out, json.dumps({"c": "lw", "fr": mac, "on": 0}), qos=0, retain=False)
        self.cli.on_connect = self._on_connect
        self.cli.on_disconnect = self._on_disconnect
        self.cli.on_message = self._on_message
        self.cli.reconnect_delay_set(min_delay=2, max_delay=30)

    # ---- 시작, 끝 ----
    def start(self):
        # keepalive 30초: 실제 보드(I2R_IoT)와 같게. 끊기면 서버가 약 45초 안에 유언(lw on:0)을 보낸다
        self.cli.connect_async(self.server, self.port, keepalive=30)
        self.cli.loop_start()

    def stop(self):
        if self.connected:
            # 정상 종료는 유언이 나가지 않으므로 직접 알린다
            self._send({"c": "lw", "on": 0})
            time.sleep(0.2)
        self.cli.disconnect()
        self.cli.loop_stop()

    # ---- 화면(사람)이 하는 일 ----
    def set_input(self, n, v):
        with self.lock:
            if 0 <= n < self.ports:
                new = 1 if v else 0
                changed = new != self.inp[n]
                self.inp[n] = new
                self._mark_dirty()
                if changed:
                    # 입력이 규칙의 트리거와 같아지면 지연 시간을 처음부터 센다 (실제 보드 setPendingExec와 같음)
                    now = time.time()
                    for r in self.rules:
                        if r["port"] == n and r["tr"] == new:
                            r["due"] = now + r["d"]

    def set_temp(self, value):
        with self.lock:
            self.temp = round(max(-40.0, min(85.0, value)), 1)

    def set_humi(self, value):
        with self.lock:
            self.humi = round(max(0.0, min(100.0, value)), 1)

    def sensor_value(self, ts):
        """보내는 센서 값 (측정값 + 보정값). 규칙도 이 값으로 판단한다."""
        raw = self.temp if ts == "temp" else self.humi
        return round(raw + self.cal[ts], 1)

    def snapshot(self):
        with self.lock:
            now = time.time()
            return {
                "index": self.index, "mac": self.mac, "kind": self.kind,
                "connected": self.connected, "in": list(self.inp), "out": list(self.out),
                "remain": [max(0, int(round(t - now))) if t else 0 for t in self.off_at],
                "rules": [{**{k: r[k] for k in ("sI", "port", "tr", "d", "du", "m", "n", "v")},
                           "wait": max(0, int(round(r["due"] - now))) if r["due"] else None}
                          for r in self.rules],
                "slots": [[dict(s) for s in pin] for pin in self.slots],
                "srules": [{**{k: r[k] for k in ("sI", "ts", "tr", "tv", "d", "du", "m", "n", "v")},
                            "wait": max(0, int(round(r["due"] - now))) if r["due"] else None}
                           for r in self.srules],
                "cal": dict(self.cal),
                "temp_out": self.sensor_value("temp"), "humi_out": self.sensor_value("humi"),
                "clock": time.strftime("%H:%M:%S", time.gmtime(now + self.tz)),
                "temp": self.temp, "humi": self.humi,
                "rx": list(self.hist_rx), "tx": list(self.hist_tx),
                "topic_in": self.topic_in, "topic_out": self.topic_out,
            }

    def _rx(self, text):
        self.hist_rx.appendleft({"t": time.strftime("%H:%M:%S"), "text": text})
        self.log(self, "rx", text)

    # ---- 주기 작업 (sim.py가 0.05초마다 부름) ----
    def tick(self):
        now = time.time()
        with self.lock:
            for n in range(self.ports):
                if self.off_at[n] and now >= self.off_at[n]:
                    self.off_at[n] = 0.0
                    if self.out[n]:
                        self.out[n] = 0
                        self._mark_dirty()
            for r in self.rules:
                if r["due"] and now >= r["due"]:
                    r["due"] = 0.0
                    self._run_rule(r)
            self._run_schedule(now)
            self._run_sensor_rules(now)
            if self.last_sent_temp is not None and abs(self.sensor_value("temp") - self.last_sent_temp) >= TEMP_STEP:
                self._mark_dirty()
            if self.last_sent_humi is not None and abs(self.sensor_value("humi") - self.last_sent_humi) >= 1.0:
                self._mark_dirty()
            if not self.connected:
                return
            if self.dirty_since and now - self.dirty_since >= COALESCE_SEC:
                self._send_state()
            elif now - self.last_state_time >= HEARTBEAT_SEC:
                self._send_state()

    # ---- MQTT ----
    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            self.log(self, "info", f"접속 실패: {reason_code}")
            return
        with self.lock:
            self.connected = True
            self.connected_at = time.time()
        client.subscribe(self.topic_in, qos=0)
        self.log(self, "info", f"접속됨 {self.server}:{self.port}")
        self._send({"c": "lw", "on": 1})
        self._send({"c": "hello", "t": self.kind, "pv": PROTOCOL_VERSION, "ver": SIM_VERSION,
                    "in": self.ports, "out": self.ports, "sensors": ["temp", "humi"], "sim": 1})
        with self.lock:
            self._send_state()

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        with self.lock:
            self.connected = False
        self.log(self, "info", f"끊김 ({reason_code})")

    def _on_message(self, client, userdata, msg):
        raw = msg.payload
        if time.time() - self.connected_at < IGNORE_SEC:
            return
        if len(raw) > MSG_MAX:
            self._err("?", "too_long", None)
            return
        try:
            j = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return                                  # JSON이 아니면 조용히 버림
        if not isinstance(j, dict):
            return
        if j.get("fr") == self.mac:
            return                                  # 내가 보낸 것이 되돌아온 것
        c = j.get("c")
        m = j.get("m")
        mid = j.get("id")
        if c in (None, "lw", "hello", "err"):      # 다른 보드의 알림은 명령이 아님
            return
        if c == "gs":
            if m in (None, "*", self.mac):
                self._rx(raw.decode("utf-8"))
                with self.lock:
                    self._send_state(mid)
            return
        if m is None:
            if c in CONTROL_CMDS:
                self._rx(raw.decode("utf-8"))
                self._err(c, "no_m", mid)
            return
        if m != "*" and m != self.mac:
            return                                  # 다른 보드 것: 조용히 무시
        self._rx(raw.decode("utf-8"))
        if c == "so":
            self._cmd_so(j, mid)
        elif c == "bio":
            self._cmd_bio(j, mid)
        elif c == "sch":
            self._cmd_sch(j, mid)
        elif c == "bs":
            self._cmd_bs(j, mid)
        elif c in ("st", "setTime"):
            tz = j.get("tz", j.get("tzOffset"))
            if isinstance(tz, (int, float)):
                self.tz = int(tz)
            with self.lock:
                self._send_state(mid)
        else:
            self._err(c, "unknown", mid)

    def _cmd_so(self, j, mid):
        n, v, d = j.get("n"), j.get("v"), j.get("d", 0)
        if not isinstance(n, int) or isinstance(n, bool) or not (0 <= n < self.ports):
            return self._err("so", "bad_n", mid)
        if v not in (0, 1, True, False):
            return self._err("so", "bad_v", mid)
        if not isinstance(d, (int, float)) or isinstance(d, bool) or d < 0 or d > 86400:
            return self._err("so", "bad_v", mid)
        with self.lock:
            self.out[n] = 1 if v else 0
            self.off_at[n] = time.time() + d if (v and d) else 0.0
            self.pending_id = mid
            self._send_state(mid)                   # 명령에는 바로 답한다 (묶지 않음)

    # ---- 입력 규칙 (bio) ----
    def _cmd_bio(self, j, mid):
        o, port = j.get("o"), j.get("n")
        if o not in ("insert", "list", "delete", "deleteAll"):
            return self._err("bio", "bad_o", mid)
        if not _is_int(port) or not (0 <= port < self.ports):
            return self._err("bio", "bad_n", mid)
        if o == "insert":
            self._bio_insert(j, port, mid)
        elif o == "list":
            with self.lock:
                lines = [self._bio_line(r) for r in self.rules if r["port"] == port]
            # 실제 보드처럼 규칙마다 한 줄씩, 0.2초 간격으로 보낸다 (받는 쪽이 놓치지 않게)
            threading.Thread(target=self._send_lines, args=(lines,), daemon=True).start()
        elif o == "delete":
            si = j.get("sI", j.get("si"))
            with self.lock:
                before = len(self.rules)
                self.rules = [r for r in self.rules if not (r["port"] == port and r["sI"] == si)]
                if len(self.rules) == before:
                    return self._err("bio", "bad_si", mid)
        else:
            with self.lock:
                self.rules = [r for r in self.rules if r["port"] != port]

    def _bio_insert(self, j, port, mid):
        tr, d, du, ps = j.get("tr"), j.get("d", 0), j.get("du", 0), j.get("ps")
        if tr not in (0, 1, True, False) or not _is_sec(d) or not _is_sec(du):
            return self._err("bio", "bad_v", mid)
        if not isinstance(ps, list) or not ps:
            return self._err("bio", "bad_v", mid)
        new = []
        for p in ps:
            if not isinstance(p, dict):
                return self._err("bio", "bad_v", mid)
            m, n, v = p.get("m") or self.mac, p.get("n"), p.get("v")
            if not isinstance(m, str) or not _is_int(n) or n < 0 or v not in (0, 1, True, False):
                return self._err("bio", "bad_v", mid)
            if m == self.mac and n >= self.ports:
                return self._err("bio", "bad_n", mid)
            new.append({"port": port, "tr": 1 if tr else 0, "d": d, "du": du,
                        "m": m, "n": n, "v": 1 if v else 0, "due": 0.0})
        with self.lock:
            if len(self.rules) + len(new) > MAX_RULES:
                return self._err("bio", "full", mid)
            for r in new:
                r["sI"] = self.next_si
                self.next_si += 1
                self.rules.append(r)
        # 실제 보드는 insert에 답하지 않는다. 화면이 list로 다시 물어본다.

    def _bio_line(self, r):
        return {"c": "bio", "o": "list", "m": self.mac, "n": r["port"],
                "ps": [{"tr": r["tr"], "d": r["d"], "du": r["du"], "m": r["m"],
                        "n": r["n"], "v": r["v"], "sI": r["sI"]}]}

    def _send_lines(self, lines):
        for i, line in enumerate(lines):
            if i:
                time.sleep(LIST_GAP_SEC)
            self._send(line)

    def _run_rule(self, r, quiet=False):
        """규칙 실행 (lock을 잡은 채로 부름). 대상이 나면 출력을 바꾸고, 다른 보드면 so를 보낸다."""
        if not quiet:
            self.log(self, "info", f"규칙 {r['sI']} 실행: IN{r['port'] + 1} → {r['m']} OUT{r['n'] + 1} "
                                   f"{'켜짐' if r['v'] else '꺼짐'}")
        if r["m"] == self.mac:
            if r["n"] < self.ports:
                self.out[r["n"]] = r["v"]
                self.off_at[r["n"]] = time.time() + r["du"] if (r["v"] and r["du"]) else 0.0
                self._mark_dirty()
        else:
            cmd = {"c": "so", "m": r["m"], "n": r["n"], "v": r["v"], "sI": r["sI"]}
            if r["du"]:
                cmd["d"] = r["du"]
            self._send(cmd, self.topic_in)

    # ---- 시간표 (sch) ----
    def _cmd_sch(self, j, mid):
        o, pi = j.get("o"), j.get("pi")
        if o not in ("insert", "list", "delete", "deleteAll"):
            return self._err("sch", "bad_o", mid)
        if not _is_int(pi) or not (0 <= pi < self.ports):
            return self._err("sch", "bad_n", mid)
        if o == "insert":
            start, end, rm, dw = j.get("start"), j.get("end"), j.get("rm", "daily"), j.get("dw", 0)
            day_sec = lambda x: _is_int(x) and 0 <= x < 86400
            if not day_sec(start) or not day_sec(end) or rm not in ("daily", "weekly"):
                return self._err("sch", "bad_v", mid)
            if rm == "weekly":
                days = dw if isinstance(dw, list) else [dw]
                if not days or not all(_is_int(d) and 0 <= d <= 6 for d in days):
                    return self._err("sch", "bad_v", mid)
                new = [{"start": start, "end": end, "rm": "weekly", "dw": d} for d in days]
            else:
                new = [{"start": start, "end": end, "rm": "daily", "dw": -1}]
            with self.lock:
                if sum(len(p) for p in self.slots) + len(new) > MAX_RULES:
                    return self._err("sch", "full", mid)
                self.slots[pi].extend(new)
            self._sch_send_list(pi)
        elif o == "list":
            self._sch_send_list(pi)
        elif o == "delete":
            si = j.get("sI", j.get("si"))
            with self.lock:
                if not _is_int(si) or not (0 <= si < len(self.slots[pi])):
                    return self._err("sch", "bad_si", mid)
                del self.slots[pi][si]
            self._sch_send_list(pi)
        else:
            with self.lock:
                self.slots[pi] = []

    def _sch_send_list(self, pi):
        with self.lock:
            total = len(self.slots[pi])
            lines = [{"c": "sch", "o": "list", "m": self.mac, "pi": pi, "index": i, "start": s["start"],
                      "end": s["end"], "rm": s["rm"], "dw": s["dw"], "pn": f"{i + 1}/{total}"}
                     for i, s in enumerate(self.slots[pi])]
        threading.Thread(target=self._send_lines, args=(lines,), daemon=True).start()

    def _run_schedule(self, now):
        """시간표: 출력마다 지금 시간대 안인지 보고, 바뀔 때만 출력을 바꾼다 (lock을 잡은 채로 부름)."""
        t = time.gmtime(now + self.tz)
        sec = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec
        wday = (t.tm_wday + 1) % 7           # 파이썬은 월요일이 0, 보드는 일요일이 0
        for pi in range(self.ports):
            on = False
            for s in self.slots[pi]:
                if s["rm"] == "weekly" and s["dw"] != wday:
                    continue
                if s["start"] < s["end"]:
                    inside = s["start"] <= sec <= s["end"]
                else:                        # 자정을 넘는 시간대 (예: 22시 ~ 6시)
                    inside = sec >= s["start"] or sec <= s["end"]
                if inside:
                    on = True
                    break
            if on != self.sched_prev[pi]:
                self.sched_prev[pi] = on
                self.log(self, "info", f"시간표: OUT{pi + 1} {'켜짐' if on else '꺼짐'}")
                self.out[pi] = 1 if on else 0
                self.off_at[pi] = 0.0
                self._mark_dirty()

    # ---- 센서 규칙, 보정 (bs) ----
    def _cmd_bs(self, j, mid):
        o = j.get("o")
        if o == "calibration":
            o = "cali"
        ts = SENSOR_ALIAS.get(j.get("ts"), j.get("ts"))
        if o not in ("insert", "list", "delete", "deleteAll", "cali"):
            return self._err("bs", "bad_o", mid)
        if ts not in SENSORS:
            return self._err("bs", "bad_ts", mid)
        if o == "insert":
            self._bs_insert(j, ts, mid)
        elif o == "list":
            with self.lock:
                lines = [self._bs_line(r) for r in self.srules if r["ts"] == ts]
            threading.Thread(target=self._send_lines, args=(lines,), daemon=True).start()
        elif o == "delete":
            si = j.get("sI", j.get("si"))
            with self.lock:
                before = len(self.srules)
                self.srules = [r for r in self.srules if r["sI"] != si]
                if len(self.srules) == before:
                    return self._err("bs", "bad_si", mid)
        elif o == "deleteAll":
            with self.lock:
                self.srules = [r for r in self.srules if r["ts"] != ts]
        else:
            v = j.get("v")
            if not isinstance(v, (int, float)) or isinstance(v, bool) or not (-40 <= v <= 100):
                return self._err("bs", "bad_v", mid)
            with self.lock:
                raw = self.temp if ts == "temp" else self.humi
                self.cal[ts] = round(v - raw, 1)
                self.log(self, "info", f"보정: {ts} 측정 {raw} → 보내는 값 {v} (보정 {self.cal[ts]:+})")
                self._send_state(mid)            # 실제 보드처럼 보정 뒤 바로 상태를 보낸다

    def _bs_insert(self, j, ts, mid):
        tr, tv, d, du, ps = j.get("tr"), j.get("tv"), j.get("d", 0), j.get("du", 0), j.get("ps")
        if tr not in (0, 1, True, False) or not _is_sec(d) or not _is_sec(du):
            return self._err("bs", "bad_v", mid)
        if not isinstance(tv, (int, float)) or isinstance(tv, bool) or not (-40 <= tv <= 100):
            return self._err("bs", "bad_v", mid)
        if not isinstance(ps, list) or len(ps) != 1 or not isinstance(ps[0], dict):
            return self._err("bs", "bad_v", mid)       # 실제 보드도 ps는 하나만 받는다
        p = ps[0]
        m, n, v = p.get("m") or self.mac, p.get("n"), p.get("v")
        if not isinstance(m, str) or not _is_int(n) or n < 0 or v not in (0, 1, True, False):
            return self._err("bs", "bad_v", mid)
        if m == self.mac and n >= self.ports:
            return self._err("bs", "bad_n", mid)
        with self.lock:
            if len(self.srules) >= MAX_RULES:
                return self._err("bs", "full", mid)
            self.srules.append({"sI": self.next_ssi, "ts": ts, "tr": 1 if tr else 0, "tv": tv, "d": d, "du": du,
                                "m": m, "n": n, "v": 1 if v else 0, "last": -1.0, "due": 0.0})
            self.next_ssi += 1

    def _bs_line(self, r):
        return {"c": "bs", "o": "list", "m": self.mac, "ts": r["ts"], "tr": r["tr"], "tv": r["tv"],
                "du": r["du"], "d": r["d"], "sI": r["sI"], "ps": [{"m": r["m"], "n": r["n"], "v": r["v"]}]}

    def _run_sensor_rules(self, now):
        """센서 규칙: 값이 기준을 넘는 순간만 실행한다 (lock을 잡은 채로 부름)."""
        for r in self.srules:
            if r["due"] and now >= r["due"]:
                r["due"] = 0.0
                self._fire_sensor_rule(r)
            cur = self.sensor_value(r["ts"])
            up = r["tr"] and r["last"] < r["tv"] <= cur
            down = not r["tr"] and r["last"] > r["tv"] >= cur
            if up or down:
                if r["d"]:
                    r["due"] = now + r["d"]      # 기다리는 중에 다시 넘으면 처음부터 센다
                else:
                    self._fire_sensor_rule(r)
            r["last"] = cur

    def _fire_sensor_rule(self, r):
        name = "온도" if r["ts"] == "temp" else "습도"
        self.log(self, "info", f"센서 규칙 {r['sI']}: {name} {'≥' if r['tr'] else '≤'} {r['tv']} → "
                               f"{r['m']} OUT{r['n'] + 1} {'켜짐' if r['v'] else '꺼짐'}")
        self._run_rule(r, quiet=True)

    # ---- 보내기 ----
    def _mark_dirty(self):
        if not self.dirty_since:
            self.dirty_since = time.time()

    def _state(self):
        s = {"m": self.mac, "in": list(self.inp), "out": list(self.out), "t": self.kind,
             "e": self.group, "temp": self.sensor_value("temp"), "humi": self.sensor_value("humi")}
        return s

    def _send_state(self, mid=None):
        s = self._state()
        if mid is not None:
            s["id"] = mid
        self._send(s)
        self.last_sent_state = (tuple(self.inp), tuple(self.out))
        self.last_sent_temp = self.sensor_value("temp")
        self.last_sent_humi = self.sensor_value("humi")
        self.last_state_time = time.time()
        self.dirty_since = 0.0

    def _err(self, re_cmd, code, mid):
        e = {"c": "err", "m": self.mac, "re": re_cmd, "code": code}
        if mid is not None:
            e["id"] = mid
        self._send(e)

    def _send(self, j, topic=None):
        j["fr"] = self.mac
        text = json.dumps(j, ensure_ascii=False, separators=(",", ":"))
        if not self.cli.is_connected():
            return
        self.cli.publish(topic or self.topic_out, text, qos=0, retain=False)
        self.hist_tx.appendleft({"t": time.strftime("%H:%M:%S"), "text": text})
        self.log(self, "tx", text)
