"""Import an explicitly fetched reference-server snapshot, not synthetic prices.

This cache is labeled REF CACHE and is never merged into Eastmoney raw closes.
Used for offline first boot and simulator preview when the primary API is down.
"""
import json
from pathlib import Path
import math
import sys

root=Path(__file__).resolve().parents[1]
payload=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
cache=root/'simulator/cache'
cache.mkdir(exist_ok=True)
lines=['// Generated from reference snapshot; do not mix with unadjusted Eastmoney data.',
       '#include "services/StockSeed.h"','namespace aurageek { namespace services {']
for ticker in ('QQQ','VOO'):
    rows=payload['tickers'][ticker]['rows']
    valid=[]
    for row in rows:
        date=int(row[0].replace('-','')); price=float(row[1])
        if not (math.isfinite(price) and price>0):raise ValueError('Invalid price')
        if valid and date<=valid[-1][0]:raise ValueError('Unsorted/duplicate daily bar')
        valid.append((date,price))
    # Preserve primary cache files; reference data has its own source-specific files.
    (cache/(ticker+'.reference.json')).write_text(json.dumps({'source':'reference dashboard snapshot; adjustment basis follows upstream','updated_at':payload['updatedAt'],'bars':valid}),encoding='utf-8')
    lines.append('const StockBar seed'+ticker+'[]={')
    lines.extend('{%du,%.6ff},'%r for r in valid)
    lines.extend(['};','const size_t seed'+ticker+'Count=sizeof(seed'+ticker+')/sizeof(StockBar);'])
lines.append('}}')
(root/'src/services/StockSeed.cpp').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Imported reference QQQ/VOO cache:',payload['updatedAt'])
