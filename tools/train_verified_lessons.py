#!/usr/bin/env python3
"""Certify bot-observer decisions after completed matches, outside gameplay."""
from pathlib import Path
import json,subprocess,os,argparse,tempfile,hashlib
ROOT=Path(__file__).resolve().parents[1]

def proposals(memory, include_synthetic=False, cancel=None):
    root=Path(str(memory)+'.experience')
    if not root.exists():return
    for episode in sorted(root.glob('*/episode.jsonl')):
        if cancel and cancel.is_set():return
        if episode.stat().st_size>4*1024*1024:continue
        try:rows=[json.loads(line) for line in episode.read_text().splitlines()]
        except (ValueError,OSError):continue
        if len(rows)<3:continue
        header,end=rows[0],rows[-1]
        if header.get('kind')!='real_game_start_v1' or not header.get('expert'):continue
        if header.get('source')!='human_vs_bot' and not include_synthetic:continue
        if end.get('kind')!='real_game_end_v1' or end.get('status')!='completed':continue
        # Human observer snapshots are never used as policy training inputs.
        demonstration=-1;eligible=[]
        for row in rows[1:-1]:
            if row.get('kind')!='real_game_decision_v1':continue
            if row.get('actor')==0:
                try:
                    action=[int(x) for x in row['action'].split(',')]
                    if action[0]==0:demonstration=action[1]
                except (KeyError,ValueError,IndexError):pass
            elif row.get('actor')==1 and row.get('controller')=='bot' and row.get('round',0)>0 and row.get('search',{}).get('iterations',0)>0:
                if isinstance(row.get('replay'),str) and isinstance(row.get('action'),str) and isinstance(row.get('position'),str):eligible.append((row,demonstration))
        # Bounded work, whole-match source identity retained. Uncertain labels
        # cannot be rescued by the eventual winner of this match.
        for row,demo in eligible[-4:]:yield row,demo

def atomic_json(path,data):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile('w',dir=path.parent,delete=False) as stream:
        json.dump(data,stream,ensure_ascii=False);stream.write('\n');name=stream.name
    os.replace(name,path)

def train(memory,binary,cancel=None,on_process=None,include_synthetic=False,maximum=4):
    memory=Path(memory);model=Path(str(memory)+'.verified-v1');ledger=Path(str(memory)+'.verified-progress.json');journal=Path(str(memory)+'.verified-reviews.jsonl')
    fingerprint=hashlib.sha256(Path(binary).read_bytes()).hexdigest()
    try:state=json.loads(ledger.read_text())
    except (ValueError,OSError):state={}
    # A changed verifier gets a new ledger; model-side dedup still protects
    # already accepted positions against duplicate and conflicting reviews.
    done=set(state.get('positions',[])) if state.get('verifier')==fingerprint else set()
    count=accepted=0
    for row,demo in proposals(memory,include_synthetic,cancel):
        if count>=maximum or (cancel and cancel.is_set()):break
        ident=row['position']
        if ident in done:continue
        if any('\t' in row[k] or '\n' in row[k] for k in ('replay','action','position')):continue
        request='\t'.join([row['replay'],row['action'],ident,str(demo)])+'\n'
        proc=subprocess.Popen([str(Path(binary).resolve()),str(model.resolve())],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=dict(os.environ,GWENT_EXPERIENCE_SOURCE='verified_review'))
        if on_process:on_process(proc)
        try:out,err=proc.communicate(request,timeout=120)
        except subprocess.TimeoutExpired:proc.kill();proc.communicate();continue
        finally:
            if on_process:on_process(None)
        if cancel and cancel.is_set():break
        if proc.returncode:continue
        try:result=json.loads(out)
        except ValueError:continue
        if result.get('position')!=ident:continue
        result.update(source='protocol_test' if include_synthetic else 'human_vs_bot',verifier=fingerprint)
        with journal.open('a') as stream:stream.write(json.dumps(result,ensure_ascii=False)+'\n')
        done.add(ident);count+=1;accepted+=bool(result.get('accepted'))
        atomic_json(ledger,{'schema':1,'verifier':fingerprint,'positions':sorted(done)[-8192:]})
    return {'reviewed':count,'accepted':accepted,'model':str(model)}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--memory',type=Path,required=True);parser.add_argument('--binary',type=Path,default=ROOT/'engine/verified_reviewer');parser.add_argument('--include-synthetic',action='store_true');parser.add_argument('--maximum',type=int,default=4);args=parser.parse_args()
    if not 1<=args.maximum<=16:parser.error('maximum must be 1..16')
    print(json.dumps(train(args.memory,args.binary,include_synthetic=args.include_synthetic,maximum=args.maximum)))
