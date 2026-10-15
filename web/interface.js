// Visual sorting only: the deck's draw order and real hand indices stay intact.
function displayGroup(c){if(c.name.startsWith("Gaunter O'Dimm"))return 9000;if(c.name.startsWith('Arachas'))return 9001;return c.flags&(8|128)&&c.group>=0?c.group+1000:c.id;}
function sortKey(c){let category=c.flags&2?0:c.name==='Decoy'?1:c.kind?2:c.flags&1?4:3;let group=displayGroup(c);let members=META.cards.filter(x=>displayGroup(x)===group&&!(x.flags&1));let power=category===3&&members.length?Math.min(...members.map(x=>x.power)):c.power;return [category,power,group,c.power,c.id];}
function sortedCards(ids){return ids.map((id,i)=>({id,i})).sort((a,b)=>{let x=sortKey(card(a.id)),y=sortKey(card(b.id));for(let j=0;j<x.length;j++)if(x[j]!==y[j])return x[j]-y[j];return a.i-b.i;});}
let choiceIndex=0,choiceStamp='',choiceItems=[],choiceRows=false,swapOrder=[],choiceCenterKey='';
let choiceNodes=[],choiceCenters=[],choiceViewport=0;
function measureChoices(){const strip=$('#choiceCards');choiceViewport=strip.clientWidth;choiceCenters=choiceNodes.map(el=>el.offsetLeft+el.offsetWidth/2);}
function paintChoice(el,active){if(!el)return;el.classList.toggle('active',active);el.setAttribute('aria-pressed',String(active));}
function updateChoiceArrows(){$('#choicePrev').disabled=choiceIndex<=0;$('#choiceNext').disabled=choiceIndex>=choiceItems.length-1;}
function keepSwapSlot(index){swapOrder=choiceItems.map(x=>x.i===index?state.players[0].hand.length-1:x.i>index?x.i-1:x.i);}
function targetChoices(mode){
 const moves=state.legal.filter(a=>mode!=='leader'||a.kind===1&&a.target>=0),groups=new Map();
 for(const a of moves){const id=mode==='leader'?a.target:a.card;if(!groups.has(id))groups.set(id,[]);groups.get(id).push(a);}
 const owner=mode==='leader'&&state.players[0].leader===3?1:0,p=state.players[owner];
 const zone=mode==='leader'?([3,13].includes(state.players[0].leader)?'grave':null):state.pendingKind===0?'grave':state.pendingKind===1?'hand':null;
 let items;
 if(zone&&Array.isArray(p[zone]))items=p[zone].flatMap((id,i)=>{const options=groups.get(id);if(!options)return [];return [{id,i,skin:p[zone+'Skins']?.[i]??options[0].skin??p.faction,variant:p[zone+'Variants']?.[i]??options[0].variant??0,moves:options}];});
 else items=[...groups].map(([id,options])=>({id,skin:options[0].skin??p.faction,variant:options[0].variant??0,moves:options}));
 return items.sort((a,b)=>{const x=sortKey(card(a.id)),y=sortKey(card(b.id));for(let j=0;j<x.length;j++)if(x[j]!==y[j])return x[j]-y[j];return (a.i??0)-(b.i??0);});
}
function renderChooser(center=true){if(!state)return;const mode=!state.ready?'swap':state.pending===0&&!pendingPlacement?'revive':selected==='leaderTargets'&&state.legal.some(a=>a.kind===1&&a.target>=0)?'leader':null;if(!mode){$('#chooser').close();choiceStamp='';choiceCenterKey='';return;}
 const stamp=mode+':'+state.steps+':'+state.swaps; if(stamp!==choiceStamp){if(mode!=='swap'||!choiceStamp.startsWith('swap:')){choiceIndex=0;if(mode==='swap')swapOrder=sortedCards(state.players[0].hand).map(x=>x.i);}choiceRows=false;choiceStamp=stamp;}
 if(mode==='swap')choiceItems=swapOrder.map(i=>({id:state.players[0].hand[i],i})).map(x=>({...x,skin:state.players[0].handSkins?.[x.i]??state.players[0].faction,variant:state.players[0].handVariants?.[x.i]??0,moves:[]}));
 else choiceItems=targetChoices(mode);
 $('#chooser').dataset.mode=mode;

 choiceIndex=Math.min(choiceIndex,choiceItems.length-1);$('#choiceTitle').textContent=mode==='swap'?`Замена карт · ${2-state.swaps}`:mode==='leader'?'Способность лидера':state.pendingKind===1?'Выберите карту для сброса':state.pendingKind===2?'Выберите карту':'Воскрешение';
 updateHTML('#choiceCards',choiceItems.map((x,i)=>`<button class="choice-card" ${mode==='swap'?'tabindex="-1"':''} data-choice="${i}" aria-label="${esc(card(x.id).ru)}"><img src="${art(x.id,x.skin,x.variant??0)}" alt="${esc(card(x.id).ru)}"></button>`).join(''));
 // Keep card elements alive during native momentum scrolling. Replacing the
 // strip when only selection changes cancels WebKit's gesture and snap target.
 choiceNodes=[...$('#choiceCards').querySelectorAll('[data-choice]')];
 for(const el of choiceNodes)paintChoice(el,+el.dataset.choice===choiceIndex);
 const item=choiceItems[choiceIndex];let buttons='';if(mode==='swap')buttons=`<button id="choiceSwap" ${busy||state.swaps>=2?'disabled':''}>Заменить</button><button id="choiceReady" class="gold" ${busy?'disabled':''}>Начать раунд →</button>`;
 updateHTML('#choiceActions',buttons);updateChoiceArrows();if(!$('#chooser').open)$('#chooser').showModal();const centerKey=stamp+':'+choiceIndex,shouldCenter=center&&centerKey!==choiceCenterKey,initial=!choiceCenterKey;if(shouldCenter)choiceCenterKey=centerKey;
 if(shouldCenter||choiceCenters.length!==choiceNodes.length)requestAnimationFrame(()=>{if(!$('#chooser').open)return;measureChoices();if(shouldCenter)choiceNodes[choiceIndex]?.scrollIntoView({block:'nearest',inline:'center',behavior:initial?'instant':'smooth'});});
}
let lastChoiceAction=0;
function chooseCurrent(){if(busy||state.ready&&Date.now()-lastChoiceAction<350)return;lastChoiceAction=Date.now();let item=choiceItems[choiceIndex];if(!item)return;if(!state.ready){if(state.swaps<2)run('swap '+item.i);}else if(item.moves.length===1)run('act '+item.moves[0].index);else if(item.moves.every(a=>a.kind===2&&a.row>=0)){pendingPlacement={id:item.id,step:state.steps};selected=item.id;selectedHand=-1;selectedArt=art(item.id,item.skin,item.variant??0);$('#chooser').close();renderInspector();}else{toast('Выберите доступную карту.');}}
function moveChoice(n){choiceIndex=Math.max(0,Math.min(choiceItems.length-1,choiceIndex+n));choiceRows=false;renderChooser();}
$('#choicePrev').onclick=()=>moveChoice(-1);$('#choiceNext').onclick=()=>moveChoice(1);$('#closeChooser').onclick=()=>{selected=null;$('#chooser').close();};
document.addEventListener('click',e=>{
 if(!state?.ready&&$('#chooser').open&&e.target.closest('#chooser')){
  if(e.target.closest('#closeChooser,#choicePrev,#choiceNext'))return;
  e.preventDefault();if(e.target.closest('#choiceReady'))run('ready');else{syncChoiceToScroll();chooseCurrent();}return;
 }
 const c=e.target.closest('[data-choice]');if(c){const i=+c.dataset.choice;if(i===choiceIndex)chooseCurrent();else{choiceIndex=i;choiceRows=false;renderChooser();}return;}if(e.target.closest('#choiceSwap,#choiceAccept'))chooseCurrent();if(e.target.closest('#choiceReady'))run('ready');const f=e.target.closest('[data-faction-choice]');if(f){$('#faction').value=f.dataset.factionChoice;setupFaction();renderFactionTabs();}});
