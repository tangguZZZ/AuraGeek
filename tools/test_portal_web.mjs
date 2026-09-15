// Execute the production inline script with an isolated DOM/fetch harness.
// This is a logic regression, not a substitute for mobile browser/hardware QA.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html = fs.readFileSync(new URL('../web/portal.html', import.meta.url), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
// Daily physical controls stay on the device; the portal must not duplicate them.
assert.doesNotMatch(html,/\bid="(?:brightness|volume|transition|spectrum)"/);
class Element {
  set id(value) { this._id=value;elements.set(value,this); } get id() { return this._id; }
  value = ''; checked = false; disabled = false; required = false; textContent = '';
  children = []; style = { setProperty() {} }; classList = { toggle() {}, remove() {} };
  attributes = new Map(); listeners = new Map(); focused = false; scrolled = false;
  addEventListener(type,fn) { const list=this.listeners.get(type)||[];list.push(fn);this.listeners.set(type,list); }
  emit(type) { for(const fn of this.listeners.get(type)||[])fn(); }
  setAttribute(key,value) { this.attributes.set(key,value); } getAttribute(key) { return this.attributes.get(key)||null; }
  removeAttribute(key) { this.attributes.delete(key); } before() {} after(node) { this.adjacent=node; }
  closest() { return get('appearance'); } scrollIntoView() { this.scrolled=true; }
  focus() { assert.equal(this.disabled,false,'Error focus must happen after unlocking');this.focused=true; } reportValidity() {} showModal() {} close() {} click() {}
  checkValidity() { return this.disabled || !this.required || String(this.value).trim() !== ''; }
  replaceChildren(...children) { this.children = children; }
  append(...children) { this.children.push(...children); }
  set innerHTML(_) { throw new Error('Unsafe HTML sink used'); }
}
const elements = new Map();
const get = id => { if (!elements.has(id)) {const element=new Element();element.id=id;} return elements.get(id); };
let request = async () => { throw new Error('offline'); };
let confirmChoice=true;const confirmations=[];
const timers = new Map(); let timerId = 0;
const documentEvents=new Map();const windowEvents=new Map();
const context = vm.createContext({
  document: { getElementById: get, querySelector: selector=>get(selector),querySelectorAll: () => [], createElement: () => new Element(),
    documentElement: { dataset: {} }, addEventListener(type,fn) {const list=documentEvents.get(type)||[];list.push(fn);documentEvents.set(type,list);} },
  window: { innerWidth:390,addEventListener(type,fn) {const list=windowEvents.get(type)||[];list.push(fn);windowEvents.set(type,list);}, scrollTo() {} },
  fetch: (...args) => request(...args), AbortController, TextEncoder, Blob, URL,
  confirm: text => { confirmations.push(text);return confirmChoice; },
  setTimeout: (fn, ms) => { timers.set(++timerId, { fn, ms }); return timerId; },
  clearTimeout: id => timers.delete(id),
});
const run = code => vm.runInContext(code, context);
const flush = async () => { for (let i = 0; i < 4; ++i) await new Promise(setImmediate); };
vm.runInContext(script, context);
await flush();
assert.equal(run('loaded'), false);
assert.match(get('notice').textContent, /不会切换为演示模式/);
for(const id of ['name','city','serviceToken','stockSymbol0','stockFast'])
  assert.equal(get(id).disabled,true,`${id} must wait for the first device snapshot`);
