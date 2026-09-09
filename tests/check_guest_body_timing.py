#!/usr/bin/env python3
"""Compare complete, three-sample exact-ROM guest transition measurements.
Usage: python3 tests/check_guest_body_timing.py baseline.json candidate.json
Input is the retained guest-timing row list, not host wall-clock durations.
"""
import json,statistics,sys
names=('first','next','reveal','hide','reduced','arabic')
def load(path):
 rows=json.load(open(path))
 result={}
 for name in names:
  selected=[r for r in rows if r['name'] in [f'{name}-{i}' for i in range(3)]]
  assert len(selected)==3 and len({r['name'] for r in selected})==3,(path,name)
  for r in selected:
   assert r['response_frame_intervals_rounded']==round(r['response_cycles']/280896),r
  result[name]=selected
 return result
baseline,candidate=map(load,sys.argv[1:])
failed=[]
for name in names:
 b=[r['response_frame_intervals_rounded'] for r in baseline[name]]
 c=[r['response_frame_intervals_rounded'] for r in candidate[name]]
 print(f'{name}: baseline median/worst {statistics.median(b)}/{max(b)}; candidate {statistics.median(c)}/{max(c)}')
 if statistics.median(c)>statistics.median(b) or max(c)>max(b):failed.append(name)
assert not failed,'Additional response intervals: '+', '.join(failed)
print('PASS all six guest transition frame budgets')