document.addEventListener('keydown',e=>{if(!$('#chooser').open)return;if(e.key==='ArrowLeft'){e.preventDefault();moveChoice(-1);}if(e.key==='ArrowRight'){e.preventDefault();moveChoice(1);}});
const factionShields=['realms','nilfgaard','monsters','scoiatael','skellige'];
function factionEmblem(f){return `<img class="faction-emblem" src="assets/screen/deck_shield_${factionShields[f]}.png" alt="">`;}
function renderFactionTabs(){if(!META)return;$('#factionTabs').innerHTML=META.factions.map(f=>`<button data-faction-choice="${f.id}" class="${+$('#faction').value===f.id?'active':''}">${factionEmblem(f.id)}<span>${esc(f.name)}</span></button>`).join('');}

function randomSeed(){const words=crypto.getRandomValues(new Uint32Array(2));return ((BigInt(words[0])<<32n)|BigInt(words[1])).toString();}
function saveDrafts(){try{localStorage.setItem('gwent-decks-v1',JSON.stringify(drafts));}catch{}}
let hadSavedDrafts=false;
function loadDrafts(){let saved={};try{hadSavedDrafts=!!localStorage.getItem('gwent-decks-v1');saved=JSON.parse(localStorage.getItem('gwent-decks-v1')||'{}');}catch{}for(let f=0;f<5;f++){let d=saved?.[f],counts={};drafts[f]=Array.isArray(d)&&d.length<=200&&d.every(id=>{let c=META.cards[id];return c&&!c.token&&(c.faction<0||c.faction===f)&&((counts[id]=(counts[id]||0)+1)<=c.copies);})?d:[...PRESETS[f]];}}
function renderSetupControls(){for(const name of ['leader','opponent','initiative']){let select=$('#'+name);if(!select)continue;let host=$('#control-'+name);if(!host){host=document.createElement('div');host.id='control-'+name;host.className='choice-control';select.after(host);select.classList.add('native-hidden');}host.innerHTML=`<button data-cycle="${name}" data-direction="-1" aria-label="Предыдущий вариант">‹</button><span>${name==='opponent'?factionEmblem(+select.value):''}${esc(select.selectedOptions[0]?.textContent||'')}</span><button data-cycle="${name}" data-direction="1" aria-label="Следующий вариант">›</button>`;}}
document.addEventListener('click',e=>{const b=e.target.closest('[data-cycle]');if(!b)return;const select=$('#'+b.dataset.cycle);select.selectedIndex=(select.selectedIndex+Number(b.dataset.direction)+select.options.length)%select.options.length;select.dispatchEvent(new Event('change'));renderSetupControls();});

