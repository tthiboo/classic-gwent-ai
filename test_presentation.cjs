// Regression checks for presentation decisions and field action routing.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const base=__dirname+'/';
const META=JSON.parse(fs.readFileSync(base+'web/assets/catalog.json','utf8'));
const id=name=>META.cards.find(c=>c.name===name).id;
const context={META,card:id=>META.cards[id],document:{addEventListener(){}},window:{addEventListener(){}},matchMedia:()=>({matches:false}),Set,Map};
vm.createContext(context);
vm.runInContext(fs.readFileSync(base+'web/effects.js','utf8')+';this.effects=GwentEffects;',context);
const unit=(name,row,index,skin=0,variant=0)=>({id:id(name),row,index,skin,variant,power:META.cards[id(name)].power});
const a=unit('Crinfrid Reavers Dragon Hunter',1,0),b={...a,index:1},c={...a,index:2,variant:1};
let diff=context.effects.difference([a,b,c],[{...a,index:8},c]);
assert.equal(diff.removed.length,1);assert.equal(diff.paired.length,2);assert.equal(diff.added.length,0);
assert.equal(context.effects.difference([a],[{...a,row:2}]).added.length,1);
const state=()=>({ready:true,winner:-1,round:0,weather:0,steps:5,turn:0,pending:-1,players:[0,1].map(()=>({leader:0,used:false,board:[]})),legal:[]});
let before=state(),after=state();before.players[1].board=[a,b];after.players[1].board=[a];
assert.equal(context.effects.plan(before,after,META.cards[id('Scorch')]).burn,true);
assert.equal(context.effects.plan(before,after,META.cards[id('Decoy')]).burn,false);
before.players[0].leader=2;after.players[0].leader=2;after.players[0].used=true;
assert.equal(context.effects.plan(before,after,null).burn,true);
after.round=1;assert.equal(context.effects.plan(before,after,META.cards[id('Scorch')]).burn,false);
before=state();after=state();before.weather=7;
let plan=context.effects.plan(before,after,META.cards[id('Clear Weather')]);assert.equal(plan.weatherRemoved,7);assert(plan.clear);
assert(context.effects.plan(state(),state(),META.cards[id('Clear Weather')]).clear);
after.weather=6;assert.equal(context.effects.plan(state(),after,META.cards[id('Skellige Storm')]).weatherAdded,6);
const source=fs.readFileSync(base+'web/app.js','utf8');
vm.runInContext(source.slice(source.indexOf('function weatherRows('),source.indexOf("document.addEventListener('click',e=>{if(e.target.closest('[data-use-leader]'))")),context);
Object.assign(context,{busy:false,state:state(),selected:id('Skellige Storm'),selectedHand:0,pendingPlacement:null});
context.state.legal=[{kind:0,card:id('Skellige Storm'),target:-1,row:-1,index:7}];
for(const side of [0,1])for(let row=0;row<3;row++)assert.equal(context.rowAction(side,row)?.index,row===0?undefined:7);
assert.equal(context.weatherRows({...META.cards[id('Skellige Storm')],weather:3}),6);
context.pendingPlacement={id:id('Olaf'),step:5};context.state.pending=0;
context.state.legal=[0,1].map(row=>({kind:2,card:id('Olaf'),row,index:row}));
for(const side of [0,1])for(let row=0;row<3;row++)assert.equal(context.rowAction(side,row)?.index,side===0&&row<2?row:undefined);
context.state.steps++;assert.equal(context.rowAction(0,0),null);context.pendingPlacement=null;context.state.pending=-1;
context.selected='leader';context.state.legal=[{kind:1,target:-1,index:9}];
for(const [leader,side,row] of [[0,0,2],[10,0,0],[16,0,1],[2,1,1],[7,1,2],[17,1,0]]){
 context.state.players[0].leader=leader;
 for(let s=0;s<2;s++)for(let r=0;r<3;r++)assert.equal(context.rowAction(s,r)?.index,s===side&&r===row?9:undefined);
}
context.busy=true;assert.equal(context.rowAction(1,0),null);context.busy=false;
context.state.winner=0;assert.equal(context.rowAction(1,0),null);context.state.winner=-1;
let opened=false;Object.assign(context,{highlightDestinations(){},renderInspector(){opened=true},run(){throw Error('Weather selection must not auto-use a target');}});
vm.runInContext(source.slice(source.indexOf('function activateLeader('),source.indexOf('function renderInspector(')),context);
context.state.players[0].leader=11;context.state.legal=[{kind:1,target:id('Biting Frost')},{kind:1,target:id('Impenetrable Fog')}];context.activateLeader();assert(opened);assert.equal(context.selected,'leaderTargets');
console.log('PASS: duplicate-safe effect diffs; Scorch vs Decoy; leader Scorch; round cleanup; Clear Weather; Storm mask; agile revival; all six row-target leaders; stale/busy/finished guards; multi-weather leader gallery');


// A destructive unit must be deployed before its victims start burning.
{
 const old=state(),next=state();old.players[0].hand=[id('Villentretenmerth')];old.players[0].handCount=1;
 old.players[1].board=[unit('Black Infantry Archer',0,0,1)];
 next.players[0].hand=[];next.players[0].handCount=0;next.players[0].board=[unit('Villentretenmerth',0,0)];next.players[1].board=[];
 context.structuredClone=structuredClone;
 const staged=context.effects.stage(old,next,META.cards[id('Villentretenmerth')]);
 assert(staged);assert.equal(staged.players[0].board[0].id,id('Villentretenmerth'));assert.equal(staged.players[1].board.length,1);assert.equal(staged.players[0].handCount,0);
 assert.equal(old.players[0].board.length,0);assert.equal(old.players[0].hand.length,1);
 assert.equal(context.effects.plan(staged,next,META.cards[id('Villentretenmerth')]).boards[0].added.length,0);
 assert.equal(context.effects.stage(old,next,META.cards[id('Decoy')]),null);
 console.log('PASS: Villentretenmerth is present beside intact victims before burning; final transition does not deploy him again; game state is not mutated');
}

(async()=>{
 const log=[];let finishBurn,finishSettle;
 const burnFinished=new Promise(r=>finishBurn=r),settled=new Promise(r=>finishSettle=r);
 const old=state(),next=state();old.players[0].board=[a];next.players[0].board=[];
 const ctx={state:old,selected:null,selectedHand:-1,selectedArt:null,pendingPlacement:null,prepareArtwork:async()=>{},playedCard:()=>META.cards[id('Scorch')],render(){log.push('render')},GwentEffects:{capture:()=>({}),stage:()=>null,before:async()=>{log.push('burn:start');await burnFinished;log.push('burn:end')},transition:async()=>{log.push('settle:start');await settled;log.push('settle:end')}}};
 vm.createContext(ctx);vm.runInContext(source.slice(source.indexOf('async function present('),source.indexOf('function showResult(')),ctx);
 const presentation=ctx.present(old,next);await new Promise(setImmediate);
 assert.strictEqual(ctx.state,old);assert.deepEqual(log,['burn:start']);
 finishBurn();await new Promise(setImmediate);assert.strictEqual(ctx.state,next);assert.deepEqual(log,['burn:start','burn:end','render','settle:start']);
 let finished=false;presentation.then(()=>finished=true);await new Promise(setImmediate);assert(!finished);
 finishSettle();await presentation;assert.equal(log.at(-1),'settle:end');
 console.log('PASS: Scorch keeps the old board until burning ends, and the next presentation waits for the new layout to settle');
})().catch(e=>{console.error(e);process.exitCode=1});