assert.equal(get('save').disabled,true);
assert.equal(get('import').disabled,true);
assert.equal(get('ssid').disabled,false); // Wi-Fi setup is independent of web-config loading.
get('mobile').onclick();assert.equal(get('mobile').getAttribute('aria-expanded'),'true');
assert.equal(get('.layout').inert,true);
assert.equal(get('navBackdrop').hidden,false);
get('navBackdrop').onclick();assert.equal(get('navBackdrop').hidden,true);
assert.equal(get('.layout').inert,false);
assert.equal(get('mobile').getAttribute('aria-expanded'),'false');assert.equal(get('mobile').focused,true);
get('mobile').onclick();run("go('appearance')");assert.equal(get('navBackdrop').hidden,true);
get('mobile').onclick();let escaped=false;
for(const fn of documentEvents.get('keydown'))fn({key:'Escape',preventDefault(){escaped=true;}});
assert.equal(escaped,true);assert.equal(get('mobile').getAttribute('aria-expanded'),'false');
get('mobile').onclick();
for(const fn of windowEvents.get('resize'))fn();
assert.equal(get('.layout').inert,true); // A phone-size resize keeps its menu open.
run('window.innerWidth=1280');
for(const fn of windowEvents.get('resize'))fn();
assert.equal(get('.layout').inert,false);
assert.equal(get('navBackdrop').hidden,true);
assert.equal(get('mobile').getAttribute('aria-expanded'),'false');
const config = {
  schema: 1, identity: { name: 'AuraGeek' },
  appearance: { background: '#050505', foreground: '#f2f2f2', accent: '#dfff00' },
  location: { label: 'SHENZHEN', latitude: 22.5431, longitude: 114.0579, utc_offset_minutes: 480 },
  conversation: { wake_enabled: true, wake_greeting: true, continuous: true, silence_ms: 900, no_speech_ms: 8000, max_speech_seconds: 20 },
  service: { mode: 'official', url: '', version: 1 },
  stocks: {symbols:['105.QQQ','107.VOO'],ma_fast:20,ma_slow:55,show_fast:true,show_slow:true},
};
const ok = data => ({ ok: true, json: async () => data });
// A failed explicit first read must leave editors locked; a later successful read
// must unlock after the exclusive transaction releases its own temporary lock.
await run('reloadButton.onclick()');
assert.equal(get('name').disabled,true);
assert.equal(get('stockSymbol0').disabled,true);
request = async () => ok({ revision: 7, config });
await run('reloadButton.onclick()');
assert.equal(run('configRevision'), 7);
assert.equal(get('name').disabled,false);
assert.equal(get('save').disabled,false);assert.equal(get('import').disabled,false);
assert.equal(run('collect().location.latitude'), 22.5431);
assert.equal(run('collect().stocks.symbols.length'),2);
assert.equal(get('stockDown0').disabled,false);assert.equal(get('stockUp0').disabled,true);
let stockRequests=0;request=async()=>{++stockRequests;throw new Error('Invalid stocks must not reach device');};
get('stockSymbol1').value='105.QQQ';await get('save').onclick();
assert.equal(stockRequests,0);assert.match(get('stockSymbol1').adjacent.textContent,/重复/);
assert.equal(get('stockSymbol1').focused,true);
get('stockSymbol1').value='1.600519';get('stockSymbol1').emit('input');
get('stockDown0').onclick();assert.equal(get('stockSymbol0').value,'1.600519');
assert.equal(run('dirty'),true);assert.equal(stockRequests,0);
get('stockSymbol2').value='';get('stockSymbol3').value='116.00700';
assert.equal(run('collect().stocks.symbols[2]'),'116.00700');
get('stockFast').value='55';get('stockSlow').value='20';await get('save').onclick();
assert.match(get('stockSlow').adjacent.textContent,/必须大于/);assert.equal(stockRequests,0);
for(const value of ['105.qqq','105.QQQ?x=1','测试<script>',' 105.QQQ','105.A..B'])assert.equal(run(`validStockSymbol(${JSON.stringify(value)})`),false);
run('populateStocks({})');assert.equal(get('stockSymbol0').disabled,true);
assert.equal(run('collect().stocks'),undefined); // Old firmware remains usable without silently adding unsupported fields.
run(`populateStocks(${JSON.stringify(config)})`);
get('stockDefaults').onclick();assert.equal(get('stockSymbol0').value,'105.QQQ');
run("showStockPolicy({paused:true})");assert.match(get('stockPolicyStatus').textContent,/暂停/);
run("showStockPolicy({paused:false,budget_day:20260912,budget_used:4,daily_limit:12,next_allowed_epoch:0})");assert.match(get('stockPolicyStatus').textContent,/4 \/ 12/);
assert.equal(stockRequests,0);
let illegalSaveRequests=0;
request=async()=>{++illegalSaveRequests;throw new Error('Invalid local input must not reach device');};
get('name').value='小陈 AuraGeek-1';get('name').emit('input');
assert.equal(run("bytes($('name').value)"),17);
get('name').value='测试<script>';get('name').emit('input');
await get('save').onclick();
assert.equal(illegalSaveRequests,0);
assert.equal(get('name').getAttribute('aria-invalid'),'true');
assert.match(get('name').adjacent.textContent,/名称含不允许的字符/);
assert.equal(get('name').focused,true);assert.equal(get('name').scrolled,true);
assert.match(get('saveState').textContent,/未保存/);
assert.equal(get('name').value,'测试<script>'); // Keep the draft; do not silently sanitize.
assert.doesNotMatch(get('notice').textContent,/名称含不允许/); // Field errors are not global notices.
run("go('stocks')");
assert.equal(run('currentPage'),'stocks');
assert.match(get('fieldSummaryText').textContent,/首页个性化有待修正项/);
assert.equal(get('reviewFields').textContent,'返回修改');
assert.equal(get('name').value,'测试<script>');
get('reviewFields').onclick();
assert.equal(run('currentPage'),'appearance');assert.equal(get('name').focused,true);
assert.equal(get('reviewFields').textContent,'定位错误');
run("notice('连接中断，请重新连接热点',true);go('conversation')");
assert.equal(get('notice').textContent,'连接中断，请重新连接热点'); // Real connection errors remain global.
get('name').value='AuraGeek';get('name').emit('compositionend');
assert.equal(get('name').getAttribute('aria-invalid'),null);
assert.equal(get('name').adjacent.hidden,true);
assert.equal(get('fieldSummary').hidden,true);
get('name').value='';await get('save').onclick();
assert.match(get('name').adjacent.textContent,/不能为空/);
run("go('stocks')");assert.equal(run('currentPage'),'stocks');
assert.match(get('fieldSummaryText').textContent,/首页个性化有待修正项/);
assert.equal(get('name').value,'');
get('name').value='AuraGeek';get('name').emit('input');
assert.equal(get('fieldSummary').hidden,true);
for(const name of ['小陈 AuraGeek-1','AuraGeek (S3)','陈·小陈','小陈（测试）'])assert.equal(run(`validLabel(${JSON.stringify(name)},true)`),true);
for(const name of ['   ','---','A<script>','陈😀','A\u200b','A\u202e',' AURA','AURA '])assert.equal(run(`validLabel(${JSON.stringify(name)},true)`),false);
assert.equal(run("validLabel('深圳',false)"),false);
assert.equal(run("validLabel('Taipei-01',false)"),true);
assert.doesNotThrow(()=>run("validateWifi('家庭 Wi-Fi 😀','88888888')"));
assert.throws(()=>run("validateWifi('家庭 Wi-Fi','中文密码12345678')"),/密码/);
assert.throws(()=>run("validateWifi('x'.repeat(33),'88888888')"),/名称/);
assert.throws(()=>run("validateWifi('Wi-Fi\\u202e','88888888')"),/名称/);
for(const url of ['wss://example.org/voice','wss://example.org:443/voice','wss://example.org/%E4%B8%AD'])assert.equal(run(`validServiceUrl(${JSON.stringify(url)})`),true);
for(const url of ['https://example.org','wss://-bad.example','wss://example..org','wss://example.org/中文','wss://example.org/%xx','wss://example.org:0/','wss://example.org/?token=abc'])assert.equal(run(`validServiceUrl(${JSON.stringify(url)})`),false);
get('latitude').value = '';
assert.throws(() => run('collect()'), /输入范围/);
await get('save').onclick();
assert.equal(get('latitude').getAttribute('aria-invalid'),'true');
assert.equal(get('latitude').focused,true);
assert.match(get('latitude').adjacent.textContent,/纬度.*-90.*90/);
get('latitude').value = '22.5431';
run('collect()');assert.equal(get('latitude').getAttribute('aria-invalid'),null);
get('foreground').value = '#050505';
assert.throws(() => run('collect()'), /对比度/);
await get('save').onclick();assert.equal(get('foreground').getAttribute('aria-invalid'),'true');
get('foreground').value = '#f2f2f2';
get('noSpeech').value = '20000'; get('maxSpeech').value = '10';
assert.throws(() => run('collect()'), /单句上限/);
await get('save').onclick();assert.equal(get('noSpeech').getAttribute('aria-invalid'),'true');
assert.equal(get('foreground').getAttribute('aria-invalid'),null);
get('noSpeech').value = '8000'; get('maxSpeech').value = '20';
get('name').value = '陈'.repeat(11);
assert.throws(() => run('collect()'), /32字节/);
get('name').value = 'AuraGeek';
assert.equal(run("contrast('#000000','#ffffff')"), 21);
let posted;
request = async (path, options) => { posted = options; return { ok: false, json: async () => ({ error: 'revision conflict' }) }; };
await assert.rejects(run("api('/api/config',collect())"), /revision conflict/);
assert.equal(posted.headers['X-AuraGeek-Revision'], '7');
assert.equal(run('configRevision'), 7); // Failed POST must not advance the snapshot.
run('configRevision=null');
await assert.rejects(run("api('/api/config',collect())"), /先读取/);
run('configRevision=7');
let scanCalls = 0, resolveScan;
request = async () => { ++scanCalls; return new Promise(resolve => { resolveScan = resolve; }); };
get('scan').onclick(); get('scan').onclick();
assert.equal(scanCalls, 1);
resolveScan(ok({ pending: true })); await flush();
assert.equal(get('scan').disabled, true);
assert.equal(run('scanActive'), true);
get('scan').onclick(); assert.equal(scanCalls, 1);
const next = [...timers.values()].find(t => t.ms === 1500);
assert.ok(next);
const maliciousSsid = '<img src=x onerror=alert(1)>';
request = async () => ok({ pending: false, networks: [{ ssid: maliciousSsid, rssi: -40, secure: true }] });
await next.fn();
assert.equal(get('scan').disabled, false);
assert.equal(run('scanActive'), false);
assert.equal(get('networks').children[0].children[0].textContent, maliciousSsid);
request = async () => { throw new Error('scan failed'); };
get('scan').onclick(); await flush();
assert.equal(get('scan').disabled, false);
assert.equal(run('scanActive'), false);
assert.match(get('notice').textContent, /scan failed/);
const importChanges=[{field:'conversation.continuous',before:true,after:false},{field:'service.token',before:'未设置',after:'替换为新值（隐藏）'}];
get('importText').value=JSON.stringify({schema:1,conversation:{continuous:false},service:{token:'secret-not-in-confirmation'}});
const calls=[];
request=async(path,options)=>{
  calls.push(path);
  if(path==='/api/config/validate'){
    assert.equal(options.headers['X-AuraGeek-Revision'],'7');
    return ok({revision:7,changes:importChanges});
  }
  if(options.method==='POST')return ok({ok:true,message:'saved',revision:8});
  return ok({revision:8,config});
};
confirmChoice=false;await get('import').onclick();
assert.deepEqual(calls,['/api/config/validate']);
assert.match(confirmations.at(-1),/连续聊天/);
assert.doesNotMatch(confirmations.at(-1),/secret-not-in-confirmation/);
assert.match(get('notice').textContent,/已取消/);
calls.length=0;confirmChoice=true;await get('import').onclick();
assert.deepEqual(calls,['/api/config/validate','/api/config','/api/config']);
assert.equal(get('importText').value,'');assert.equal(run('configRevision'),8);
get('importText').value='{"schema":1}';calls.length=0;
request=async(path)=>{calls.push(path);return ok({revision:8,changes:[]});};
await get('import').onclick();assert.deepEqual(calls,['/api/config/validate']);
assert.match(get('notice').textContent,/无需保存/);
calls.length=0;request=async(path)=>{calls.push(path);return ok({revision:9,changes:importChanges});};
await get('import').onclick();assert.deepEqual(calls,['/api/config/validate']);
assert.match(get('notice').textContent,/校验结果不匹配/);
// Save locks editors across both POST and readback, but validates before locking.
run('populate(lastConfig)');
let resolveWrite,resolveRead;
request=async(path,options)=>new Promise(resolve=>{
  if(options.method==='POST')resolveWrite=resolve;else resolveRead=resolve;
});
get('serviceToken').disabled=true; // Preserve independently disabled controls.
const saving=get('save').onclick();
assert.equal(get('name').disabled,true);
assert.equal(get('stockSymbol0').disabled,true);assert.equal(get('stockDown0').disabled,true);
assert.equal(get('importText').disabled,true);
assert.equal(get('ssid').disabled,true);
assert.equal(run('busy'),true);
await get('save').onclick(); // A second save must not start another transaction.
resolveWrite(ok({ok:true,message:'saved',revision:9}));await flush();
assert.equal(get('name').disabled,true);
resolveRead(ok({revision:9,config}));await saving;
assert.equal(get('name').disabled,false);
assert.equal(get('stockSymbol0').disabled,false);assert.equal(get('stockDown0').disabled,false);
assert.equal(get('importText').disabled,false);
assert.equal(get('serviceToken').disabled,true);
assert.equal(run('busy'),false);
get('serviceToken').disabled=false;
let interruptedRequests=0;
request=async()=>{++interruptedRequests;throw new Error('save interrupted');};
get('name').value='Keep my draft';
await get('save').onclick();
assert.equal(get('name').value,'Keep my draft');
assert.equal(get('name').disabled,false);
assert.match(get('notice').textContent,/保存结果尚未确认/);
assert.equal(run('writeRecovery'),'unknown');
assert.equal(run('dirty'),true);
assert.equal(get('save').disabled,true);assert.equal(get('import').disabled,true);
await get('save').onclick();await get('import').onclick();
assert.equal(interruptedRequests,1); // Never automatically or manually re-POST an uncertain write.
get('name').emit('input');assert.match(get('saveState').textContent,/保存结果尚未确认/);
confirmChoice=false;await run('reloadButton.onclick()');
assert.equal(interruptedRequests,1);assert.equal(get('name').value,'Keep my draft');
confirmChoice=true;request=async()=>ok({revision:9,config});
await run('reloadButton.onclick()');assert.equal(run('writeRecovery'),'');
assert.equal(get('save').disabled,false);
// An acknowledged POST followed by a failed GET is not a failed save.
let readbackCalls=[];
request=async(path,options)=>{readbackCalls.push(options.method);if(options.method==='POST')return ok({ok:true,message:'saved',revision:10});throw new Error('readback lost');};
get('name').value='Saved draft';get('name').emit('input');
await get('save').onclick();
assert.deepEqual(readbackCalls,['POST','GET']);
assert.equal(run('writeRecovery'),'saved');assert.match(get('notice').textContent,/设备已确认保存/);
assert.equal(get('name').value,'Saved draft');assert.equal(get('save').disabled,true);
request=async()=>{throw new Error('still disconnected');};
await run('reloadButton.onclick()');assert.equal(run('writeRecovery'),'saved');
assert.equal(get('name').value,'Saved draft');
request=async()=>ok({revision:10,config});await run('reloadButton.onclick()');
// Explicit validation rejection is safe to correct, but a conflict needs a fresh snapshot.
request=async()=>({ok:false,status:400,json:async()=>({error:'device validation rejected'})});
await get('save').onclick();assert.equal(run('writeRecovery'),'');assert.equal(get('save').disabled,false);
assert.match(get('notice').textContent,/device validation rejected/);
request=async()=>({ok:false,status:409,json:async()=>({error:'revision conflict'})});
await get('save').onclick();assert.equal(run('writeRecovery'),'conflict');assert.equal(get('save').disabled,true);
request=async()=>ok({revision:9,config});await run('reloadButton.onclick()');
// A malformed success acknowledgement or server-side failure cannot prove a commit.
for(const response of [ok({message:'missing acknowledgement',revision:10}),
  ok({ok:true,message:'wrong revision',revision:90}),
  {ok:false,status:500,json:async()=>({error:'storage readback failed'})}]){
  let writes=0;request=async()=>{++writes;return response;};
  await get('save').onclick();assert.equal(writes,1);
  assert.equal(run('writeRecovery'),'unknown');assert.equal(get('save').disabled,true);
  request=async()=>ok({revision:9,config});await run('reloadButton.onclick()');
}
// Imports share the same commit recovery; don't clear pasted content on readback failure.
const retainedImport='{"schema":1,"conversation":{"continuous":false}}';
get('importText').value=retainedImport;get('name').value='Unsaved form';get('name').emit('input');
request=async(path,options)=>{
  if(path==='/api/config/validate')return ok({revision:9,changes:importChanges});
  if(options.method==='POST')return ok({ok:true,revision:10,message:'saved'});
  throw new Error('readback unavailable');
};
await get('import').onclick();assert.match(confirmations.at(-1),/表单另有未保存修改/);
assert.equal(get('importText').value,retainedImport);assert.equal(get('name').value,'Unsaved form');
assert.equal(run('writeRecovery'),'saved');assert.equal(get('import').disabled,true);
request=async()=>ok({revision:9,config});await run('reloadButton.onclick()');
get('name').value='';let invalidWrites=0;
request=async()=>{++invalidWrites;return ok({});};
await get('save').onclick();assert.equal(invalidWrites,0); // Locking must not bypass browser required-field checks.
get('name').value='AuraGeek';get('serviceMode').value='official';
request=async(path,options)=>{
  assert.equal(path,'/api/ai/test');assert.equal(options.headers['X-AuraGeek-Revision'],'9');
  const body=JSON.parse(options.body);assert.deepEqual(Object.keys(body).sort(),['schema','service']);
  return ok({message:'started',ai_test:{active:true,state:'connect',message:'connecting',http_status:0}});
};
await get('testAi').onclick();assert.equal(get('testAi').disabled,true);
assert.equal(run('configRevision'),9); // Diagnostic never advances or saves configuration.
run("showAiTest({active:false,state:'passed',message:'hello passed',http_status:101})");
assert.equal(get('testAi').disabled,false);assert.match(get('aiTestStatus').textContent,/101/);
// Reconnect never reads the password from a form or sends a password to the API.
run("showNetworkStatus({connection:'idle',saved_network:{available:false,ssid:''}})");
assert.equal(get('reconnect').disabled,true);
run(`showNetworkStatus({connection:'idle',saved_network:{available:true,ssid:${JSON.stringify(maliciousSsid)}}})`);
assert.equal(get('reconnect').disabled,false);
assert.ok(get('savedNetworkStatus').textContent.includes(maliciousSsid));
get('ssid').value='Unfinished new network';get('wifiPassword').value='not-sent-in-reconnect';
request=async(path,options)=>{
  assert.equal(path,'/api/wifi/reconnect');assert.equal(options.body,'{}');
  assert.equal(options.method,'POST');assert.ok('X-AuraGeek-Token' in options.headers);
  return ok({message:'connecting saved network'});
};
await get('reconnect').onclick();
assert.equal(run('networkOperation'),true);assert.equal(get('reconnect').disabled,true);
assert.equal(get('scan').disabled,true);assert.equal(get('connect').disabled,true);
assert.equal(get('ssid').value,'Unfinished new network');
assert.equal(get('wifiPassword').value,'not-sent-in-reconnect');
run("showNetworkStatus({connection:'failed',saved_network:{available:true,ssid:'Old network'}})");
assert.equal(get('reconnect').disabled,false);assert.equal(get('scan').disabled,false);
run("showNetworkStatus({connection:'connected_existing',saved_network:{available:true,ssid:'Old network'}})");
assert.equal(run('networkOperation'),false);
// Status responses started before a user action cannot roll back newer UI state.
let resolveStatus;
request=async()=>new Promise(resolve=>{resolveStatus=resolve;});
const polling=run('poll()');
run('++actionEpoch;networkOperation=true');
resolveStatus(ok({connection:'idle',saved_network:{available:false}}));await polling;
assert.equal(run('networkOperation'),true);assert.equal(run('savedAvailable'),true);
run('networkOperation=false');
// A late background first-read response must not replace a newer explicit read
// or the draft typed after that newer action completed.
let resolveOldConfig;
request=async()=>new Promise(resolve=>{resolveOldConfig=resolve;});
const oldRead=run('refreshConfig()');
request=async()=>ok({revision:10,config});
await run('exclusive(refreshConfig)');
get('name').value='New draft';get('name').emit('input');
resolveOldConfig(ok({revision:9,config:{...config,identity:{name:'Stale value'}}}));
assert.equal(await oldRead,false);
assert.equal(get('name').value,'New draft');assert.equal(run('dirty'),true);
assert.equal(run('configRevision'),10);
// A stale status failure must not overwrite a more recent successful notice.
let rejectOldStatus;
request=async()=>new Promise((resolve,reject)=>{rejectOldStatus=reject;});
const oldStatus=run('poll()');
run("++actionEpoch;notice('new action succeeded')");
rejectOldStatus(new Error('old connection failed'));await oldStatus;
assert.equal(get('notice').textContent,'new action succeeded');
// Reset is a distinct confirmed transaction; invalid drafts need not validate.
// A second phone/desktop saving must warn before this tab attempts a write.
run("token='session-a';writeRecovery='';sessionChanged=false");
get('name').value='Retained draft';get('importText').value='{"schema":1}';
run("observeConfigVersion({token:'session-a',revision:11})");
assert.equal(get('name').value,'Retained draft');assert.equal(get('importText').value,'{"schema":1}');
assert.equal(run('configRevision'),10);assert.equal(run('writeRecovery'),'external');
assert.equal(get('save').disabled,true);assert.match(get('notice').textContent,/另一网页/);
run("go('stocks')");assert.match(get('notice').textContent,/另一网页/);
request=async()=>ok({revision:11,token:'session-a',config});await run('reloadButton.onclick()');
assert.equal(run('writeRecovery'),'');assert.equal(get('save').disabled,false);
// Session replacement also gates non-versioned operations (Wi-Fi and exit).
get('name').value='Restart draft';run("observeConfigVersion({token:'session-b',revision:12})");
assert.equal(run('sessionChanged'),true);assert.equal(get('connect').disabled,true);
assert.equal(get('exit').disabled,true);assert.equal(get('factoryOpen').disabled,true);
let stalePosts=0;request=async()=>{++stalePosts;return ok({});};
await assert.rejects(run("api('/api/wifi/connect',{})"),/设备已重启/);assert.equal(stalePosts,0);
assert.equal(get('name').value,'Restart draft');
// Keep stronger lost-write evidence rather than replacing it with a generic conflict.
run("writeRecovery='unknown';observeConfigVersion({token:'session-b',revision:12})");
assert.equal(run('writeRecovery'),'unknown');
request=async()=>ok({revision:10,token:'session-b',config});await run('reloadButton.onclick()');
assert.equal(run('sessionChanged'),false);assert.equal(run('token'),'session-b');
assert.equal(get('connect').disabled,false);assert.equal(get('exit').disabled,false);
run("observeConfigVersion({token:'session-b',revision:10})");assert.equal(run('writeRecovery'),'');
run('factorySupported=false;updateFactory()');assert.equal(get('factoryOpen').disabled,true);
run('factorySupported=true;writeRecovery="";loaded=true;updateFactory()');
get('factoryOpen').onclick();assert.equal(get('factoryText').value,'');assert.equal(get('factoryAck').checked,false);
let resetRequests=0;request=async()=>{++resetRequests;return ok({ok:true});};
await get('factoryConfirm').onclick();assert.equal(resetRequests,0);assert.equal(get('factoryError').hidden,false);
get('factoryText').value='恢复出厂';await get('factoryConfirm').onclick();assert.equal(resetRequests,0);
get('factoryAck').checked=true;
run('networkOperation=true');await get('factoryConfirm').onclick();assert.equal(resetRequests,0);run('networkOperation=false');
run('busy=true;updateFactory()');assert.equal(get('factoryCancel').disabled,true);assert.equal(get('factoryText').disabled,true);assert.equal(get('factoryAck').disabled,true);run('busy=false;updateFactory()');
request=async(path,options)=>{++resetRequests;assert.equal(path,'/api/factory-reset');assert.equal(options.headers['X-AuraGeek-Revision'],'10');assert.deepEqual(JSON.parse(options.body),{confirmation:'恢复出厂',preserve_official_binding:true});return{ok:false,status:409,json:async()=>({error:'配置已变化'})};};
await get('factoryConfirm').onclick();assert.equal(resetRequests,1);assert.equal(run('exiting'),false);
get('name').value='测试<script>';request=async()=>{++resetRequests;return ok({ok:true});};
await get('factoryConfirm').onclick();assert.equal(resetRequests,2);assert.equal(run('exiting'),true);
await get('factoryConfirm').onclick();assert.equal(resetRequests,2);
assert.match(get('notice').textContent,/官方 AI 绑定保留/);
for(const id of ['name','ssid','save','import'])assert.equal(get(id).disabled,true);
// Lost acknowledgement must lock the page instead of automatically resubmitting.
run('exiting=false;loaded=true;updateFactory()');request=async()=>{throw new Error('lost response');};
await get('factoryConfirm').onclick();assert.equal(run('exiting'),true);assert.match(get('notice').textContent,/请勿重复提交/);
run('exiting=false;loaded=true;updateFactory()');
// Exiting is terminal for this browser page; do not re-enable form controls.
request=async()=>ok({message:'exiting'});
await get('exit').onclick();
assert.equal(run('exiting'),true);
for(const id of ['name','ssid','stockSymbol0','save','import','importText'])assert.equal(get(id).disabled,true);
assert.doesNotMatch(script, /\.innerHTML\s*=/);
console.log('PASS portal web: validation, revisions, scan recovery, safe text, import preview, save locking, uncertain/confirmed-write recovery, initial-load recovery, stale-config/status protection, exit locking and saved-WiFi reconnect');
