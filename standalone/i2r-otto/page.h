// page.h: Otto standalone 웹 화면 (HTML, CSS, JavaScript 한 파일)
// 보드가 http://<주소>/ 로 보낸다. 명령은 POST /cmd, 답과 알림은 GET /ev, 내 음악 저장은 POST /upload
// .ino 안에 긴 HTML 을 두면 Arduino 가 JavaScript 를 C++ 로 잘못 읽어 오류가 나서 따로 둔다.
// 책읽기의 글(이솝우화 4편)은 i2r.link 웹의 ottoBooks.ts 와 같다.
#pragma once
const char PAGE_HTML[] PROGMEM = R"PAGE(<!DOCTYPE html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Otto 로봇</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,"Malgun Gothic",sans-serif;background:#f1f5f9;color:#1e293b}
.wrap{max-width:760px;margin:0 auto;padding:14px}
header{display:flex;justify-content:space-between;align-items:flex-end;flex-wrap:wrap;gap:6px;margin:6px 4px 12px}
h1{margin:0;font-size:1.6rem}
.sub{font-size:.82rem;color:#64748b}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;background:#ef4444;margin-right:4px}
.dot.on{background:#22c55e}
.card{background:#fff;border-radius:18px;padding:14px;box-shadow:0 1px 4px #0001;margin-bottom:12px}
.tabs{display:inline-flex;background:#e2e8f0;border-radius:12px;padding:4px;gap:4px;margin-bottom:10px}
.tabs button{border:0;background:none;padding:8px 14px;border-radius:9px;font-weight:700;color:#475569;cursor:pointer}
.tabs button.sel{background:#3b82f6;color:#fff}
.chips{display:flex;flex-wrap:wrap;gap:8px;padding-bottom:12px;border-bottom:1px solid #e2e8f0}
.chip{border:1px solid #cbd5e1;background:#fff;border-radius:999px;padding:8px 14px;font-weight:700;font-size:.88rem;cursor:pointer;color:#334155}
.chip::before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;background:#cbd5e1;margin-right:6px;vertical-align:middle}
.chip.on{border-color:#3b82f6;background:#eff6ff;color:#1d4ed8}
.chip.on::before{background:#3b82f6}
.chk{font-size:.82rem;display:flex;align-items:center;gap:4px;color:#475569}
#pad{display:block;width:min(300px,82vw);margin:18px auto 6px;touch-action:none;user-select:none;-webkit-user-select:none}
.center{text-align:center}
.help{font-size:.8rem;color:#64748b;text-align:center;margin:6px 0}
.btn{border:0;border-radius:10px;padding:8px 14px;font-weight:700;cursor:pointer;background:#3b82f6;color:#fff;margin:3px}
.btn.gray{background:#e2e8f0;color:#334155}.btn.red{background:#ef4444}.btn.green{background:#16a34a}
.btn:disabled{opacity:.5}
.panel{display:none}.panel.show{display:block}
.panel h2{font-size:1.1rem;margin:0 0 8px}
.row{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin:6px 0}
.row label{min-width:86px;font-size:.88rem;color:#475569}
input[type=range]{flex:1;min-width:140px}
input,select,textarea{font-size:.95rem;border:1px solid #cbd5e1;border-radius:8px;padding:6px}
textarea{width:100%}
.num{width:52px;text-align:right;font-weight:700;font-variant-numeric:tabular-nums}
.msg{font-size:.85rem;color:#b91c1c;min-height:1em}
.ok{color:#15803d}
.log{max-height:240px;overflow:auto;background:#f8fafc;border-radius:10px;padding:8px;font-size:.9rem}
.log .u{color:#1d4ed8;margin-top:6px}.log .r{color:#0f172a}
table{width:100%;border-collapse:collapse;font-size:.85rem}td{padding:4px 2px;border-bottom:1px solid #f1f5f9}
.days label{min-width:0;font-size:.8rem}
.bar{height:8px;background:#e2e8f0;border-radius:4px;overflow:hidden}.bar div{height:100%;width:0;background:#3b82f6}
.step{border-left:4px solid #3b82f6;padding-left:10px;margin:12px 0}
.step b{display:block;margin-bottom:4px}
</style></head><body><div class="wrap">

<header>
  <div><h1>로봇</h1><div class="sub" id="info">MAC: -</div></div>
  <div class="sub"><span class="dot" id="conn"></span><span id="connTx">연결 중</span> · 보드 시각 <b id="bt">--:--</b>
    <button class="btn gray" style="padding:4px 8px" onclick="sendTime(true)">시간 맞추기</button></div>
</header>

<div class="card">
  <div class="tabs" id="tabs">
    <button data-m="walk" class="sel">걷기 (walk)</button><button data-m="roll">굴리기 (roll)</button><button data-m="slide">미끄러짐 (slide)</button>
  </div>
  <div class="chips">
    <button class="chip" id="bDance" onclick="startDance()">댄스 (dance)</button>
    <label class="chk"><input type="checkbox" id="danceMusic" checked>댄스할 때 내 음악 재생</label>
    <button class="chip" id="bPatrol" onclick="startPatrol()">순찰</button>
    <button class="chip" data-p="ai">AI 대화</button>
    <button class="chip" data-p="radio">라디오</button>
    <button class="chip" data-p="speak">말하기</button>
    <button class="chip" data-p="dict">받아쓰기</button>
    <button class="chip" id="bClock" onclick="toggleClock()">시계</button>
    <button class="chip" data-p="book">책읽기</button>
    <button class="chip" data-p="sched">예약</button>
    <button class="chip" data-p="music">내 음악</button>
    <button class="chip" data-p="light">조명</button>
    <button class="chip" id="bLedDemo" onclick="toggleLedDemo()">LED 데모</button>
    <button class="chip" id="bDemo" onclick="startDemo()">OTTO 데모</button>
    <button class="chip" data-p="cal">로봇 맞추기</button>
  </div>

  <svg id="pad" viewBox="-100 -100 200 200">
    <defs><radialGradient id="g"><stop offset="0" stop-color="#fff"/><stop offset="1" stop-color="#e2e8f0"/></radialGradient></defs>
    <circle r="97" fill="url(#g)" stroke="#94a3b8" stroke-width="2"/>
    <g id="sectors"></g>
    <line x1="-68" y1="-68" x2="-21" y2="-21" stroke="#cbd5e1"/><line x1="68" y1="-68" x2="21" y2="-21" stroke="#cbd5e1"/>
    <line x1="-68" y1="68" x2="-21" y2="21" stroke="#cbd5e1"/><line x1="68" y1="68" x2="21" y2="21" stroke="#cbd5e1"/>
    <circle r="62" fill="none" stroke="#cbd5e1" stroke-dasharray="3 4" id="midRing"/>
    <text x="0" y="-80" text-anchor="middle" font-size="7" fill="#64748b" class="spd">빠르게 200</text>
    <text x="0" y="-41" text-anchor="middle" font-size="7" fill="#64748b" class="spd">느리게 40</text>
    <text x="0" y="45" text-anchor="middle" font-size="7" fill="#64748b" class="spd">느리게 40</text>
    <text x="0" y="86" text-anchor="middle" font-size="7" fill="#64748b" class="spd">빠르게 200</text>
    <text x="0" y="-55" text-anchor="middle" font-size="13" font-weight="700">전진</text>
    <text x="0" y="65" text-anchor="middle" font-size="13" font-weight="700">후진</text>
    <text x="-60" y="5" text-anchor="middle" font-size="12" font-weight="700">좌회전</text>
    <text x="60" y="5" text-anchor="middle" font-size="12" font-weight="700">우회전</text>
    <circle r="26" fill="#fee2e2" stroke="#fca5a5" stroke-width="3" id="stopC"/>
    <text x="0" y="5" text-anchor="middle" font-size="14" font-weight="800" fill="#dc2626">정지</text>
    <circle r="5" fill="#3b82f6" id="touch" style="display:none"/>
    <text id="spTx" x="0" y="-8" text-anchor="middle" font-size="9" fill="#1d4ed8"></text>
  </svg>
  <div class="center"><button class="btn gray" onclick="stopAll()">● 정지</button> <span class="sub" id="nowAct"></span></div>
  <p class="help">구역을 누르면 정지할 때까지 계속 움직입니다. 가운데 [정지] 원을 누르면 멈춥니다.<br>
  전진·후진은 가운데에 가까울수록 느리게(40), 바깥일수록 빠르게(200). 누른 채 끌어 속도를 고르고 손을 떼면 그 속도로 바뀝니다.</p>
  <div class="msg" id="actMsg"></div>
</div>

<!-- AI 대화 -->
<div class="card panel" id="p_ai"><h2>AI 대화</h2>
  <div class="row"><button class="btn" id="aiBtn" onclick="toggleAi()">대화 켜기</button><span class="sub">로봇 마이크로 듣고 스피커로 대답합니다 (인터넷 필요)</span></div>
  <div class="msg" id="aiMsg"></div>
  <div class="log" id="chatLog"><span class="sub">대화 내용이 여기에 나옵니다.</span></div>
  <div class="row" style="margin-top:10px"><label>Gemini API 키</label><input type="password" id="keyIn" placeholder="키를 붙여 넣기" style="flex:1">
    <button class="btn" onclick="saveKey()">저장</button><button class="btn gray" onclick="delKey()">삭제</button></div>
  <div class="sub" id="keyMsg">키 상태 확인 중...</div>
</div>

<!-- 라디오 -->
<div class="card panel" id="p_radio"><h2>라디오</h2>
  <div id="stations"></div>
  <div class="row"><button class="btn red" onclick="cmd({c:'rd',a:'stop'})">라디오 끄기</button><span class="sub" id="rdState"></span></div>
  <div class="row"><label>볼륨</label><input type="range" min="0" max="21" id="rdVol"><span class="num" id="rdVolTx"></span></div>
  <div class="sub" id="rdTitle"></div><div class="msg" id="rdMsg"></div>
</div>

<!-- 말하기 -->
<div class="card panel" id="p_speak"><h2>말하기</h2>
  <textarea id="skText" rows="2" maxlength="80">사랑아 생일 축하해</textarea>
  <div class="row"><label>쉬는 시간(초)</label><input type="number" id="skWait" value="5" min="0" max="3600" style="width:80px">
    <label>반복 횟수</label><input type="number" id="skCount" value="0" min="0" max="1000" style="width:80px"><span class="sub">0 = 멈출 때까지</span></div>
  <div class="row"><label>볼륨</label><input type="range" min="0" max="21" value="18" id="skVol"><span class="num" id="skVolTx">18</span></div>
  <div class="row"><button class="btn" onclick="startSpeak()">말하기 시작</button><button class="btn red" onclick="stopSpeak()">멈춤</button><span class="sub" id="skState"></span></div>
  <div class="msg" id="skMsg"></div>
</div>

<!-- 받아쓰기 -->
<div class="card panel" id="p_dict"><h2>받아쓰기</h2>
  <p class="sub">내가 말하면 이 휴대폰(PC)의 브라우저가 글자로 바꿔 보여 줍니다. 로봇 LED 가 🟢 녹색이면 말하고, 🔴 빨간색이면 글자로 바꾸는 중입니다.</p>
  <div class="row"><button class="btn green" id="dictBtn" onclick="toggleDict()">받아쓰기 시작</button><span class="sub" id="dictState"></span></div>
  <div class="msg" id="dictMsg"></div>
  <div class="sub" id="dictInterim"></div>
  <div class="log" id="dictLog"></div>
</div>

<!-- 책읽기 -->
<div class="card panel" id="p_book"><h2>책읽기</h2>
  <div class="row"><label>책</label><select id="bookSel"></select></div>
  <textarea id="bookText" rows="6" maxlength="5000"></textarea>
  <div class="row"><button class="btn" onclick="startBook()">읽기 시작</button><button class="btn red" onclick="stopBook(true)">멈춤</button><span class="sub" id="bookState"></span></div>
  <div class="msg" id="bookMsg"></div>
</div>

<!-- 예약 -->
<div class="card panel" id="p_sched"><h2>예약</h2>
  <p class="sub">정해진 시각에 조명, 라디오, 내 음악을 켜고 끕니다. 최대 8개. 보드 시각: <b id="schTime">--:--</b></p>
  <table><tbody id="schRows"></tbody></table>
  <div class="row"><button class="btn gray" onclick="addSched()">+ 예약 추가</button><button class="btn" onclick="saveSched()">보드에 저장</button></div>
  <div class="msg" id="schMsg"></div>
</div>

<!-- 내 음악 -->
<div class="card panel" id="p_music"><h2>내 음악</h2>
  <table><tbody id="muRows"></tbody></table>
  <div class="row"><button class="btn red" onclick="cmd({c:'mu',a:'stop'})">정지</button><label>볼륨</label><input type="range" min="0" max="21" id="muVol"><span class="num" id="muVolTx"></span></div>
  <div class="sub" id="muState"></div>
  <div class="step"><b>mp3 를 보드에 저장 (최대 6곡)</b>
    <div class="row"><input type="file" id="muFile" accept=".mp3,audio/mpeg"></div>
    <div class="row"><label>제목</label><input id="muTitle" maxlength="60" style="flex:1"><button class="btn" onclick="uploadMusic()">보드에 저장</button></div>
    <div class="bar"><div id="muBar"></div></div><div class="sub" id="muUp"></div>
  </div>
  <div class="msg" id="muMsg"></div>
</div>

<!-- 조명 -->
<div class="card panel" id="p_light"><h2>조명</h2>
  <div class="row"><label>색</label><input type="color" id="lgColor" value="#ff0000"></div>
  <div class="row"><label>밝기</label><input type="range" min="1" max="100" value="50" id="lgPct"><span class="num" id="lgPctTx">50%</span></div>
  <div class="row"><button class="btn" onclick="lightOn()">켜기</button><button class="btn gray" onclick="cmd({c:'ol',ef:'off'})">끄기</button><span class="sub" id="lgState"></span></div>
</div>

<!-- 로봇 맞추기 -->
<div class="card panel" id="p_cal"><h2>로봇 맞추기</h2>
  <p class="sub">값을 바꾸면 바로 로봇에 반영됩니다. 다 맞추면 [저장]을 눌러 보드에 기록하세요.</p>
  <div class="step"><b>1. 조립 자세</b><button class="btn" onclick="cal({a:'p0'})">0도 / 180도 (다리 끼우기)</button><button class="btn" onclick="cal({a:'p90'})">90도 (가운데)</button></div>
  <div class="step"><b>2. 다리 수직</b><div id="cal2"></div></div>
  <div class="step"><b>3. 굴리기 시험 (두 발 속도)</b><div id="cal3"></div>
    <button class="btn" onclick="testMove('rf',3)">앞으로 굴리기 3초</button></div>
  <div class="step"><b>4. 걷기 시험 (발을 들 때 기울기)</b><div id="cal4"></div>
    <button class="btn" onclick="testMove('wf',4)">걷기 4초</button></div>
  <div class="step"><b>5. 댄스 시험 (한 발로 설 때 엉덩이 각도)</b><div id="cal5"></div>
    <button class="btn" onclick="testMove('dl',0)">왼발 댄스</button><button class="btn" onclick="testMove('dr',0)">오른발 댄스</button><button class="btn gray" onclick="stopAll()">정지</button></div>
  <div class="row"><button class="btn green" onclick="cal({a:'save'})">저장 (보드)</button><button class="btn gray" onclick="copyCal()">값 복사</button>
    <button class="btn red" onclick="if(confirm('저장한 값을 지우고 기본값으로 되돌릴까요?'))cal({a:'clear'})">기본값으로</button><span class="sub" id="calSaved"></span></div>
  <div class="msg" id="calMsg"></div>
</div>

<p class="help" id="foot"></p>
</div>
<script>
const $ = (s) => document.querySelector(s);
const BOOKS = [{"title":"토끼와 거북이","text":"옛날에 토끼와 거북이가 살았어요. 토끼는 달리기를 아주 잘했어요. 그래서 거북이를 늘 놀렸어요. \"거북아, 너는 왜 그렇게 느리니?\" 화가 난 거북이가 말했어요. \"그럼 우리 달리기 시합을 하자!\" 시합이 시작되었어요. 토끼는 쌩 하고 달려가서 금방 거북이가 보이지 않게 되었어요. 토끼는 뒤를 돌아보며 웃었어요. \"거북이는 아직 멀었네. 나는 낮잠이나 한숨 자야지.\" 토끼는 나무 그늘에서 쿨쿨 잠이 들었어요. 그동안 거북이는 쉬지 않고 한 걸음 한 걸음 걸었어요. 토끼가 눈을 떴을 때, 거북이는 결승선에 거의 다 와 있었어요. 토끼는 놀라서 힘껏 달렸지만 너무 늦었어요. 이긴 것은 거북이였어요. 꾸준히 노력하는 사람이 결국 이긴답니다."},{"title":"개미와 베짱이","text":"더운 여름날, 개미들은 땀을 뻘뻘 흘리며 먹이를 모았어요. 그런데 베짱이는 나무 그늘에서 노래만 불렀어요. \"개미야, 같이 놀자. 노래가 얼마나 즐거운데!\" 개미가 대답했어요. \"겨울에 먹을 음식을 모아야 해. 너도 준비하렴.\" 베짱이는 웃기만 했어요. 곧 찬바람이 불고 눈이 내리기 시작했어요. 베짱이는 먹을 것이 하나도 없어서 배가 고프고 몸이 떨렸어요. 그래서 개미네 집을 찾아갔어요. \"개미야, 먹을 것을 조금만 나누어 주겠니?\" 개미는 베짱이를 따뜻하게 맞아 주었어요. 베짱이는 눈물을 흘리며 말했어요. \"미안해. 이제는 나도 열심히 준비할게.\" 미리 준비하는 사람은 어려운 때에도 걱정이 없답니다."},{"title":"양치기 소년","text":"산 아래 마을에 양을 돌보는 소년이 있었어요. 소년은 심심해서 장난을 치기로 했어요. \"늑대가 나타났어요! 도와주세요!\" 마을 사람들은 깜짝 놀라 몽둥이를 들고 달려왔어요. 하지만 늑대는 없었어요. 소년은 깔깔 웃었어요. 사람들은 화가 나서 돌아갔어요. 다음 날 소년은 또 거짓말을 했어요. 사람들은 또 속아서 달려왔어요. 며칠 뒤, 진짜 늑대가 나타났어요! 소년은 목이 터져라 소리쳤어요. \"늑대예요! 진짜예요! 도와주세요!\" 그러나 마을 사람들은 또 장난이라고 생각하고 오지 않았어요. 양들은 모두 늑대에게 잡히고 말았어요. 소년은 엉엉 울며 후회했어요. 거짓말을 자주 하면 진짜 말을 해도 아무도 믿어 주지 않는답니다."},{"title":"여우와 신 포도","text":"배고픈 여우가 길을 가다가 높은 포도나무를 보았어요. 가지마다 잘 익은 포도가 주렁주렁 달려 있었어요. \"야, 맛있겠다!\" 여우는 포도를 따려고 펄쩍 뛰었어요. 하지만 손이 닿지 않았어요. 여우는 몇 번이고 힘껏 뛰어올랐지만 포도는 너무 높이 달려 있었어요. 마침내 여우는 지쳐서 주저앉았어요. 그러고는 고개를 돌리며 말했어요. \"흥, 저 포도는 어차피 시어서 맛이 없을 거야.\" 그리고 터벅터벅 걸어갔어요. 하지 못한 일을 두고 핑계를 대는 것은 좋은 모습이 아니랍니다."}];
const STATIONS = ['Classic Praha (체코 클래식)', 'TBN 부산교통방송 (트로트/7080 가요)', 'Power 181 (미국 Top 40 팝송)',
  'ENERGY K-Pop (케이팝)', 'YourClassical Lullabies (자장가 클래식)', 'TBN 경인교통방송 (트로트/7080 가요)'];
const ERR = { busy: 'AI 대화 중에는 움직이지 않습니다. 먼저 대화를 끄세요.', no_key: 'Gemini API 키를 먼저 저장하세요.',
  no_psram: 'PSRAM 설정이 꺼져 있습니다 (보드 설정 OPI PSRAM).', no_mem: '메모리가 부족합니다.', no_wifi: '인터넷(Wi-Fi)이 필요합니다.',
  connect_fail: '방송국에 연결하지 못했습니다. 다른 방송국을 골라 보세요.', stream_fail: '방송이 끊겼습니다.',
  full: '6곡이 가득 찼습니다. 곡을 지운 뒤 저장하세요.', no_space: '보드 저장 공간이 부족합니다.', no_song: '없는 곡입니다.',
  open_fail: '파일을 만들지 못했습니다.', write_fail: '쓰는 중 오류가 났습니다.', timeout: '저장이 중간에 끊겼습니다.',
  bad_param: '값이 맞지 않습니다.', bad_action: '모르는 명령입니다.', nvs_fail: '보드에 저장하지 못했습니다.', unknown: '모르는 명령입니다.' };
const errText = (e) => ERR[e] || e || '';

// ───────── 보드와 이야기하기 ─────────
// 명령: POST /cmd 에 JSON 한 줄.  답과 알림: GET /ev 를 0.5초마다 (새 것만)
async function cmd(o) {
  try {
    const r = await fetch('/cmd', { method: 'POST', body: JSON.stringify(o) });
    const j = await r.json();
    if (!j.ok) $('#actMsg').textContent = errText(j.err);
    return j;
  } catch (e) { setConn(false); return { ok: 0, err: 'net' }; }
}
let evSeq = null, timeSent = false, online = false;
function setConn(on) {
  online = on; $('#conn').className = 'dot' + (on ? ' on' : '');
  $('#connTx').textContent = on ? '연결됨' : '연결 끊김';
}
async function poll() {
  try {
    const r = await fetch('/ev' + (evSeq === null ? '' : '?s=' + evSeq));
    const j = await r.json();
    setConn(true);
    if (evSeq !== null) j.ev.forEach(onEv);
    evSeq = j.s;
    showAct(j.a);
    $('#bt').textContent = j.tm || '--:--'; $('#schTime').textContent = j.tm || '--:--';
    if (!j.ts && !timeSent) sendTime(false);  // 보드 시계가 맞지 않았으면 이 기기의 시각을 보낸다
  } catch (e) { setConn(false); }
  setTimeout(poll, 500);
}
// st: 이 기기(휴대폰, PC)의 현지 시각과 시간대를 보드에 보낸다 (인터넷이 없어도 시계, 예약이 돈다)
function sendTime(show) {
  timeSent = true;
  const tz = -new Date().getTimezoneOffset() * 60;
  cmd({ c: 'st', epoch: Math.floor(Date.now() / 1000) + tz, tz }).then((j) => { if (show && j.ok) $('#actMsg').textContent = ''; });
}

// 보드가 보낸 답과 알림
function onEv(m) {
  switch (m.c) {
    case 'ot': if (!m.ok) $('#actMsg').textContent = errText(m.err); else $('#actMsg').textContent = ''; break;
    case 'lm': aiOn = !!m.v && !!m.ok; showAi(); $('#aiMsg').textContent = m.ok ? '' : errText(m.err); break;
    case 'lr': addChat(m.us, m.tx); break;
    case 'ak': $('#keyMsg').textContent = m.ok ? (m.has ? '✅ 보드에 키가 저장되어 있습니다' + (m.kv === 0 ? ' (Gemini 가 거부한 키입니다)' : '') : '저장된 키가 없습니다')
      : errText(m.err); break;
    case 'rd': onRadio(m); break;
    case 'sk': onSpeak(m); break;
    case 'mu': onMusic(m); break;
    case 'ol': onLight(m); break;
    case 'ck': clockOn = m.a === 'on'; $('#bClock').classList.toggle('on', clockOn); break;
    case 'os': onSched(m); break;
    case 'oc': onCal(m); break;
  }
}

// ───────── 조종판 ─────────
const CODES = { walk: { f: 'wf', b: 'wb', l: 'wl', r: 'wr' }, roll: { f: 'rf', b: 'rb', l: 'rl', r: 'rr' }, slide: { f: 'sf', b: 'sb', l: 'sl', r: 'sr' } };
const SPEED_CODES = ['wf', 'wb', 'rf', 'rb'];  // 속도(40~200%)를 쓰는 동작
const NAMES = { wf: '걷기 전진', wb: '걷기 후진', wl: '걷기 좌회전', wr: '걷기 우회전', rf: '굴리기 전진', rb: '굴리기 후진', rl: '굴리기 좌회전',
  rr: '굴리기 우회전', sf: '미끄러짐 전진', sb: '미끄러짐 후진', sl: '미끄러짐 좌회전', sr: '미끄러짐 우회전', dn: '댄스', dl: '왼발 댄스',
  dr: '오른발 댄스', pt: '순찰', od: 'OTTO 데모' };
let mode = 'walk', pressed = null, curAct = '';
document.querySelectorAll('#tabs button').forEach((b) => b.onclick = () => {
  mode = b.dataset.m;
  document.querySelectorAll('#tabs button').forEach((x) => x.classList.toggle('sel', x === b));
  document.querySelectorAll('.spd').forEach((t) => t.style.display = mode === 'slide' ? 'none' : '');
  $('#midRing').style.display = mode === 'slide' ? 'none' : '';
});
function showAct(a) {
  curAct = a || '';
  $('#nowAct').textContent = a ? '지금: ' + (NAMES[a] || a) : '';
  $('#bDance').classList.toggle('on', a === 'dn'); $('#bPatrol').classList.toggle('on', a === 'pt'); $('#bDemo').classList.toggle('on', a === 'od');
  if (!a && danceMusicOn) { danceMusicOn = false; cmd({ c: 'mu', a: 'stop' }); }
}
// 누른 곳 → 방향, 속도. 가운데(26 안)는 정지, 바깥(97 밖)은 무시
function padPoint(ev) {
  const svg = $('#pad'), r = svg.getBoundingClientRect();
  const x = (ev.clientX - r.left) / r.width * 200 - 100, y = (ev.clientY - r.top) / r.height * 200 - 100;
  const d = Math.hypot(x, y);
  let dir = null;
  if (d > 26 && d <= 97) dir = Math.abs(y) >= Math.abs(x) ? (y < 0 ? 'f' : 'b') : (x < 0 ? 'l' : 'r');
  const sp = Math.round(40 + Math.min(1, Math.max(0, (d - 30) / 62)) * 160);
  return { x, y, d, dir, sp };
}
function showTouch(p, code) {
  const t = $('#touch'); t.setAttribute('cx', p.x); t.setAttribute('cy', p.y); t.style.display = '';
  $('#spTx').textContent = SPEED_CODES.includes(code) ? '속도 ' + p.sp : '';
}
function leaveAi() { if (aiOn) { cmd({ c: 'lm', v: 0 }); aiOn = false; showAi(); } }
function move(code, sp, du) {
  leaveAi();
  const o = { c: 'ot', a: code, du: du || 0 };
  if (sp) o.sp = sp;
  cmd(o);
}
$('#pad').addEventListener('pointerdown', (ev) => {
  const p = padPoint(ev);
  if (p.d <= 26) { stopAll(); return; }
  if (!p.dir) return;
  const code = CODES[mode][p.dir], speed = SPEED_CODES.includes(code);
  pressed = { code, sp: speed ? p.sp : null, sent: speed ? p.sp : null, dir: p.dir };
  $('#pad').setPointerCapture(ev.pointerId);
  showTouch(p, code);
  move(code, pressed.sp);
});
$('#pad').addEventListener('pointermove', (ev) => {
  if (!pressed || pressed.sp === null) return;
  const p = padPoint(ev);
  if (p.dir === pressed.dir) { pressed.sp = p.sp; showTouch(p, pressed.code); }
});
const release = () => {
  if (pressed && pressed.sp !== null && pressed.sp !== pressed.sent) cmd({ c: 'ot', a: pressed.code, du: 0, sp: pressed.sp });
  pressed = null; $('#touch').style.display = 'none'; $('#spTx').textContent = '';
};
$('#pad').addEventListener('pointerup', release);
$('#pad').addEventListener('pointercancel', release);

function stopAll() {
  cmd({ c: 'ot', a: 'stop' });
  setTimeout(() => cmd({ c: 'ot', a: 'stop' }), 150);  // 걸음 중간이면 한 번 더
  if (danceMusicOn) { danceMusicOn = false; cmd({ c: 'mu', a: 'stop' }); }
  stopBook(false);
}
let danceMusicOn = false;
function startDance() {
  move('dn');
  if ($('#danceMusic').checked && songs.length) {
    const sn = songs.some((s) => s.sn === muCur) ? muCur : songs[0].sn;
    danceMusicOn = true;
    cmd({ c: 'mu', a: 'play', sn });
  }
}
function startPatrol() { move('pt'); if (!ledDemo) toggleLedDemo(); }
function startDemo() { move('od'); }
function testMove(code, du) { move(code, null, du); }
$('#danceMusic').checked = localStorage.getItem('otto_dance_music') !== '0';
$('#danceMusic').onchange = (e) => localStorage.setItem('otto_dance_music', e.target.checked ? '1' : '0');

// 패널 열고 닫기
const OPEN = { ai: () => cmd({ c: 'ak' }), radio: () => cmd({ c: 'rd', a: 'get' }), speak: () => cmd({ c: 'sk', a: 'get' }),
  sched: () => cmd({ c: 'os', a: 'get' }), music: () => { cmd({ c: 'mu', a: 'list' }); cmd({ c: 'mu', a: 'get' }); },
  light: () => cmd({ c: 'ol', ef: 'get' }), cal: () => cal({ a: 'get' }) };
document.querySelectorAll('.chip[data-p]').forEach((b) => b.onclick = () => {
  const p = $('#p_' + b.dataset.p), show = !p.classList.contains('show');
  p.classList.toggle('show', show); b.classList.toggle('on', show);
  if (show && OPEN[b.dataset.p]) OPEN[b.dataset.p]();
  if (show) p.scrollIntoView({ behavior: 'smooth', block: 'start' });
});

// ───────── AI 대화 ─────────
let aiOn = false;
function showAi() { $('#aiBtn').textContent = aiOn ? '대화 끄기' : '대화 켜기'; $('#aiBtn').className = 'btn' + (aiOn ? ' red' : ''); }
function toggleAi() {
  if (!aiOn) { cmd({ c: 'ot', a: 'stop' }); $('#aiMsg').textContent = '대화를 켜는 중... (몇 초 걸립니다)'; }
  cmd({ c: 'lm', v: aiOn ? 0 : 1 });
}
function addChat(us, tx) {
  const log = $('#chatLog');
  if (log.querySelector('.sub')) log.innerHTML = '';
  const a = document.createElement('div'); a.className = 'u'; a.textContent = '🙂 ' + (us || '');
  const b = document.createElement('div'); b.className = 'r'; b.textContent = '🤖 ' + (tx || '');
  log.append(a, b); log.scrollTop = log.scrollHeight;
}
function saveKey() { const k = $('#keyIn').value.trim(); if (!k) return; cmd({ c: 'ak', k }); $('#keyIn').value = ''; $('#keyMsg').textContent = '저장하는 중...'; }
function delKey() { if (confirm('보드에 저장된 API 키를 지울까요?')) cmd({ c: 'ak', k: '' }); }

// ───────── 라디오 ─────────
let rdSn = -1;
STATIONS.forEach((n, i) => {
  const b = document.createElement('button'); b.className = 'chip'; b.style.margin = '3px'; b.textContent = n;
  b.onclick = () => { leaveAi(); $('#rdMsg').textContent = '연결하는 중...'; cmd({ c: 'rd', a: 'play', sn: i }); };
  $('#stations').append(b);
});
function onRadio(m) {
  if (m.a === 'title') { $('#rdTitle').textContent = '♪ ' + m.tx; return; }
  $('#rdMsg').textContent = m.ok ? '' : errText(m.err);
  rdSn = m.pl ? m.sn : -1;
  document.querySelectorAll('#stations .chip').forEach((b, i) => b.classList.toggle('on', i === rdSn));
  $('#rdState').textContent = m.pl ? '재생 중: ' + STATIONS[m.sn] : '꺼짐';
  if (!m.pl) $('#rdTitle').textContent = '';
  if (m.vl !== undefined) { $('#rdVol').value = m.vl; $('#rdVolTx').textContent = m.vl; }
}
$('#rdVol').oninput = (e) => { $('#rdVolTx').textContent = e.target.value; later('rd', () => cmd({ c: 'rd', a: 'vol', vl: +e.target.value })); };
// 슬라이더를 끄는 동안 너무 자주 보내지 않게 0.3초 모아서 보낸다
const timers = {};
function later(key, fn) { clearTimeout(timers[key]); timers[key] = setTimeout(fn, 300); }

// ───────── 말하기 ─────────
$('#skVol').oninput = (e) => { $('#skVolTx').textContent = e.target.value; later('sk', () => cmd({ c: 'sk', a: 'vol', vl: +e.target.value })); };
function startSpeak() {
  const sx = $('#skText').value.trim(); if (!sx) return;
  stopBook(false); leaveAi();
  cmd({ c: 'sk', a: 'start', sx, sw: +$('#skWait').value, sq: +$('#skCount').value, vl: +$('#skVol').value });
}
function stopSpeak() { stopBook(false); cmd({ c: 'sk', a: 'stop' }); }
function onSpeak(m) {
  $('#skMsg').textContent = m.ok ? '' : errText(m.err);
  $('#skState').textContent = m.pl ? '말하는 중 (' + m.sd + '번)' : '멈춤';
  if (m.a === 'done' && book.running) nextBook();
  if (!m.ok && book.running) { $('#bookMsg').textContent = errText(m.err); stopBook(false); }
}

// ───────── 책읽기: 글을 70자 조각으로 나눠 한 조각씩 말하기(1번)로 보내고, done 이 오면 다음 조각 ─────────
const book = { running: false, chunks: [], pos: 0 };
BOOKS.forEach((b, i) => $('#bookSel').append(new Option(b.title, i)));
$('#bookSel').append(new Option('직접 입력', -1));
$('#bookSel').onchange = (e) => { const i = +e.target.value; $('#bookText').value = i >= 0 ? BOOKS[i].text : ''; };
$('#bookText').value = BOOKS[0].text;
function splitBook(text, limit = 70) {
  const sentences = (text.replace(/[ \t]+/g, ' ').match(/[^.!?。…\n]+[.!?。…]*/g) || []).map((s) => s.trim()).filter((s) => s);
  const pieces = [];
  for (const s of sentences) {
    let rest = s;
    while (rest.length > limit) {
      let cut = rest.lastIndexOf(',', limit - 1);
      if (cut < limit / 3) cut = rest.lastIndexOf(' ', limit);
      if (cut < limit / 3) cut = limit - 1;
      pieces.push(rest.slice(0, cut + 1).trim()); rest = rest.slice(cut + 1).trim();
    }
    if (rest) pieces.push(rest);
  }
  const chunks = [];
  for (const p of pieces) { const l = chunks.length - 1; if (l >= 0 && chunks[l].length + 1 + p.length <= limit) chunks[l] += ' ' + p; else chunks.push(p); }
  return chunks;
}
function sendBook() {
  $('#bookState').textContent = '읽는 중 ' + (book.pos + 1) + ' / ' + book.chunks.length;
  cmd({ c: 'sk', a: 'start', sx: book.chunks[book.pos], sw: 0, sq: 1, vl: +$('#skVol').value });
}
function startBook() {
  const chunks = splitBook($('#bookText').value); if (!chunks.length) return;
  leaveAi(); Object.assign(book, { running: true, chunks, pos: 0 }); $('#bookMsg').textContent = ''; sendBook();
}
function nextBook() {
  book.pos++;
  if (book.pos >= book.chunks.length) { book.running = false; $('#bookState').textContent = '다 읽었어요'; return; }
  setTimeout(() => { if (book.running) sendBook(); }, 300);
}
function stopBook(sendStop) {
  if (!book.running) return;
  book.running = false; $('#bookState').textContent = '멈춤 (' + book.pos + ' / ' + book.chunks.length + ')';
  if (sendStop) cmd({ c: 'sk', a: 'stop' });
}

// ───────── 받아쓰기: 브라우저 음성 인식 + 로봇 LED (녹색 = 말하세요, 빨간색 = 바꾸는 중) ─────────
const Rec = window.SpeechRecognition || window.webkitSpeechRecognition;
let rec = null, dictOn = false, ledState = '';
function dictLed(s) {
  if (ledState === s) return; ledState = s;
  if (s === 'off') cmd({ c: 'ol', ef: 'off' }); else cmd({ c: 'ol', ef: 'sd', co: s === 'green' ? '00ff00' : 'ff0000', br: 80 });
}
function toggleDict() {
  if (dictOn) { dictOn = false; if (rec) rec.stop(); dictLed('off'); $('#dictBtn').textContent = '받아쓰기 시작'; $('#dictState').textContent = ''; return; }
  if (!Rec || !window.isSecureContext) {
    $('#dictMsg').innerHTML = '이 주소(http://)에서는 브라우저가 마이크를 막습니다. PC Chrome 이라면 주소창에 '
      + '<b>chrome://flags/#unsafely-treat-insecure-origin-as-secure</b> 를 열고 <b>' + location.origin + '</b> 를 넣은 뒤 Enabled → 다시 시작하세요.';
    return;
  }
  dictOn = true; $('#dictMsg').textContent = ''; $('#dictBtn').textContent = '받아쓰기 끝내기';
  rec = new Rec(); rec.lang = 'ko-KR'; rec.interimResults = true; rec.continuous = true;
  rec.onstart = () => { dictLed('green'); $('#dictState').textContent = '🟢 말하세요'; };
  rec.onspeechend = () => { dictLed('red'); $('#dictState').textContent = '🔴 글자로 바꾸는 중'; };
  rec.onresult = (e) => {
    let interim = '';
    for (let i = e.resultIndex; i < e.results.length; i++) {
      const r = e.results[i];
      if (r.isFinal) {
        const d = document.createElement('div'); d.textContent = new Date().toLocaleTimeString() + '  ' + r[0].transcript; $('#dictLog').prepend(d);
        dictLed('green'); $('#dictState').textContent = '🟢 말하세요';
      } else interim += r[0].transcript;
    }
    $('#dictInterim').textContent = interim;
  };
  rec.onerror = (e) => { $('#dictMsg').textContent = e.error === 'not-allowed' ? '마이크 사용이 막혀 있습니다.' : '음성 인식 오류: ' + e.error; };
  rec.onend = () => { if (dictOn) rec.start(); };  // 잠깐 조용하면 끝나므로 다시 시작한다
  rec.start();
}

// ───────── 시계, 조명, LED 데모 ─────────
let clockOn = false, ledDemo = false;
function toggleClock() { cmd({ c: 'ck', a: clockOn ? 'off' : 'on' }); }
function lightOn() { cmd({ c: 'ol', ef: 'sd', co: $('#lgColor').value.slice(1), br: Math.round(+$('#lgPct').value * 255 / 100) }); }
$('#lgPct').oninput = (e) => { $('#lgPctTx').textContent = e.target.value + '%'; later('lg', ledDemo ? () => cmd({ c: 'ol', ef: 'demo', br: Math.round(+e.target.value * 2.55) }) : lightOn); };
$('#lgColor').oninput = () => later('lg', lightOn);
function toggleLedDemo() { cmd(ledDemo ? { c: 'ol', ef: 'off' } : { c: 'ol', ef: 'demo', br: Math.round(+$('#lgPct').value * 2.55) }); }
function onLight(m) {
  ledDemo = m.ef === 'demo'; $('#bLedDemo').classList.toggle('on', ledDemo);
  $('#lgState').textContent = m.ef === 'demo' ? 'LED 데모 중' : m.ef === 'sd' ? '켜짐' : '꺼짐';
}

// ───────── 예약: "대상,켜는 분,끄는 분,요일,사용,값" 을 ; 로 이어서 주고받는다 ─────────
const TARGETS = ['조명', '라디오', '내 음악'], DAYS = ['일', '월', '화', '수', '목', '금', '토'];
let slots = [];
const m2t = (m) => m < 0 ? '' : String(Math.floor(m / 60)).padStart(2, '0') + ':' + String(m % 60).padStart(2, '0');
const t2m = (t) => { if (!t) return -1; const [h, m] = t.split(':').map(Number); return h * 60 + m; };
function onSched(m) {
  $('#schMsg').textContent = m.ok ? (m.a === 'set' ? '✅ 저장했습니다' : '') : errText(m.err);
  slots = (m.tx || '').split(';').filter((s) => s).map((s) => { const v = s.split(',').map(Number); return { t: v[0], on: v[1], off: v[2], d: v[3], en: v[4], p: v[5] }; });
  drawSched();
}
function drawSched() {
  const tb = $('#schRows'); tb.innerHTML = '';
  slots.forEach((s, i) => {
    const tr = document.createElement('tr');
    let days = ''; DAYS.forEach((n, k) => days += `<label><input type="checkbox" ${s.d & (1 << k) ? 'checked' : ''} onchange="slots[${i}].d^=${1 << k}">${n}</label>`);
    let param = '';
    if (s.t === 1) param = '<select onchange="slots[' + i + '].p=+this.value">' + STATIONS.map((n, k) => `<option value="${k}" ${k === s.p ? 'selected' : ''}>${n.split(' (')[0]}</option>`).join('') + '</select>';
    if (s.t === 2) param = '곡 <input type="number" min="0" max="6" style="width:50px" value="' + s.p + '" onchange="slots[' + i + '].p=+this.value">';
    tr.innerHTML = `<td><input type="checkbox" ${s.en ? 'checked' : ''} onchange="slots[${i}].en=this.checked?1:0" title="사용"></td>
      <td><select onchange="slots[${i}].t=+this.value;slots[${i}].p=0;drawSched()">${TARGETS.map((n, k) => `<option value="${k}" ${k === s.t ? 'selected' : ''}>${n}</option>`).join('')}</select></td>
      <td>켜기 <input type="time" value="${m2t(s.on)}" onchange="slots[${i}].on=t2m(this.value)"><br>끄기 <input type="time" value="${m2t(s.off)}" onchange="slots[${i}].off=t2m(this.value)"></td>
      <td class="days">${days}<br>${param}</td><td><button class="btn gray" onclick="slots.splice(${i},1);drawSched()">✕</button></td>`;
    tb.append(tr);
  });
  if (!slots.length) tb.innerHTML = '<tr><td class="sub">예약이 없습니다.</td></tr>';
}
function addSched() { if (slots.length >= 8) return; slots.push({ t: 0, on: 7 * 60, off: 8 * 60, d: 127, en: 1, p: 0 }); drawSched(); }
function saveSched() { cmd({ c: 'os', a: 'set', tx: slots.map((s) => [s.t, s.on, s.off, s.d, s.en, s.p].join(',')).join(';') }); }

// ───────── 내 음악 ─────────
let songs = [], muCur = 0;
function onMusic(m) {
  if (m.a === 'list') {
    songs = (m.tx || '').split('\t').filter((s) => s).map((s) => { const v = s.split('|'); return { sn: +v[0], kb: +v[1], title: v.slice(2).join('|') }; });
    const tb = $('#muRows'); tb.innerHTML = songs.length ? '' : '<tr><td class="sub">저장된 곡이 없습니다.</td></tr>';
    songs.forEach((s) => {
      const tr = document.createElement('tr');
      tr.innerHTML = `<td>${s.sn}</td><td></td><td class="sub">${s.kb} KB</td><td><button class="btn" onclick="playSong(${s.sn})">▶</button><button class="btn gray" onclick="delSong(${s.sn})">🗑</button></td>`;
      tr.children[1].textContent = s.title; tb.append(tr);
    });
    return;
  }
  if (m.a === 'put') {
    if (m.dn) { $('#muUp').textContent = '✅ 저장했습니다 (' + m.sn + '번)'; cmd({ c: 'mu', a: 'list' }); }
    if (!m.ok) $('#muUp').textContent = '저장 실패: ' + errText(m.err);
  }
  if (m.a === 'del' && m.ok) cmd({ c: 'mu', a: 'list' });
  if (!m.ok && m.a !== 'put') $('#muMsg').textContent = errText(m.err); else $('#muMsg').textContent = '';
  muCur = m.sn;
  if (m.vl !== undefined) { $('#muVol').value = m.vl; $('#muVolTx').textContent = m.vl; }
  $('#muState').textContent = (m.pl ? '재생 중: ' + m.sn + '번' : '멈춤') + (m.fr !== undefined ? ' · 남은 공간 ' + m.fr + ' KB' : '');
}
function playSong(sn) { leaveAi(); cmd({ c: 'mu', a: 'play', sn }); }
function delSong(sn) { if (confirm(sn + '번 곡을 지울까요?')) cmd({ c: 'mu', a: 'del', sn }); }
$('#muVol').oninput = (e) => { $('#muVolTx').textContent = e.target.value; later('mu', () => cmd({ c: 'mu', a: 'vol', vl: +e.target.value })); };
$('#muFile').onchange = (e) => { const f = e.target.files[0]; if (f) $('#muTitle').value = f.name.replace(/\.mp3$/i, ''); };
// mp3 앞뒤의 태그(앨범 그림 등)를 떼어 소리만 남긴다. 큰 앨범 그림이 붙어 있으면 보드가 재생하지 못한다
function stripMp3Tags(src) {
  let start = 0;
  while (src.length - start >= 10 && src[start] === 0x49 && src[start + 1] === 0x44 && src[start + 2] === 0x33) {
    const size = ((src[start + 6] & 0x7f) << 21) | ((src[start + 7] & 0x7f) << 14) | ((src[start + 8] & 0x7f) << 7) | (src[start + 9] & 0x7f);
    start += 10 + size + ((src[start + 5] & 0x10) ? 10 : 0);
  }
  for (let i = start, lim = Math.min(src.length - 1, start + 32768); i < lim; i++) if (src[i] === 0xff && (src[i + 1] & 0xe0) === 0xe0) { start = i; break; }
  let end = src.length;
  if (end - start >= 128 && src[end - 128] === 0x54 && src[end - 127] === 0x41 && src[end - 126] === 0x47) end -= 128;
  if (end - start >= 32 && String.fromCharCode(...src.subarray(end - 32, end - 24)) === 'APETAGEX') {
    const tagSize = src[end - 20] | (src[end - 19] << 8) | (src[end - 18] << 16) | (src[end - 17] << 24);
    const cut = tagSize + ((src[end - 9] & 0x80) ? 32 : 0);
    if (cut > 0 && cut <= end - start) end -= cut;
  }
  return start >= end ? src : src.subarray(start, end);
}
async function uploadMusic() {
  const f = $('#muFile').files[0]; if (!f) { $('#muUp').textContent = 'mp3 파일을 고르세요.'; return; }
  const data = stripMp3Tags(new Uint8Array(await f.arrayBuffer()));
  const title = $('#muTitle').value.trim() || f.name;
  const fd = new FormData(); fd.append('f', new Blob([data], { type: 'audio/mpeg' }), 'song.mp3');
  const x = new XMLHttpRequest();
  x.open('POST', '/upload?sz=' + data.length + '&tx=' + encodeURIComponent(title));
  x.upload.onprogress = (e) => { if (e.lengthComputable) { $('#muBar').style.width = (e.loaded / e.total * 100) + '%'; $('#muUp').textContent = '보내는 중 ' + Math.round(e.loaded / 1024) + ' / ' + Math.round(e.total / 1024) + ' KB'; } };
  x.onload = () => { $('#muBar').style.width = '100%'; };
  x.onerror = () => { $('#muUp').textContent = '보내다가 끊겼습니다.'; };
  $('#muUp').textContent = '보내는 중... (로봇이 멈추고 소리가 꺼집니다)';
  x.send(fd);
}

// ───────── 로봇 맞추기: oc get/set/save/clear/p0/p90 ─────────
const CAL = {
  cal2: [['cyl', '왼쪽 다리 YL (us)', 1000, 1900, 5], ['cyr', '오른쪽 다리 YR (us)', 1000, 1900, 5]],
  cal3: [['crl', '왼발 속도 (us)', 0, 600, 5], ['crr', '오른발 속도 (us)', 0, 600, 5]],
  cal4: [['ctr', '오른발 들 때 (도)', 0, 45, 1], ['ctl', '왼발 들 때 (도)', 0, 45, 1]],
  cal5: [['d1', '왼발 서기 YR (도)', 0, 135, 1], ['d2', '왼발 서기 YL (도)', 55, 180, 1], ['d3', '오른발 서기 YR (도)', 0, 135, 1], ['d4', '오른발 서기 YL (도)', 55, 180, 1]],
};
let calVals = {};
Object.entries(CAL).forEach(([box, rows]) => rows.forEach(([k, label, min, max, step]) => {
  const d = document.createElement('div'); d.className = 'row';
  d.innerHTML = `<label>${label}</label><button class="btn gray" data-d="-${step}">−</button><input type="range" min="${min}" max="${max}" step="${step}" id="cv_${k}">
    <button class="btn gray" data-d="${step}">+</button><span class="num" id="ct_${k}">-</span>`;
  const r = d.querySelector('input');
  const setV = (v) => { v = Math.min(max, Math.max(min, v)); r.value = v; $('#ct_' + k).textContent = v; calVals[k] = v; later('cal', () => cal({ a: 'set', [k]: v })); };
  r.oninput = () => setV(+r.value);
  d.querySelectorAll('button').forEach((b) => b.onclick = () => setV(+r.value + +b.dataset.d));
  $('#' + box).append(d);
}));
function cal(o) { return cmd(Object.assign({ c: 'oc' }, o)); }
function onCal(m) {
  $('#calMsg').textContent = m.ok ? '' : errText(m.err);
  if (m.a === 'save' && m.ok) $('#calMsg').innerHTML = '<span class="ok">✅ 보드에 저장했습니다</span>';
  ['cyl', 'cyr', 'crl', 'crr', 'ctr', 'ctl', 'd1', 'd2', 'd3', 'd4'].forEach((k) => {
    if (m[k] === undefined) return; calVals[k] = m[k]; $('#cv_' + k).value = m[k]; $('#ct_' + k).textContent = m[k];
  });
  $('#calSaved').textContent = m.csv ? '저장된 값을 쓰는 중' : '기본값 (저장 안 됨)';
}
function copyCal() {
  const t = JSON.stringify(Object.assign({ c: 'oc', a: 'restore' }, calVals));
  const ta = document.createElement('textarea'); ta.value = t; document.body.append(ta); ta.select(); document.execCommand('copy'); ta.remove();
  $('#calMsg').innerHTML = '<span class="ok">복사했습니다: ' + t + '</span>';
}

// ───────── 시작 ─────────
fetch('/info').then((r) => r.json()).then((j) => {
  $('#info').textContent = 'MAC: ' + j.m;
  $('#foot').textContent = j.t + ' ' + j.ver + ' · http://' + j.ip + ' · http://' + j.host + '.local';
}).catch(() => {});
drawSched();
poll();
</script></body></html>
)PAGE";
