// page.h: 보드가 보내 주는 웹페이지 (i2r.link 장치 화면과 같은 배치)
// 보드와는 GET /state (1초마다 상태), POST /cmd (명령 JSON)로만 주고받는다.

#pragma once

const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>i2r IoT PLC</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,"Segoe UI","Malgun Gothic",sans-serif;background:#f8fafc;color:#0f172a}
header{background:#fff;box-shadow:0 2px 8px rgba(0,0,0,.06);height:64px;display:flex;align-items:center;gap:14px;padding:0 24px}
.logo{width:26px;height:26px;border-radius:50%;display:block}
.brand{font-size:17px;color:#334155}
.off{margin-left:auto;background:#fee2e2;color:#b91c1c;padding:6px 12px;border-radius:8px;font-size:14px}
main{max-width:980px;margin:0 auto;padding:24px 12px 40px}
.title{padding:0 8px 18px}
.title h2{margin:8px 0 6px;font-size:26px;display:flex;align-items:center;gap:8px}
.gear{border:none;background:none;font-size:22px;color:#94a3b8;cursor:pointer;padding:0}
.gear:hover{color:#475569}
.mac{font-size:13px;color:#64748b}
.card{background:#fff;border:1px solid #e2e8f0;border-radius:16px;padding:22px;margin-bottom:22px;box-shadow:0 1px 3px rgba(0,0,0,.03)}
.card h3{margin:0 0 14px}
.ct{font-weight:700;font-size:14px;color:#475569;margin-bottom:16px}
.grid{display:flex;flex-wrap:wrap;gap:18px}
.item{width:120px;height:84px;background:#f8fafc;border:1px solid #edf2f7;border-radius:12px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:10px;position:relative}
.nm{font-size:14px;color:#334155;max-width:110px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.link{cursor:pointer}.link:hover{color:#007bff;text-decoration:underline}
.dot{width:24px;height:24px;border-radius:50%;background:#cbd5e1;border:3px solid #fff;box-shadow:0 0 0 1px #e2e8f0}
.dot.on{background:#22c55e;box-shadow:0 0 8px #22c55e}
.sw{width:54px;height:28px;border-radius:14px;border:none;background:#e2e8f0;position:relative;cursor:pointer;transition:.2s}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:22px;height:22px;border-radius:50%;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.25);transition:.2s}
.sw.on{background:#22c55e}.sw.on::after{left:29px}
.rm{position:absolute;bottom:3px;font-size:11px;color:#f59e0b}
.sensors{display:grid;grid-template-columns:1fr 1fr;gap:22px}
.sc{background:#fff;border:1px solid #e2e8f0;border-radius:16px;padding:18px;display:flex;align-items:center;gap:16px;cursor:pointer}
.sc:hover{border-color:#93c5fd}
.si{width:50px;height:50px;border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:24px}
.sv{font-size:26px;font-weight:700}.su{font-size:15px;margin-left:4px}.sl{font-size:13px;color:#64748b;margin-top:2px}
.hint{color:#94a3b8;font-size:13px;margin:22px 8px 0}
.ngrid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.ngrid label{display:block;font-size:12px;color:#666;margin-bottom:3px}
.hide{display:none!important}
/* 대화창 */
.ov{position:fixed;inset:0;background:rgba(15,23,42,.45);display:flex;align-items:flex-start;justify-content:center;overflow:auto;padding:30px 10px;z-index:10}
.modal{background:#fff;border-radius:14px;width:100%;max-width:700px;padding:24px 22px}
.modal h3{margin:0 0 6px;font-size:24px;display:inline-block;padding-bottom:8px;border-bottom:2px solid #007bff}
.modal h4{color:#0b6bcb;margin:22px 0 10px;font-size:16px}
.modal h4.bar{color:#1e293b;border-left:4px solid #3b82f6;padding-left:8px}
.tbl{border-radius:10px;overflow:hidden;box-shadow:0 1px 4px rgba(0,0,0,.08)}
table{width:100%;border-collapse:collapse;font-size:14px}
th{background:#007bff;color:#fff;padding:12px 6px;font-weight:700}
td{padding:9px 6px;text-align:center;border-bottom:1px solid #f1f5f9}
td.empty{color:#475569;font-style:italic;padding:12px}
.del{border:none;background:none;cursor:pointer;font-size:16px}
.fg{background:#f6f7f9;border-radius:10px;padding:16px;display:flex;flex-direction:column;gap:10px}
.fg label{font-size:14px;font-weight:500;color:#334155}
select,input[type=number],input[type=text],input[type=time]{width:100%;padding:10px 12px;border:1px solid #d6dbe1;border-radius:6px;font-size:15px;background:#fff}
.row{display:flex;gap:10px}
.days{display:flex;gap:6px;flex-wrap:wrap}
.days button{width:40px;height:36px;border:1px solid #cbd5e1;background:#fff;border-radius:6px;cursor:pointer}
.days button.sel{background:#007bff;color:#fff;border-color:#007bff}
.clock{background:#f8fafc;border:1px solid #e2e8f0;border-radius:8px;padding:12px 14px;margin:16px 0 6px;display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
.clock b{font-family:monospace;font-size:17px;background:#fff;border:1px solid #cbd5e1;border-radius:4px;padding:3px 8px}
.clock .muted{color:#94a3b8;font-size:14px}
.clock select{width:auto;max-width:230px;padding:6px 8px;font-size:14px}
.acts{display:flex;justify-content:flex-end;gap:10px;margin-top:22px;padding-top:18px;border-top:1px solid #e2e8f0}
.acts.left{justify-content:flex-start;border:none;padding-top:0}
.acts button,.btn{padding:10px 18px;border-radius:6px;border:none;font-size:15px;font-weight:700;cursor:pointer;color:#fff}
.b-view{background:#0056b3}.b-save{background:#007bff}.b-del{background:#ff4757}
.b-cancel{background:#fff!important;color:#333!important;border:1px solid #ccc!important;font-weight:500!important}
.b-sync{background:#3b82f6;padding:6px 14px;font-size:14px}
.acts button{white-space:nowrap}
@media(max-width:600px){.acts{gap:6px}.acts button{flex:1;padding:10px 4px;font-size:14px}.sensors{grid-template-columns:1fr}.item{width:calc(50% - 9px)}.ngrid{grid-template-columns:1fr}.row{flex-direction:column}}
</style></head><body>
<header><img class="logo" src="/logo.png" alt="i2r"><span class="brand">IoT PLC 로컬 제어</span><span id="off" class="off hide">보드와 연결이 끊겼습니다</span></header>
<main>
<div class="title">
  <h2><span id="devName">연결 중...</span><button class="gear" onclick="openNames()" title="이름 변경">⚙</button></h2>
  <div class="mac" id="mac"></div>
</div>

<div id="nameCard" class="card hide">
  <h3>이름 변경</h3>
  <div id="nameGrid" class="ngrid"></div>
  <div class="acts left" style="margin-top:18px"><button class="b-save" onclick="saveNames()">저장</button><button class="b-cancel" onclick="hide('nameCard')">취소</button></div>
</div>

<div class="card"><div class="ct">▣ 입력 상태 (INPUTS)</div><div class="grid" id="inGrid"></div></div>
<div class="card"><div class="ct">ϟ 출력 제어 (OUTPUTS)</div><div class="grid" id="outGrid"></div></div>
<div class="sensors hide" id="sensors">
  <div class="sc" onclick="openBs('temp')"><div class="si" style="background:#fee2e2">🌡️</div><div><span class="sv" id="vTemp">--</span><span class="su">°C</span><div class="sl">온도</div></div></div>
  <div class="sc" onclick="openBs('humi')"><div class="si" style="background:#e0f2fe">💧</div><div><span class="sv" id="vHumi">--</span><span class="su">%</span><div class="sl">습도</div></div></div>
</div>
<p class="hint">입력 이름을 누르면 입력 규칙, 출력 이름을 누르면 스케줄, 온도·습도를 누르면 센서 트리거를 설정합니다. ⚙ 를 누르면 이름을 바꿉니다.</p>
</main>

<!-- 입력 규칙 (bio) -->
<div class="ov hide" id="mBio"><div class="modal">
  <h3 id="bioTitle"></h3>
  <h4>설정된 동작 목록</h4>
  <div class="tbl"><table><thead><tr><th>번호</th><th>트리거</th><th>지연</th><th>동작시간</th><th>이름</th><th>포트</th><th>상태</th><th>삭제</th></tr></thead><tbody id="bioList"></tbody></table></div>
  <h4>포트 트리거</h4>
  <div class="fg">
    <select id="bioTr" onchange="bioStep()"><option value="" disabled>선택포트의 트리거를 선택하세요</option><option value="1">켜짐</option><option value="0">꺼짐</option></select>
    <select id="bioPort" onchange="bioStep()"></select>
    <select id="bioV"><option value="" disabled>상태를 선택하세요</option><option value="1">켜짐</option><option value="0">꺼짐</option></select>
    <div class="row" id="bioTimes"><input type="number" id="bioD" placeholder="지연 (초)" min="0"><input type="number" id="bioDu" placeholder="동작 시간 (초, 0=계속)" min="0"></div>
  </div>
  <div class="acts"><button class="b-view" onclick="bioView()">보기</button><button class="b-save" onclick="bioInsert()">등록</button><button class="b-del" onclick="bioDelAll()">모두지움</button><button class="b-cancel" onclick="closeM()">취소</button></div>
</div></div>

<!-- 스케줄 (sch) -->
<div class="ov hide" id="mSch"><div class="modal">
  <h3 id="schTitle"></h3>
  <div class="clock">
    <div>🕒 <span style="color:#475569;font-weight:500">기기 현재 시간</span> <span id="clock" class="muted">로딩 중...</span><div id="tsrc" style="font-size:12px;color:#64748b;margin-top:6px"></div></div>
    <div style="display:flex;gap:8px;align-items:center"><select id="tz" title="시간대"></select><button class="btn b-sync" onclick="syncTime()">♻️ 동기화</button></div>
  </div>
  <h4 class="bar">설정된 스케줄 목록</h4>
  <div class="tbl"><table><thead><tr><th>번호</th><th>시간 / 반복</th><th>삭제</th></tr></thead><tbody id="schList"></tbody></table></div>
  <h4>스케줄 추가</h4>
  <div class="fg">
    <label>시작 시간</label><input type="time" step="1" id="schS" value="09:00:00">
    <label>종료 시간</label><input type="time" step="1" id="schE" value="17:00:00">
    <label>반복 설정</label><select id="schRm" onchange="schStep()"><option value="daily">매일</option><option value="weekly">매주</option></select>
    <div id="schDays"><label>요일 선택</label><div class="days" id="dayBtns"></div></div>
  </div>
  <div class="acts"><button class="b-view" onclick="schView()">보기</button><button class="b-save" onclick="schSave()">저장</button><button class="b-del" onclick="schDelAll()">모두 지우기</button><button class="b-cancel" onclick="closeM()">취소</button></div>
</div></div>

<!-- 센서 트리거 (bs) -->
<div class="ov hide" id="mBs"><div class="modal">
  <h3 id="bsTitle"></h3>
  <h4>설정된 트리거 목록</h4>
  <div class="tbl"><table><thead><tr><th>번호</th><th>조건</th><th>설정</th><th>지연</th><th>동작시간</th><th>동작</th><th>삭제</th></tr></thead><tbody id="bsList"></tbody></table></div>
  <h4>트리거 추가</h4>
  <div class="fg">
    <label>트리거 선택</label>
    <select id="bsTr" onchange="bsStep()"><option value="">선택하세요</option><option value="1">⬆️ 올라갈 때</option><option value="0">⬇️ 내려갈 때</option></select>
    <div id="bsMore" style="display:flex;flex-direction:column;gap:10px">
      <label id="bsTvLabel"></label>
      <div class="row"><input type="number" id="bsTv" step="0.1"><input type="number" id="bsD" placeholder="지연(초)" min="0"><input type="number" id="bsDu" placeholder="동작시간(초, 0=계속)" min="0"></div>
      <label>동작할 포트</label>
      <select id="bsPort" onchange="bsStep()"></select>
      <select id="bsV"><option value="" disabled>상태 선택</option><option value="1">켜짐</option><option value="0">꺼짐</option></select>
      <button class="btn b-save" style="width:100%;margin-top:4px" onclick="bsInsert()">트리거 등록</button>
    </div>
  </div>
  <div class="fg" style="margin-top:20px">
    <label id="calLabel"></label>
    <div class="row"><input type="number" id="cal" step="0.1"><button class="btn b-view" style="white-space:nowrap" onclick="saveCal()">보정 저장</button></div>
  </div>
  <div class="acts"><button class="b-view" onclick="bsView()">보기</button><button class="b-save" onclick="bsInsert()">등록</button><button class="b-del" onclick="bsDelAll()">모두삭제</button><button class="b-cancel" onclick="closeM()">취소</button></div>
</div></div>

<script>
const $=id=>document.getElementById(id);
const hide=id=>$(id).classList.add('hide');
const show=(id,on=true)=>$(id).classList.toggle('hide',!on);
let S=null,built=false,cur=null,busy=false,days=[];
const DAYS=['일','월','화','수','목','금','토'];
const TZ=[[-43200,'UTC-12:00'],[-39600,'UTC-11:00'],[-36000,'UTC-10:00 (Hawaii)'],[-32400,'UTC-09:00 (Alaska)'],[-28800,'UTC-08:00 (Pacific Time)'],[-25200,'UTC-07:00 (Mountain Time)'],[-21600,'UTC-06:00 (Central Time)'],[-18000,'UTC-05:00 (Eastern Time)'],[-14400,'UTC-04:00 (Atlantic Time)'],[-10800,'UTC-03:00'],[-7200,'UTC-02:00'],[-3600,'UTC-01:00'],[0,'UTC+00:00 (London, GMT)'],[3600,'UTC+01:00 (Paris, Berlin)'],[7200,'UTC+02:00 (Cairo)'],[10800,'UTC+03:00 (Moscow, Istanbul)'],[14400,'UTC+04:00 (Dubai)'],[18000,'UTC+05:00 (Karachi)'],[19800,'UTC+05:30 (India)'],[21600,'UTC+06:00 (Dhaka)'],[25200,'UTC+07:00 (Bangkok, Jakarta)'],[28800,'UTC+08:00 (Beijing, Singapore)'],[32400,'UTC+09:00 (Seoul, Tokyo)'],[34200,'UTC+09:30'],[36000,'UTC+10:00 (Sydney)'],[39600,'UTC+11:00'],[43200,'UTC+12:00 (Auckland)']];
const ERR={bad_n:'없는 포트 번호입니다.',bad_v:'값을 확인하세요. (숫자, 범위: 시간은 0~86400초, 기준 값은 -40~100)',bad_o:'모르는 작업입니다.',bad_si:'이미 지워진 항목입니다. 보기를 눌러 다시 확인하세요.',bad_ts:'이 보드에는 그 센서가 없습니다.',full:'더 넣을 수 없습니다. (최대 40개)',unknown:'보드가 모르는 명령입니다.',no_sensor:'센서 값을 읽지 못해 보정할 수 없습니다.',too_long:'이름이 너무 깁니다. (한글 약 20자까지)',json:'보낸 내용이 올바르지 않습니다.'};

const nIn=()=>S.in.length;
const inName=i=>S.name[1+i]||('입력 '+(i+1));
const outName=i=>S.name[1+nIn()+i]||('출력 '+(i+1));
const z=n=>String(n).padStart(2,'0');
const hms=s=>z(Math.floor(s/3600))+':'+z(Math.floor(s%3600/60))+':'+z(s%60);
const toSec=t=>{const p=(t||'').split(':').map(Number);return (p[0]||0)*3600+(p[1]||0)*60+(p[2]||0)};
const secTxt=(v,zero)=>v>0?v+'초':zero;
const num=s=>s===''?0:Number(s);

async function api(o){
  try{
    const r=await fetch('/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});
    const j=await r.json();
    if(j.c==='err'){alert('보드가 받지 않았습니다: '+(ERR[j.code]||j.code));return null}
    return j;
  }catch(e){alert('보드에 연결할 수 없습니다. 같은 Wi-Fi에 있는지 확인하세요.');return null}
}
function setState(s){S=s;render()}
async function poll(){
  if(busy)return;busy=true;
  try{const r=await fetch('/state',{cache:'no-store'});setState(await r.json());hide('off')}
  catch(e){show('off')}
  busy=false;
}

function build(){
  const ig=$('inGrid'),og=$('outGrid');ig.innerHTML='';og.innerHTML='';
  S.in.forEach((_,i)=>{
    const d=document.createElement('div');d.className='item link';d.title='입력 규칙 설정';
    d.innerHTML='<span class="nm" id="inN'+i+'"></span><div class="dot" id="in'+i+'"></div>';
    d.onclick=()=>openBio(i);ig.appendChild(d);
  });
  S.out.forEach((_,i)=>{
    const d=document.createElement('div');d.className='item';
    d.innerHTML='<span class="nm link" id="outN'+i+'" title="스케줄 설정"></span><button class="sw" id="sw'+i+'"></button><span class="rm" id="rm'+i+'"></span>';
    d.querySelector('.nm').onclick=()=>openSch(i);
    d.querySelector('.sw').onclick=()=>toggle(i);
    og.appendChild(d);
  });
  show('sensors',!!S.sensor);
  const tz=$('tz');TZ.forEach(([v,l])=>tz.add(new Option(l,v)));
  built=true;
}
function render(){
  if(!S)return;
  if(!built)build();
  $('devName').textContent=S.name[0];
  document.title=S.name[0]+' - i2r';
  $('mac').textContent='MAC: '+S.m+'  ·  '+S.t+'  ·  '+S.ip+'  ·  '+S.ver;
  S.in.forEach((v,i)=>{$('in'+i).classList.toggle('on',v===1);$('inN'+i).textContent=inName(i)});
  S.out.forEach((v,i)=>{$('sw'+i).classList.toggle('on',v===1);$('outN'+i).textContent=outName(i);$('rm'+i).textContent=S.remain[i]>0?'⏱ '+S.remain[i]+'초':''});
  $('vTemp').textContent=S.temp!==undefined?S.temp.toFixed(1):'--';
  $('vHumi').textContent=S.humi!==undefined?Math.round(S.humi):'--';
  const c=$('clock');
  if(S.time){c.textContent=S.time;c.className='';c.style.cssText='font-family:monospace;font-size:17px;font-weight:600;background:#fff;border:1px solid #cbd5e1;border-radius:4px;padding:3px 8px'}
  else{c.textContent='시간 없음 (동기화를 누르세요)';c.className='muted';c.style.cssText=''}
  const ago=S.ntpAgo>=0?(S.ntpAgo<60?'방금':Math.floor(S.ntpAgo/60)+'분 전'):'';
  const src={ntp:'🌐 인터넷 시간 ('+ago+' 맞춤, 1시간마다 다시 맞춤)',saved:'💾 저장된 시각에서 이어 가는 중 (인터넷 시간을 받으면 고침)',web:'🖥️ 화면에서 동기화한 시간'+(ago?' (인터넷 시간: '+ago+')':''),none:''};
  $('tsrc').textContent=src[S.tsrc]||'';
}
async function toggle(i){const r=await api({c:'so',m:S.m,n:i,v:S.out[i]?0:1});if(r)setState(r)}

// ---- 공통 ----
function openM(id){['mBio','mSch','mBs'].forEach(hide);show(id)}
function closeM(){['mBio','mSch','mBs'].forEach(hide);cur=null}
function fillPorts(sel,first){sel.innerHTML='';const o=new Option(first,'');o.disabled=true;sel.add(o);S.out.forEach((_,i)=>sel.add(new Option(outName(i),i)));sel.value=''}
function delBtn(fn){const b=document.createElement('button');b.className='del';b.title='삭제';b.textContent='🗑️';b.onclick=fn;return b}
function table(id,cols,empty,items,rowFn){
  const tb=$(id);tb.innerHTML='';
  if(!items.length){tb.innerHTML='<tr><td colspan="'+cols+'" class="empty"></td></tr>';tb.querySelector('td').textContent=empty;return}
  items.forEach((it,k)=>{const tr=document.createElement('tr');rowFn(it,k).forEach(c=>{const td=document.createElement('td');if(c instanceof Node)td.appendChild(c);else td.textContent=c;tr.appendChild(td)});tb.appendChild(tr)});
}

// ---- 입력 규칙 (bio) ----
function openBio(i){
  cur={k:'bio',i};$('bioTitle').textContent=inName(i);
  $('bioTr').value='';fillPorts($('bioPort'),'포트를 선택하세요');$('bioV').value='';$('bioD').value='';$('bioDu').value='';
  bioStep();showBio([]);openM('mBio');bioView();
}
function bioStep(){const t=$('bioTr').value!=='',p=$('bioPort').value!=='';show('bioPort',t);show('bioV',t&&p);show('bioTimes',t&&p)}
function showBio(items){table('bioList',8,'등록된 동작이 없습니다.',items,(it,k)=>[k+1,it.tr?'켜짐':'꺼짐',secTxt(it.d,'-'),secTxt(it.du,'계속'),S.name[0],outName(it.n),it.v?'켜짐':'꺼짐',delBtn(()=>bioDel(it.sI))])}
async function bioView(){const r=await api({c:'bio',o:'list',m:S.m,n:cur.i});if(r)showBio(r.items)}
async function bioInsert(){
  const tr=$('bioTr').value,p=$('bioPort').value,v=$('bioV').value;
  if(tr===''||p===''||v===''){alert('트리거, 포트, 상태를 모두 선택하세요.');return}
  const r=await api({c:'bio',o:'insert',m:S.m,n:cur.i,tr:+tr,d:num($('bioD').value),du:num($('bioDu').value),ps:[{m:S.m,n:+p,v:+v}]});
  if(r){showBio(r.items);$('bioTr').value='';fillPorts($('bioPort'),'포트를 선택하세요');$('bioV').value='';$('bioD').value='';$('bioDu').value='';bioStep()}
}
async function bioDel(sI){const r=await api({c:'bio',o:'delete',m:S.m,n:cur.i,sI});if(r)showBio(r.items)}
async function bioDelAll(){if(!confirm(inName(cur.i)+'의 동작을 모두 지울까요?'))return;const r=await api({c:'bio',o:'deleteAll',m:S.m,n:cur.i});if(r)showBio(r.items)}

// ---- 스케줄 (sch) ----
function openSch(i){
  cur={k:'sch',i};$('schTitle').textContent=outName(i);
  const tz=$('tz');if(![...tz.options].some(o=>+o.value===S.tz))tz.add(new Option('UTC '+(S.tz/3600),S.tz));tz.value=S.tz;
  $('schRm').value='daily';days=[];dayBtns();schStep();showSch([]);openM('mSch');schView();
}
function dayBtns(){const box=$('dayBtns');box.innerHTML='';DAYS.forEach((d,i)=>{const b=document.createElement('button');b.textContent=d;b.className=days.includes(i)?'sel':'';b.onclick=()=>{days=days.includes(i)?days.filter(x=>x!==i):[...days,i].sort();dayBtns()};box.appendChild(b)})}
function schStep(){show('schDays',$('schRm').value==='weekly')}
function showSch(items){
  items=[...items].sort((a,b)=>a.start-b.start||a.end-b.end||a.dw-b.dw);
  table('schList',3,'등록된 스케줄이 없습니다.',items,(it,k)=>[k+1,hms(it.start)+' ~ '+hms(it.end)+'  '+(it.rm==='weekly'?DAYS[it.dw]+'요일':'매일'),delBtn(()=>schDel(it.index))]);
}
async function schView(){const r=await api({c:'sch',o:'list',m:S.m,pi:cur.i});if(r)showSch(r.items)}
async function schSave(){
  const rm=$('schRm').value,o={c:'sch',o:'insert',m:S.m,pi:cur.i,start:toSec($('schS').value),end:toSec($('schE').value),rm};
  if(o.start===o.end){alert('시작 시간과 종료 시간이 같습니다.');return}
  if(rm==='weekly'){if(!days.length){alert('요일을 하나 이상 선택하세요.');return}o.dw=days}
  const r=await api(o);if(r)showSch(r.items);
  if(!S.time)alert('보드가 아직 시간을 모릅니다. 위의 동기화를 눌러 주세요. 시간을 알기 전에는 스케줄이 동작하지 않습니다.');
}
async function schDel(index){const r=await api({c:'sch',o:'delete',m:S.m,pi:cur.i,sI:index});if(r)showSch(r.items)}
async function schDelAll(){if(!confirm(outName(cur.i)+'의 스케줄을 모두 지울까요?'))return;const r=await api({c:'sch',o:'deleteAll',m:S.m,pi:cur.i});if(r)showSch(r.items)}
async function syncTime(){const tz=+$('tz').value;const r=await api({c:'st',m:S.m,epoch:Math.floor(Date.now()/1000)+tz,tz});if(r)setState(r)}

// ---- 센서 트리거 (bs) ----
const label=ts=>ts==='temp'?'온도':'습도';
const unit=ts=>ts==='temp'?'°C':'%';
function openBs(ts){
  cur={k:'bs',ts};$('bsTitle').textContent=label(ts)+' 센서 트리거';
  $('calLabel').textContent=label(ts)+' 보정 (선택: 값이 틀릴 때만 지금 실제 값을 입력)';$('cal').value='';$('cal').placeholder='예: '+(S[ts]!==undefined?S[ts]:'28.1');
  $('bsTr').value='';bsReset();bsStep();showBs([]);openM('mBs');bsView();
}
function bsReset(){$('bsTv').value='';$('bsD').value='';$('bsDu').value='';fillPorts($('bsPort'),'포트를 선택하세요');$('bsV').value=''}
function bsStep(){
  const t=$('bsTr').value;show('bsMore',t!=='');show('bsV',$('bsPort').value!=='');
  $('bsTvLabel').textContent=t==='1'?'상한 설정 (이 값 이상으로 올라가면)':'하한 설정 (이 값 이하로 내려가면)';
  $('bsTv').placeholder=(t==='1'?'상한':'하한')+'(기준 값, '+unit(cur.ts)+')';
}
function showBs(items){table('bsList',7,'등록된 트리거가 없습니다.',items,(it,k)=>[k+1,it.tr?'⬆️ 올라갈 때':'⬇️ 내려갈 때',it.tv+unit(cur.ts),secTxt(it.d,'-'),secTxt(it.du,'계속'),outName(it.n)+' '+(it.v?'켜짐':'꺼짐'),delBtn(()=>bsDel(it.sI))])}
async function bsView(){const r=await api({c:'bs',o:'list',m:S.m,ts:cur.ts});if(r)showBs(r.items)}
async function bsInsert(){
  const tr=$('bsTr').value,tv=$('bsTv').value,p=$('bsPort').value,v=$('bsV').value;
  if(tr===''){alert('트리거(⬆️/⬇️)를 선택하세요.');return}
  if(tv===''||p===''||v===''){alert('기준 값, 포트, 상태를 모두 입력하세요.');return}
  const r=await api({c:'bs',o:'insert',m:S.m,ts:cur.ts,tr:+tr,tv:Number(tv),d:num($('bsD').value),du:num($('bsDu').value),ps:[{m:S.m,n:+p,v:+v}]});
  if(r){showBs(r.items);$('bsTr').value='';bsReset();bsStep()}
}
async function bsDel(sI){const r=await api({c:'bs',o:'delete',m:S.m,ts:cur.ts,sI});if(r)showBs(r.items)}
async function bsDelAll(){if(!confirm(label(cur.ts)+' 트리거를 모두 지울까요?'))return;const r=await api({c:'bs',o:'deleteAll',m:S.m,ts:cur.ts});if(r)showBs(r.items)}
async function saveCal(){
  const v=$('cal').value;
  if(v===''){  // 보정은 선택: 비어 있으면 보정하지 않고, 트리거를 채워 두었다면 그대로 등록한다
    if($('bsTr').value!=='')return bsInsert();
    alert('보정값이 비어 있어 보정하지 않았습니다. 보정이 필요 없으면 비워 두셔도 됩니다.');return;
  }
  const r=await api({c:'bs',o:'cali',m:S.m,ts:cur.ts,v:Number(v)});
  if(r){setState(r);$('cal').value='';alert(label(cur.ts)+'를 '+v+unit(cur.ts)+'로 보정했습니다.')}
}

// ---- 이름 (⚙) ----
function openNames(){
  if(!S)return;const g=$('nameGrid');g.innerHTML='';
  S.name.forEach((n,k)=>{
    const lab=k===0?'메인 기기 이름':(k<=nIn()?'입력 '+k:'출력 '+(k-nIn()));
    const d=document.createElement('div');d.innerHTML='<label></label><input type="text" maxlength="20">';
    d.querySelector('label').textContent=lab;d.querySelector('input').value=n;g.appendChild(d);
  });
  show('nameCard');
}
async function saveNames(){const a=[...$('nameGrid').querySelectorAll('input')].map(i=>i.value);const r=await api({c:'name',m:S.m,name:a});if(r){setState(r);hide('nameCard')}}

document.addEventListener('keydown',e=>{if(e.key==='Escape')closeM()});
poll();setInterval(poll,1000);
</script>
</body></html>
)rawliteral";
