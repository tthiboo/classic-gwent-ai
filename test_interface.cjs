const fs=require('fs'),vm=require('vm'),assert=require('assert');
const META=JSON.parse(fs.readFileSync('game/web/assets/catalog.json','utf8'));
const listeners={};let html={},node={dataset:{},open:true,close(){},showModal(){},scrollIntoView(){},addEventListener(){},querySelectorAll(){return []},classList:{toggle(){},remove(){}}};
const context={META,card:id=>META.cards[id],$:()=>node,document:{addEventListener(type,fn){(listeners[type]??=[]).push(fn);}},pendingPlacement:null,selected:null,selectedHand:-1,selectedArt:null,renderInspector(){},toast(){},updateHTML:(s,v)=>html[s]=v,esc:x=>x,art:(id)=>String(id),ROWS:['melee','ranged','siege'],requestAnimationFrame:()=>{},busy:false,run:()=>{}};
vm.createContext(context);vm.runInContext(fs.readFileSync('game/web/interface.js','utf8'),context);
const id=n=>META.cards.find(c=>c.name===n).id;
context.ids=['Geralt of Rivia','Decoy','Thaler','Mysterious Elf (Avallach)','Scorch','Catapult','Dandelion'].map(id);
const sorted=vm.runInContext('sortedCards(ids).map(x=>card(x.id).name)',context);
assert.deepEqual(Array.from(sorted),['Mysterious Elf (Avallach)','Thaler','Decoy','Scorch','Dandelion','Catapult','Geralt of Rivia']);
context.state={ready:true,pending:0,pendingKind:0,steps:3,swaps:2,players:[{faction:4}],legal:[{kind:2,card:id('Olaf'),index:0,row:0},{kind:2,card:id('Olaf'),index:1,row:1},{kind:2,card:id('War Longship'),index:2,row:2}]};
vm.runInContext('renderChooser()',context);assert.equal((html['#choiceCards'].match(/class="choice-card/g)||[]).length,2);
vm.runInContext('choiceIndex=1; chooseCurrent()',context);assert.equal(context.pendingPlacement.id,id('Olaf'));assert.equal(context.selected,id('Olaf'));assert(!html['#choiceActions'].includes('data-action'));context.pendingPlacement=null;
console.log('PASS: hand category/power sorting, unique agile revival card, field-based row selection without action buttons');

context.selected='leaderTargets';context.state.pending=-1;context.state.legal=[{kind:1,card:-1,target:id('Thaler'),skin:0,index:5,row:-1}];vm.runInContext('renderChooser()',context);assert(html['#choiceCards'].includes('Талер'));console.log('PASS: Emhyr target gallery visible through the fullscreen chooser');

context.state={ready:false,pending:-1,steps:0,swaps:0,players:[{faction:0,hand:[id('Catapult'),id('Thaler'),id('Decoy')]}],legal:[]};
vm.runInContext("choiceStamp='';renderChooser();choiceIndex=1;keepSwapSlot(choiceItems[1].i)",context);
context.state.players[0].hand=[id('Catapult'),id('Thaler'),id('Dandelion')];context.state.swaps=1;
vm.runInContext('renderChooser()',context);
assert.equal(vm.runInContext('choiceIndex',context),1);
assert.equal(vm.runInContext('choiceItems[choiceIndex].id',context),id('Dandelion'));
console.log('PASS: replacement remains selected in the same visual slot');

let centers=0;context.requestAnimationFrame=()=>centers++;
vm.runInContext("choiceCenterKey='';renderChooser();renderChooser();renderChooser()",context);
assert.equal(centers,1);
vm.runInContext('moveChoice(1)',context);assert.equal(centers,2);
console.log('PASS: unchanged renders do not interrupt carousel scrolling');

// Selection must not change strip markup: the real updateHTML retains its nodes.
const stripBefore=html['#choiceCards'];
vm.runInContext('choiceIndex=0;renderChooser()',context);
assert.equal(html['#choiceCards'],stripBefore);
console.log('PASS: selection changes preserve carousel DOM and native momentum');

// Each real grave copy is shown separately; alternate rows are not extra cards.
context.state={ready:true,pending:0,pendingKind:0,steps:8,swaps:2,players:[{faction:4,grave:[id('Olaf'),id('Olaf'),id('War Longship'),id('Geralt of Rivia')],graveSkins:[4,4,4,4],graveVariants:[0,1,0,0]}],legal:[{kind:2,card:id('Olaf'),index:0,row:0},{kind:2,card:id('Olaf'),index:1,row:1},{kind:2,card:id('War Longship'),index:2,row:2}]};
context.pendingPlacement=null;vm.runInContext('renderChooser()',context);
const actualCopies=vm.runInContext('choiceItems.filter(x=>x.id===cardId)',Object.assign(context,{cardId:id('Olaf')}));
assert.equal(actualCopies.length,2);assert.deepEqual(Array.from(actualCopies,x=>x.variant),[0,1]);assert(actualCopies.every(x=>x.moves.length===2));
assert.equal((html['#choiceCards'].match(/class="choice-card/g)||[]).length,3);assert(!html['#choiceCards'].includes('choice-copies'));
console.log('PASS: every eligible grave copy appears separately, with its own artwork and agile row choices');

context.state={ready:false,pending:-1,steps:0,swaps:0,players:[{faction:0,hand:[id('Catapult'),id('Thaler'),id('Decoy')]}],legal:[]};
vm.runInContext("choiceStamp='';renderChooser();choiceIndex=0",context);
let commands=[];context.run=command=>commands.push(command);
const hit={dataset:{choice:'2'}};const pointerEvent={pointerType:'mouse',preventDefault(){},target:{closest:s=>s==='[data-choice]'?hit:s==='#chooser'?node:null}};
for(const fn of listeners.pointerover||[])fn(pointerEvent);
assert.equal(vm.runInContext('choiceIndex',context),0);
vm.runInContext('lastChoiceAction=0',context);
for(const fn of listeners.click||[])fn(pointerEvent);
assert.equal(vm.runInContext('choiceIndex',context),0);assert.deepEqual(commands,['swap '+vm.runInContext('choiceItems[0].i',context)]);assert.equal(node.dataset.mode,'swap');
// The scroll path only updates selection, without rebuilding HTML/artwork or measuring cards.
const updateHTML=context.updateHTML;context.updateHTML=()=>{throw Error('gallery rebuilt during scroll');};
vm.runInContext('selectChoice(2)',context);assert.equal(vm.runInContext('choiceIndex',context),2);context.updateHTML=updateHTML;
vm.runInContext('lastChoiceAction=0',context);
const emptyClick={preventDefault(){},target:{closest:s=>s==='#chooser'?node:null}};
for(const fn of listeners.click||[])fn(emptyClick);
assert.equal(commands.at(-1),'swap '+vm.runInContext('choiceItems[2].i',context));
context.busy=true;vm.runInContext('lastChoiceAction=0',context);const count=commands.length;
for(const fn of listeners.click||[])fn(emptyClick);assert.equal(commands.length,count);
context.busy=false;
const readyClick={preventDefault(){},target:{closest:s=>s==='#chooser'||s==='#choiceReady'?node:null}};
for(const fn of listeners.click||[])fn(readyClick);assert.equal(commands.at(-1),'ready');
console.log('PASS: swap ignores hover; card/background clicks confirm the scroll-selected card; busy and Ready guards preserved; scrolling never rebuilds the gallery');
