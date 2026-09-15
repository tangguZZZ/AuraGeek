// Local-only browser QA fixture. Never imported or embedded by production firmware.
// No device connection, real credentials, disk persistence or permissive API fallback.
import http from 'node:http';
import fs from 'node:fs';
import {randomUUID} from 'node:crypto';
const token=randomUUID();
let revision=1;
let config={stocks:{symbols:['105.QQQ','107.VOO'],ma_fast:20,ma_slow:55,show_fast:true,show_slow:true},schema:1,identity:{name:'AuraGeek'},appearance:{background:'#120c1f',foreground:'#f4edff',accent:'#cfafff'},location:{label:'SHENZHEN',latitude:22.5431,longitude:114.0579,utc_offset_minutes:480},conversation:{wake_enabled:true,wake_greeting:true,continuous:true,silence_ms:900,no_speech_ms:8000,max_speech_seconds:20},service:{mode:'official',url:'',version:1}};
http.createServer(async(req,res)=>{
  res.setHeader('Cache-Control','no-store');
  const json=(status,data)=>{res.writeHead(status,{'Content-Type':'application/json; charset=utf-8'});res.end(JSON.stringify(data));};
  if(req.method==='GET'&&req.url==='/portal.html'){
    res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'});
    res.end(fs.readFileSync(new URL('../web/portal.html',import.meta.url),'utf8').replace('<title>AuraGeek','<title>LOCAL QA FIXTURE · AuraGeek'));return;
  }
  if(req.method==='GET'&&req.url==='/api/status')return json(200,{revision,factory_reset_preserve_binding:true,name:config.identity.name,token,firmware:'LOCAL QA FIXTURE - NOT A DEVICE',internal_free:100000,internal_largest:50000,psram_free:6000000,uptime_s:60,connection:'idle',stock_policy:{paused:false,budget_day:20260912,budget_used:4,daily_limit:12,next_allowed_epoch:0},saved_network:{available:false},ai_test:{active:false,message:'本地测试，不连接设备'}});
  if(req.method==='GET'&&req.url==='/api/config')return json(200,{revision,token,config});
  if(req.method==='POST'&&req.url==='/api/config'){
    if(req.headers['x-aurageek-token']!==token)return json(403,{error:'Fixture session changed'});
    if(req.headers['x-aurageek-revision']!==String(revision))return json(409,{error:'Fixture revision changed'});
    console.log('FIXTURE POST /api/config');
    let body='';for await(const chunk of req){body+=chunk;if(body.length>4096)return json(413,{error:'Too large'});}
    try{const next=JSON.parse(body);if(!/^[A-Za-z0-9\u3400-\u9fff _().·（）-]+$/u.test(next.identity.name))return json(400,{error:'Fixture rejected name'});config=next;++revision;return json(200,{ok:true,revision,message:'本地测试保存成功（未写入设备）'});}catch{return json(400,{error:'Invalid fixture request'});}
  }
  json(404,{error:'Fixture route not implemented'});
}).listen(8766,'127.0.0.1',()=>console.log('LOCAL QA ONLY http://127.0.0.1:8766/portal.html'));
