"""Serial, bounded Eastmoney raw-close cache updater. No host/proxy rotation.

Reference fallback is a separate immutable snapshot. Never merge it into raw
daily closes. Daily budgets and cooldowns survive restarts, including failures.
"""
import csv
import http.client
import json
import math
import msvcrt
import os
import random
import time
from pathlib import Path
from datetime import datetime, timedelta, timezone
from email.utils import parsedate_to_datetime
from urllib.parse import urlencode

BASE=Path(__file__).resolve().parent/'cache'
TZ=timezone(timedelta(hours=8))
HOST='push2his.eastmoney.com'
HEADERS={'User-Agent':'AuraGeek/1.0 (Windows; daily-cache)',
         'Referer':'https://quote.eastmoney.com/','Accept':'application/json',
         'Accept-Encoding':'identity','Connection':'keep-alive'}

def atomic_json(path,data):
    tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');os.replace(tmp,path)

def retry_seconds(value):
    if not value:return 0
    try:return max(0,int(value))
    except ValueError:
        try:return max(0,int(parsedate_to_datetime(value).timestamp()-time.time()))
        except (ValueError,TypeError):return 0

def merge_payload(payload,history,today):
    if payload.get('rc')!=0 or not isinstance(payload.get('data'),dict):raise ValueError('Invalid envelope')
    rows=payload['data'].get('klines')
    if not isinstance(rows,list):raise ValueError('Missing daily rows')
    merged=dict(history)
    for row in rows:
        date,value=row.split(',')[:2];datetime.strptime(date,'%Y-%m-%d');close=float(value)
        if not math.isfinite(close) or close<=0:raise ValueError('Invalid close')
        if date<today:merged[date]=close
    if not merged:raise ValueError('Empty history')
    return merged

def export_csv(ticker,history):
    path=BASE/(ticker+'.csv');temp=path.with_suffix('.tmp')
    with temp.open('w',newline='',encoding='utf-8') as f:csv.writer(f).writerows(sorted(history.items()))
    os.replace(temp,path)

def main():
    BASE.mkdir(exist_ok=True)
    with (BASE/'update.lock').open('a+b') as lock:
        lock.seek(0)
        try:msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1)
        except OSError:print('Updater already running; no request sent');return 1
        now=datetime.now(TZ);today=now.date().isoformat()
        if now.weekday() not in (1,2,3,4,5) or now.hour<9:
            print('Outside Tue-Sat 09:00+ Shanghai window; cache only');return 0
        connection=http.client.HTTPSConnection(HOST,timeout=20)
        failed=False
        try:
            for ticker,secid in (('QQQ','105.QQQ'),('VOO','107.VOO')):
                path=BASE/(ticker+'.json');state=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'history':{}}
                if state.get('attempt_date')!=today:state.update(attempt_date=today,attempts=0)
                # Migrate old daily-budget marker as one consumed attempt, not zero.
                used=state.get('attempts',1)
                if state.get('history'):export_csv(ticker,state['history'])
                while used<3 and time.time()>=state.get('next_attempt_utc',0):
                    used+=1;state.update(attempts=used,next_attempt_utc=time.time()+300,source='eastmoney raw fqt=0')
                    atomic_json(path,state)
                    history=state['history'];begin='0' if not history else max(history).replace('-','')
                    query=urlencode(dict(secid=secid,fields1='f1,f2,f3,f4,f5,f6',fields2='f51,f53',klt=101,fqt=0,beg=begin,end=20500101,lmt=10000))
                    code=0;retry_after=0
                    try:
                        connection.request('GET','/api/qt/stock/kline/get?'+query,headers=HEADERS)
                        response=connection.getresponse();code=response.status;retry_after=retry_seconds(response.getheader('Retry-After'))
                        body=response.read(900001)
                        if code!=200:raise ValueError(f'HTTP {code}')
                        if len(body)>900000:raise ValueError('Response size limit')
                        state['history']=merge_payload(json.loads(body),history,today)
                        state.update(status='EM RAW / DAILY',attempts=3,updated_at=now.isoformat())
                        atomic_json(path,state);export_csv(ticker,state['history']);print(ticker,len(state['history']),'raw daily bars cached');break
                    except (OSError,ValueError,http.client.HTTPException) as error:
                        failed=True;connection.close()
                        wait=max(retry_after,86400 if code==403 else 3600 if code==429 else 60 if used==1 else 300)
                        state.update(status='STALE / '+str(error),next_attempt_utc=time.time()+wait)
                        atomic_json(path,state);print(ticker,f'attempt {used}/3:',error,flush=True)
                        if code in (403,429) or used>=3:break
                        # Do not block the UI process or a terminal for minutes.
                        # Next manual/scheduled invocation honors the persisted cooldown.
                        print('Retry eligible after',wait,'seconds; cached UI remains available');break
                time.sleep(2+random.uniform(0,.5))
        finally:connection.close()
        return 1 if failed else 0

if __name__=='__main__':raise SystemExit(main())