// Trackpads keep their native horizontal gesture; a mouse wheel scrolls the gallery too.
$('#hand').addEventListener('wheel',event=>{
 const hand=$('#hand');if(hand.scrollWidth<=hand.clientWidth||Math.abs(event.deltaX)>=Math.abs(event.deltaY))return;
 const before=hand.scrollLeft;hand.scrollLeft+=event.deltaY*(event.deltaMode===1?18:event.deltaMode===2?hand.clientWidth:1);
 if(hand.scrollLeft!==before)event.preventDefault();
},{passive:false});
// No cancelable document-level wheel listener: native horizontal gestures stay
// on the compositor. The modal locks page scrolling, so vertical wheel mapping
// needs no preventDefault either.
$('#choiceCards').addEventListener('wheel',event=>{const strip=$('#choiceCards');if(event.deltaX||!event.deltaY)return;strip.scrollLeft+=event.deltaY*(event.deltaMode===1?18:event.deltaMode===2?strip.clientWidth:1);},{passive:true});

// Selection follows each rendered scroll frame, without recentering or
// rebuilding the strip. Native trackpad momentum stays in charge.
let choiceScrollFrame=0;
function selectChoice(index){if(index===choiceIndex)return;paintChoice(choiceNodes[choiceIndex],false);choiceIndex=index;choiceRows=false;choiceCenterKey=choiceStamp+':'+choiceIndex;paintChoice(choiceNodes[choiceIndex],true);updateChoiceArrows();}
function syncChoiceToScroll(){const strip=$('#choiceCards');if(!choiceCenters.length)return;const center=strip.scrollLeft+choiceViewport/2;let nearest=choiceIndex,distance=Infinity;for(let i=0;i<choiceCenters.length;i++){const d=Math.abs(choiceCenters[i]-center);if(d<distance){distance=d;nearest=i;}}selectChoice(nearest);}
// A mouse click must not focus a side card and make the browser scroll it.
document.addEventListener('pointerdown',event=>{if(!state?.ready&&event.pointerType!=='touch'&&event.target.closest('#chooser')&&!event.target.closest('#closeChooser,#choicePrev,#choiceNext,#choiceReady,#choiceSwap'))event.preventDefault();});
document.addEventListener('pointerover',event=>{const el=event.target.closest('[data-choice]');if(el&&state?.ready&&event.pointerType!=='touch')selectChoice(+el.dataset.choice);});
document.addEventListener('scroll',event=>{
 if(event.target.id!=='choiceCards'||choiceScrollFrame)return;
 choiceScrollFrame=requestAnimationFrame(()=>{choiceScrollFrame=0;const strip=$('#choiceCards');if(!$('#chooser').open)return;
  syncChoiceToScroll();
 });
},true);
if(typeof ResizeObserver!=='undefined')new ResizeObserver(()=>{if($('#chooser').open)measureChoices();}).observe($('#choiceCards'));
