const $=s=>document.querySelector(s);let META,PRESETS,state=null,selected=null,busy=false,resultShown=false,drafts={},editing=false;let selectedArt=null,selectedHand=-1,pendingPlacement=null;
const ROWS=['Рукопашный','Дальнобойный','Осадный'],TOP=[33.0,16.7,.4,50.9,67.2,83.5];
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const card=id=>META.cards[id],fname=f=>META.factions[f].name,lmeta=l=>META.leaders.find(x=>x.code===l);
function art(id,f,variant=0){let group=META.assets[id]||{},list=group[f]||group[card(id).faction]||Object.values(group)[0]||[];const src=list[variant%list.length];return src?src+'?v=melee2':'';}
// Preserve existing image nodes when only selection or controls change.
function updateHTML(selector,html){const el=$(selector);if(el._html===html)return;
 const template=document.createElement('template');template.innerHTML=html;
 function patch(parent,fresh){
  const wanted=[...fresh.childNodes],keys=new Map([...parent.children].filter(n=>n.dataset.renderKey).map(n=>[n.dataset.renderKey,n]));
  for(let i=0;i<wanted.length;i++){
   const next=wanted[i],key=next.nodeType===1?next.dataset.renderKey:null;let old=key?keys.get(key):parent.childNodes[i];
   if(!key&&old?.nodeType===1&&old.dataset.renderKey)old=null;
   if(!old||old.nodeType!==next.nodeType||old.nodeName!==next.nodeName){parent.insertBefore(next.cloneNode(true),parent.childNodes[i]||null);continue;}
   if(old!==parent.childNodes[i])parent.insertBefore(old,parent.childNodes[i]||null);
   if(old.nodeType===3){if(old.nodeValue!==next.nodeValue)old.nodeValue=next.nodeValue;continue;}
   if(old.nodeType!==1)continue;
   if(old.hasAttribute('data-portrait'))continue;
   for(const attr of [...old.attributes])if(!next.hasAttribute(attr.name))old.removeAttribute(attr.name);
   for(const name of ['magnified-source','portrait-ready','incinerating'])if(old.classList.contains(name))next.classList.add(name);
   for(const attr of next.attributes)if(old.getAttribute(attr.name)!==attr.value)old.setAttribute(attr.name,attr.value);
   patch(old,next);
  }
  while(parent.childNodes.length>wanted.length)parent.lastChild.remove();
 }
 patch(el,template.content);el._html=html;
}
function selectCard(id,src,hand=-1){closeBoardPreview();if(hand>=0||id===null||pendingPlacement&&id!==pendingPlacement.id)pendingPlacement=null;selected=id;selectedArt=src;selectedHand=hand;document.querySelectorAll('#hand .card').forEach(el=>el.classList.toggle('selected',+el.dataset.hand===hand));renderInspector();}

function picture(id,f,opts={}){let c=card(id),cls=(opts.small?'small ':'')+(changedCards.has(opts.side+':'+opts.board)?'arriving':'');let changed=opts.power!==undefined&&opts.power!==c.power;return `<button class="card ${cls} " ${opts.key?`data-render-key="${opts.key}"`:""} ${opts.hand!==undefined?`data-hand="${opts.hand}" draggable="true"`:''} ${opts.board!==undefined?`data-board="${opts.board}" data-side="${opts.side}"`:''} data-card="${id}" data-art="${art(id,f,opts.variant||0)}" aria-label="${esc(c.ru)}${c.kind?'':', сила '+(opts.power??c.power)}"><img src="${(opts.board!==undefined||opts.hand!==undefined?art(id,f,opts.variant||0):art(id,f,opts.variant||0).replace("/cards/","/thumbs/"))}" alt="${esc(c.ru)}" loading="eager" decoding="sync" draggable="false">${opts.small&&opts.board!==undefined?'<canvas class="board-portrait" data-portrait aria-hidden="true"></canvas>':''}${changed?`<span class="current-power ${opts.power>c.power?'buffed':'weakened'}" style="background-image:url(assets/power/${opts.power>c.power?'up':'down'}-${opts.power}.webp)" aria-label="Сила ${opts.power}"></span>`:''}</button>`;}
function zonePictures(items,faction,zone,side){const copies=new Map();return items.map(u=>{const skin=u.skin??faction,variant=u.variant??0,base=[zone,side,u.row??-1,u.id,skin,variant].join(':'),n=copies.get(base)||0;copies.set(base,n+1);return picture(u.id,skin,{...u,side,key:base+':'+n,variant});}).join('');}
function ability(c){let a=[];if(c.flags&1)a.push('Герой');if(c.flags&2)a.push('Шпион');if(c.flags&4)a.push('Лекарь');if(c.flags&8)a.push('Прочная связь');if(c.flags&16)a.push('Прилив сил');if(c.flags&32)a.push('Казнь рукопашных');if(c.flags&64)a.push('Командирский рог');if(c.flags&128)a.push('Двойник');if(c.flags&256)a.push('Призыв беса');if(c.flags&512)a.push('Берсерк');if(c.flags&1024)a.push('Мардрём');if(c.flags&2048)a.push('Призыв Хеймдалля');if(c.flags&4096)a.push('Казнь дальнобойных');if(c.flags&8192)a.push('Казнь осадных');if(c.flags&16384)a.push('Казнь');return a.join(' · ')||(c.kind?'Спецкарта':'Обычный боец');}
function desc(c){let a=[];if(c.flags&1)a.push('Не подвержен погоде, Казни и усилениям. Нельзя воскресить или вернуть Чучелом.');if(c.flags&2)a.push('Выставляется противнику; вы добираете до двух карт.');if(c.flags&4)a.push('Воскрешает обычного бойца из своего сброса. Его способность срабатывает снова.');if(c.flags&8)a.push('Сила умножается на количество одноимённых бойцов в этом ряду.');if(c.flags&16)a.push('Другим обычным бойцам ряда +1 силы.');if(c.flags&64)a.push('Удваивает силу остальных обычных бойцов этого ряда.');if(c.flags&128)a.push('Призывает подходящие карты группы из руки и колоды.');if(c.flags&256)a.push('При уходе в сброс призывает беса силой 8.');if(c.flags&512)a.push('Под действием Мардрёма в этом ряду превращается в вильдкаарла.');if(c.flags&1024)a.push('Превращает берсерков в своём ряду.');if(c.flags&2048)a.push('При уходе в сброс призывает Хеймдалля.');if(c.flags&(32|4096|8192))a.push('Если сила соответствующего ряда противника не меньше 10, уничтожает его сильнейших обычных бойцов этого ряда.');if(c.flags&16384||c.name==='Scorch')a.push('Уничтожает всех сильнейших обычных бойцов на обеих сторонах по их текущей силе.');if(c.weather)a.push('Снижает базовую силу обычных бойцов соответствующих рядов на обеих сторонах до 1.');if(c.name==='Clear Weather')a.push('Снимает всю погоду с обеих сторон.');if(c.name==="Commander's Horn")a.push('Удваивает силу обычных бойцов выбранного ряда. Несколько рогов не складываются.');if(c.name==='Decoy')a.push('Возвращает своего обычного бойца в руку, занимая его место. Герои недоступны.');if(c.name==='Mardroeme')a.push('Превращает берсерков выбранного ряда в вильдкаарлов. Превращение сохраняется после ухода в сброс.');return a.join(' ')||'Обычный боец. На него действуют погода и усиления.';}
function translate(s){for(let c of META.cards.slice().sort((a,b)=>b.name.length-a.name.length))s=s.replaceAll(c.name,c.ru);return s;}
async function api(command){const r=await fetch('/api',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command})});if(!r.ok)throw Error('Нет связи с локальным сервером.');const s=await r.json();if(!s.ok)throw Error(s.error);return s;}
function toast(s){$('#toast').textContent=s;$('#toast').hidden=false;setTimeout(()=>$('#toast').hidden=true,7000);}
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
let roundContinue=null,changedCards=new Set(),presenting=false;
function playedCard(previous,next){
 let overlap=Math.min(previous?.log.length||0,next.log.length);while(overlap&&previous.log.slice(-overlap).join('\n')!==next.log.slice(0,overlap).join('\n'))overlap--;const lines=next.log.slice(overlap);
 for(const line of lines){if(!line.startsWith('Вы:')&&!line.startsWith('Соперник:'))continue;if(line.includes('Чучело →'))return META.cards.find(c=>c.name==='Decoy');
  const c=META.cards.slice().sort((a,b)=>b.name.length-a.name.length).find(c=>line.includes(c.name));if(c)return c;}
 return null;
}
async function present(previous,next){await prepareArtwork(next);
 const c=previous?playedCard(previous,next):null;let visualPrevious=previous,snapshot=GwentEffects.capture(previous,next,c);
 const staged=previous&&GwentEffects.stage(previous,next,c);
 if(staged){state=staged;selected=null;selectedHand=-1;pendingPlacement=null;render();await GwentEffects.transition(previous,staged,null,snapshot);visualPrevious=staged;snapshot=GwentEffects.capture(staged,next,c);}
 if(previous)await GwentEffects.before(visualPrevious,next,c,snapshot);
 const roundEnded=previous&&(next.round>previous.round||(next.winner>=0&&previous.winner<0));
 if(roundEnded){
  state=structuredClone(previous);for(let i=0;i<2;i++){state.players[i].passed=true;state.players[i].lives=next.players[i].lives;state.players[i].wins=next.players[i].wins;}selected=null;selectedHand=-1;pendingPlacement=null;render();
  const losses=next.players.map((p,i)=>previous.players[i].lives-p.lives),outcome=losses[0]===losses[1]?'draw':losses[0]>losses[1]?'defeat':'victory';
  const roundEffect=GwentEffects.round(snapshot,losses,outcome);
  $('#roundPause').dataset.outcome=outcome;
  $('#roundSummary').innerHTML=`<span>${outcome==='draw'?'Ничья в раунде':outcome==='victory'?'Раунд за вами':'Раунд за соперником'}</span><small>${previous.players[0].score} · ${previous.players[1].score}</small>`;
  $('#turnStatus').textContent='Раунд завершён';$('#turnStatus').classList.remove('thinking');
  if(next.winner<0){$('#roundPause').hidden=false;$('#continueRound').disabled=true;await roundEffect;$('#continueRound').disabled=false;await new Promise(resolve=>{roundContinue=resolve;});roundContinue=null;$('#roundPause').hidden=true;}
  else {await roundEffect;state=next;render();await GwentEffects.transition(previous,next,c,snapshot);showResult();return;}
 }
 state=next;selected=null;selectedHand=-1;selectedArt=null;pendingPlacement=null;render();
 if(previous)await GwentEffects.transition(visualPrevious,next,c,snapshot);
 if(!previous||!next.ready&&previous.ready)GwentEffects.deal();
 if(previous&&next.players[0].leader===8&&!previous.players[0].used&&next.players[0].used&&next.players[1].known.length)showCards('Карты соперника',next.players[1].known,next.players[1].faction);
}
function showResult(){if(!state||state.winner<0||resultShown)return;resultShown=true;const outcome=state.winner===2?'draw':state.winner===0?'victory':'defeat';$('#result').dataset.outcome=outcome;$('#resultTitle').textContent=outcome==='draw'?'Ничья':outcome==='victory'?'Победа':'Поражение';$('#resultText').innerHTML=`<span>${esc(fname(state.players[0].faction))}</span><b>${state.players[0].score} : ${state.players[1].score}</b><span>${esc(fname(state.players[1].faction))}</span>`;$('#result').showModal();GwentEffects.result($('#result'));}
// Decode only artwork entering the live match, before its first visible frame.
const readyArtwork=new Map();
async function prepareArtwork(next){
 const urls=new Set();for(const p of next.players){for(const u of p.board){urls.add(art(u.id,u.skin??p.faction,u.variant??0));if(u.power!==card(u.id).power)urls.add(`assets/power/${u.power>card(u.id).power?'up':'down'}-${u.power}.webp`);}for(let i=0;i<p.hand.length;i++)urls.add(art(p.hand[i],p.handSkins?.[i]??p.faction,p.handVariants?.[i]??0));}
 await Promise.all([...urls].map(src=>{if(!readyArtwork.has(src)){const img=new Image();img.src=src;readyArtwork.set(src,img.decode().catch(()=>{readyArtwork.delete(src);}));}return readyArtwork.get(src);}));
}
async function run(cmd){if(busy)return;if(cmd.startsWith('act ')&&selectedHand>=0){const action=state.legal.find(a=>a.index===+cmd.slice(4));if(action?.kind===0&&state.players[0].hand[selectedHand]===action.card)cmd+=' '+selectedHand;}busy=true;render();try{let previous=state;let next=await api(cmd);if(cmd.startsWith('swap '))keepSwapSlot(+cmd.slice(5));await present(previous,next);if(cmd.startsWith('swap ')&&state.swaps===2){previous=state;next=await api('ready');await present(previous,next);}while(state.ready&&state.winner<0&&state.turn===1){await pause(800);previous=state;next=await api('bot');await present(previous,next);}}catch(e){toast(e.message);}finally{busy=false;render();}}

function panel(p,s){return `<span class="eyebrow">${s===0?'ВЫ':'СОПЕРНИК'}</span><h2>${esc(fname(p.faction))}</h2><div class="score"><strong>${p.score}</strong><span>ОБЩАЯ СИЛА</span></div><div class="rubies" aria-label="Рубинов: ${p.lives}">${[0,1].map(i=>`<i data-ruby="${s}:${i}" class="${i<p.lives?'alive':''}"><img src="assets/ui/ruby.svg" alt=""></i>`).join('')}</div><div class="player-hand-count" aria-label="Карт в руке: ${p.handCount}"><svg viewBox="0 0 32 28" aria-hidden="true"><path d="M8 22 3 6l12-4 2 5M13 23 10 6l12-2 1 5"/><rect x="17" y="9" width="12" height="17" rx="1.5"/></svg><span>${p.handCount}</span></div><p class="pass-status">${p.passed?'<b>ПАС</b>':''}</p><p class="grave-count">Сброс · ${p.grave.length}</p>`;}
function render(){if(!state)return;const me=state.players[0],enemy=state.players[1];updateHTML('#ownPanel',panel(me,0));updateHTML('#enemyPanel',panel(enemy,1));$('#roundNumber').textContent=['I','II','III'][state.round]||state.round+1;$('#matchLabel').textContent=fname(me.faction)+' × '+fname(enemy.faction);
 $('#turnStatus').textContent=state.winner>=0?(state.winner===2?'Ничья':state.winner===0?'Победа':'Поражение'):!state.ready?`Можно заменить ещё ${2-state.swaps} ${state.swaps===1?'карту':'карты'}`:state.turn===1?'Соперник обдумывает ход':state.pending===0?(state.pendingKind===1?'Сбросьте две карты':state.pendingKind===2?'Выберите карту из колоды':'Выберите бойца для воскрешения'):enemy.passed?'Соперник спасовал':'Ваш ход';
 $('#turnStatus').classList.toggle('thinking',state.turn===1&&state.ready&&state.winner<0);
 let rows='';for(let s=0;s<2;s++)for(let r=0;r<3;r++){let p=state.players[s],units=p.board.filter(u=>u.row===r&&!["Commander's Horn","Mardroeme"].includes(card(u.id).name)),specials=p.board.filter(u=>u.row===r&&["Commander's Horn",'Mardroeme'].includes(card(u.id).name)),top=s===1?TOP[r]:TOP[r+3];rows+=`<div class="battle-row ${state.weather&(1<<r)?'weathered weather-'+r:''}" style="top:${top}%" data-row="${r}" data-side="${s}" aria-label="${s?'Чужой':'Ваш'} ${ROWS[r]} ряд"><span class="row-score">${units.reduce((n,u)=>n+u.power,0)}</span><div class="row-special">${zonePictures(specials.map(u=>({...u,board:u.index})),p.faction,'special',s)}${p.horn&(1<<r)?`<img src="${art(META.cards.find(c=>c.name==="Commander's Horn").id,p.faction)}" alt="Рог лидера">`:''}</div><div class="row-cards">${zonePictures(units.map(u=>({...u,small:true,board:u.index})),p.faction,'board',s)}</div>${state.weather&(1<<r)?'<span class="weather-fx" aria-hidden="true"><i></i><i></i><i></i></span>':''}<span class="weather-icon">${state.weather&(1<<r)?`<img src="assets/weather/icon-${r}.svg" alt="${['Мороз','Мгла','Ливень'][r]}">`:''}</span></div>`;}updateHTML('#battleRows',rows);
 updateHTML('#boardZones',state.players.map((p,s)=>`<button class="board-leader ${s?'enemy-leader':'own-leader'} ${p.used?'spent':''}" data-leader="${s}" title="${esc(lmeta(p.leader).name)}"><img src="${lmeta(p.leader).asset}" alt="${esc(lmeta(p.leader).name)}"></button><button class="deck-stack ${s?'enemy-deck':'own-deck'}" ${s?'aria-label="Колода соперника"':'data-deck'}><img src="${META.backs[p.faction]}" alt="Рубашка ${esc(fname(p.faction))}"><span class="pile-label">КОЛОДА</span><strong>${p.deckCount}</strong></button><button data-grave="${s}" class="board-grave ${s?'enemy-grave':'own-grave'}" aria-label="Сброс: ${p.grave.length} карт">${p.grave.length?`<img src="${art(p.grave.at(-1),p.graveSkins?.at(-1)??p.faction,p.graveVariants?.at(-1)??0)}" alt="Верхняя карта: ${esc(card(p.grave.at(-1)).ru)}">`:""}<span>СБРОС</span><strong>${p.grave.length}</strong></button>`).join(''));
 updateHTML('#weather',state.weatherCards.map((id,i)=>`<button data-inspect="${id}" title="${esc(card(id).ru)}"><img src="${art(id,state.weatherSkins?.[i]??me.faction)}" alt="${esc(card(id).ru)}"></button>`).join(''));
 updateHTML('#hand',zonePictures(sortedCards(me.hand).map(({id,i})=>({id,hand:i,skin:me.handSkins?.[i]??me.faction,variant:me.handVariants?.[i]??0})),me.faction,'hand',0));$('#handCount').textContent=me.handCount;$('.table').classList.toggle('empty-hand',me.handCount===0);$('#hint').textContent=!state.ready?'Выберите карту для замены':state.pending===0?'Выберите бойца, затем ряд на поле':'Выберите карту и подходящий ряд';
 $('#passBtn').textContent=state.ready?(me.passed?'Пас принят':'Пас →'):'Начать раунд →';$('#passBtn').classList.toggle('passed',me.passed);$('#passBtn').disabled=busy||state.winner>=0||(state.ready&&(state.turn!==0||state.pending>=0));
 $('#log').innerHTML=state.log.slice().reverse().map(x=>'<p>'+esc(translate(x))+'</p>').join('');if(state.pending===0&&state.turn===0&&!pendingPlacement)selected='pending';renderInspector();renderChooser();paintBoardPortraits();syncBoardPreview();

}
function label(a){if(a.kind===2)return 'Воскресить: '+card(a.card).ru;if(a.kind===4)return 'Сбросить: '+card(a.card).ru;if(a.kind===5)return 'Взять: '+card(a.card).ru;if(a.kind===1)return a.target>=0?'Выбрать: '+card(a.target).ru:'Применить способность';if(card(a.card).name==='Decoy')return 'Вернуть: '+card(state.players[0].board[a.target].id).ru;return a.row>=0?'Разыграть · '+ROWS[a.row]:'Разыграть карту';}
// The preview lifts the leader out of its board slot and returns it on dismissal.
let liftedLeader=null;
function leaderImageRect(img){const r=img.getBoundingClientRect(),ratio=(img.naturalWidth||1)/(img.naturalHeight||2),w=Math.min(r.width,r.height*ratio),h=w/ratio;return {left:r.left+(r.width-w)/2,top:r.top+(r.height-h)/2,width:w,height:h};}
function leaderFlight(img,from,to){if(!from.width||!to.width||matchMedia('(prefers-reduced-motion: reduce)').matches)return null;return img.animate([{transform:`translate(${from.left-to.left}px,${from.top-to.top}px) scale(${from.width/to.width},${from.height/to.height})`},{transform:'none'}],{duration:550,easing:'cubic-bezier(.2,.75,.25,1)'});}
function prepareLeaderPreview(next){
 if(liftedLeader===next)return false;
 const previousLeader=liftedLeader;liftedLeader=next;
 if(previousLeader!==null){
  const slot=$(`[data-leader="${previousLeader}"]`),img=$('#inspector .inspector-card');
  if(slot&&img){slot.classList.add('lifted');const from=img.getBoundingClientRect(),to=leaderImageRect(slot.querySelector('img')),ghost=img.cloneNode();ghost.className='leader-flight';ghost.setAttribute('aria-hidden','true');Object.assign(ghost.style,{left:to.left+'px',top:to.top+'px',width:to.width+'px',height:to.height+'px'});document.body.append(ghost);const flight=leaderFlight(ghost,from,to);const finish=()=>{ghost.remove();if(liftedLeader!==Number(slot.dataset.leader))slot.classList.remove('lifted');};if(flight)flight.finished.then(finish,finish);else finish();}
 }
 liftedLeader=next;return true;
}
function finishLeaderPreview(side,changed){
 if(side===null)return;const slot=$(`[data-leader="${side}"]`),img=$('#inspector .inspector-card');if(!slot||!img)return;slot.classList.add('lifted');if(changed)leaderFlight(img,leaderImageRect(slot.querySelector('img')),img.getBoundingClientRect());
}
// Capture dismissal before the normal handlers; legal card/row actions retain selection.
document.addEventListener('click',e=>{
 if(!$('.details').classList.contains('open')||e.target.closest('.details,dialog,[data-hand],[data-board],[data-leader],[data-inspect]'))return;
 const row=e.target.closest('[data-row]');if(row&&rowAction(+row.dataset.side,+row.dataset.row))return;
 selectCard(null,null);
},true);
function activateLeader(){
 if(selected!=='leader')return;
 if(busy||!state?.ready||state.turn!==0||state.winner>=0){selectCard(null,null);return;}
 const moves=state.legal.filter(a=>a.kind===1);if(!moves.length){selectCard(null,null);return;}
 // A leader with several weather choices needs the card gallery first.
 if(state.players[0].leader===11&&moves.length>1){selected='leaderTargets';renderInspector();return;}
 const rowCodes=[0,2,5,6,7,10,11,16,17,18];
 if(rowCodes.includes(state.players[0].leader)){if(moves.length===1)run('act '+moves[0].index);else highlightDestinations();return;}
 if(moves.some(a=>a.target>=0)){selected='leaderTargets';renderInspector();return;}
 if(moves.length===1)run('act '+moves[0].index);
}
function renderInspector(){if(!state)return;highlightDestinations();const leaderSide=selected==='leader'?0:selected==='enemyLeader'?1:null;const leaderChanged=prepareLeaderPreview(leaderSide);
 if(state.pending===0&&!pendingPlacement){$('.details').classList.remove('open');renderChooser();return;}
 if(selected==='leaderTargets'&&state.legal.some(a=>a.kind===1&&a.target>=0)){$('.details').classList.remove('open');renderChooser();return;}
 let html='';
 if(leaderSide!==null){const l=lmeta(state.players[leaderSide].leader),canUse=leaderSide===0&&state.legal.some(a=>a.kind===1);html=`<img class="inspector-card ${canUse?'actionable':''}" data-use-leader src="${l.asset}" alt="${esc(l.name)}"><p class="leader-description">${esc(l.description)}</p>${canUse?'<p class="placement-hint">Нажмите карту лидера или выделенный ряд</p>':''}`;}
 else if(typeof selected==='number'){let c=card(selected);html=`<img class="inspector-card ${selectedHand>=0?'actionable':''}" data-use-card src="${selectedArt||art(selected,state.players[0].faction)}" alt="${esc(c.ru)}">`;if(pendingPlacement)html+='<p class="placement-hint">Выберите ряд на поле</p>';else if(selectedHand>=0&&state.ready)html+=`<p class="placement-hint">${c.name==='Scorch'||c.name==='Clear Weather'?'Нажмите карту для розыгрыша':'Выберите выделенный ряд на поле'}</p>`;}
 updateHTML('#inspector',html);$('.details').classList.toggle('open',selected!==null&&selected!=='pending');$('.details').classList.remove('choosing');finishLeaderPreview(leaderSide,leaderChanged);
}

function inspect(id){selectedArt=null;selectedHand=-1;selected=selected===id?null:id;renderInspector();}
function showContent(html){$('#contentBody').innerHTML=html;if(!$('#content').open)$('#content').showModal();}
function showCards(title,ids,f=state?.players[0].faction||0,zone={}){editing=false;const copies={};showContent(`<h2>${esc(title)}</h2>${ids.length?'<div class="collection-grid">'+sortedCards(ids).map(({id,i})=>`<div>${picture(id,zone.skins?.[i]??f,{variant:zone.variants?.[i]??(copies[id]=(copies[id]??-1)+1)})}<p>${esc(card(id).ru)}</p></div>`).join('')+'</div>':'<p>Пока нет карт.</p>'}`);}
function deckSummary(){saveDrafts();let d=drafts[$('#faction').value],units=d.filter(id=>!card(id).kind).length,specials=d.length-units;$('#deckSummary').textContent=`${d.length} карт · ${units} бойцов · ${specials} спецкарт`;$('#startBtn').disabled=units<22||specials>10;return {units,specials};}
function setupFaction(enemy=false){let f=+$(enemy?'#opponent':'#faction').value,select=$(enemy?'#enemyLeader':'#leader');select.innerHTML=(enemy?'<option value="-1">Автоматически</option>':'')+META.leaders.filter(l=>l.faction===f).map(l=>`<option value="${l.code}">${esc(l.name)}</option>`).join('');if(!enemy){const saved=loadLeaderChoice(f);if(saved!==null)select.value=String(saved);}$(enemy?'#enemyAbility':'#factionAbility').textContent=META.factions[f].ability;leaderPreview(enemy);renderSetupControls();if(!enemy){$('#initiativeLabel').hidden=f!==3;deckSummary();}}
async function leaderPreview(enemy=false){
 const host=$(enemy?'#enemyLeaderPreview':'#leaderPreview'),code=+$(enemy?'#enemyLeader':'#leader').value;
 const stamp=String(code);host.dataset.requestedLeader=stamp;
 if(enemy&&code===-1){host.innerHTML='<div class="auto-leader">◇<p>Бот подберёт лидера и колоду<br>против вашей фракции.</p></div>';return;}
 const l=lmeta(code);if(!l)return;
 const img=new Image();img.src=l.asset;img.alt=l.name;
 try{await img.decode();}catch{}
 if(host.dataset.requestedLeader!==stamp)return;
 const description=document.createElement('p');description.textContent=l.description;
 host.replaceChildren(img,description);
}
function loadLeaderChoice(f){try{const value=JSON.parse(localStorage.getItem('gwent-leaders-v1')||'{}')[f];return META.leaders.some(l=>l.faction===f&&l.code===value)?value:null;}catch{return null;}}
function saveLeaderChoice(){try{const saved=JSON.parse(localStorage.getItem('gwent-leaders-v1')||'{}');saved[$('#faction').value]=+$('#leader').value;localStorage.setItem('gwent-leaders-v1',JSON.stringify(saved));}catch{}}

function editDeck(){editing=true;let f=+$('#faction').value,d=drafts[f],counts={};d.forEach(id=>counts[id]=(counts[id]||0)+1);let available=sortedCards(META.cards.filter(c=>!c.token&&(c.faction===-1||c.faction===f)).map(c=>c.id)).map(x=>card(x.id));let s=deckSummary();showContent(`<h2>Колода · ${esc(fname(f))}</h2><p id="draftCount">${s.units} бойцов / минимум 22 · ${s.specials} спецкарт / максимум 10</p><div class="editor-tools"><button id="resetDraft">Готовая колода</button><button id="saveDraft" class="gold">Готово</button></div><div class="collection-grid editor-grid">${available.map(c=>`<article>${picture(c.id,f)}<p>${esc(c.ru)}</p><div class="copy-controls"><button data-minus="${c.id}" ${counts[c.id]?'':'disabled'} aria-label="Убрать ${esc(c.ru)}">−</button><span>${counts[c.id]||0} / ${c.copies}</span><button data-plus="${c.id}" ${(counts[c.id]||0)>=c.copies?'disabled':''} aria-label="Добавить ${esc(c.ru)}">+</button></div></article>`).join('')}</div>`);}
function weatherRows(c){return c.name==='Skellige Storm'?6:c.weather;}
function rowAction(side,row){
 if(busy||!state?.ready||state.turn!==0||state.winner>=0)return null;
 if(pendingPlacement&&state.pending===0&&pendingPlacement.step===state.steps){if(side!==0)return null;return state.legal.find(a=>a.kind===2&&a.card===pendingPlacement.id&&a.row===row)||null;}
 if(selected==='leader'){
  const code=state.players[0].leader,moves=state.legal.filter(a=>a.kind===1),targets={0:[0,2],10:[0,0],16:[0,1],2:[1,1],7:[1,2],17:[1,0]},target=targets[code];
  if(target)return target[0]===side&&target[1]===row?moves.find(a=>a.target<0)||null:null;
  const weather=moves.filter(a=>a.target>=0&&weatherRows(card(a.target))&(1<<row));return weather.length===1?weather[0]:null;
 }
 if(selectedHand<0||typeof selected!=='number')return null;
 const c=card(selected),weather=weatherRows(c),global=c.name==='Scorch'||c.name==='Clear Weather';if(weather&&!(weather&(1<<row)))return null;
 if(!weather&&!global&&side!==((c.flags&2)?1:0))return null;
 const moves=state.legal.filter(a=>a.kind===0&&a.card===selected&&a.target<0&&(a.row===row||a.row<0));return moves.length===1?moves[0]:null;
}
document.addEventListener('click',e=>{if(e.target.closest('[data-use-leader]')){if(selected==='enemyLeader')selectCard(null,null);else activateLeader();return;}if(e.target.closest('[data-use-card]')){if(pendingPlacement)return;const moves=selectedHand>=0?state.legal.filter(a=>a.kind===0&&a.card===selected):[];if(moves.length===1&&moves[0].row<0)run('act '+moves[0].index);else if(selectedHand<0)selectCard(null,null);return;}if(e.target.closest('#leaderTargets')){selected='leaderTargets';renderInspector();return;}let x=e.target.closest('[data-action]');if(x){run('act '+x.dataset.action);return;}x=e.target.closest('[data-hand]');if(x&&!busy){const id=+x.dataset.card;if(selected===id&&selectedHand===+x.dataset.hand&&!state.ready&&state.swaps<2){run('swap '+x.dataset.hand);return;}if(selected===id&&selectedHand===+x.dataset.hand&&state.ready&&state.turn===0){const moves=state.legal.filter(a=>a.kind===0&&a.card===id);if(moves.length===1){run('act '+moves[0].index);return;}}selectCard(id,x.dataset.art,+x.dataset.hand);return;}x=e.target.closest('[data-board]');if(x){let id=+x.dataset.card;const dec=META.cards.find(c=>c.name==='Decoy').id;if(selected===dec&&+x.dataset.side===0){let a=state.legal.find(a=>a.kind===0&&a.card===dec&&a.target===+x.dataset.board);if(a){run('act '+a.index);return;}}const row=x.closest('[data-row]'),move=row&&rowAction(+row.dataset.side,+row.dataset.row);if(move){run('act '+move.index);return;}selectCard(id,x.dataset.art);return;}x=e.target.closest('[data-row]');if(x){const move=rowAction(+x.dataset.side,+x.dataset.row);if(move)run('act '+move.index);return;}x=e.target.closest('[data-leader]');if(x){const leader=+x.dataset.leader===0?'leader':'enemyLeader';if(selected===leader&&leader==='leader'&&state.legal.some(a=>a.kind===1)){activateLeader();return;}pendingPlacement=null;selected=selected===leader?null:leader;selectedHand=-1;renderInspector();return;}x=e.target.closest('[data-grave]');if(x){const p=state.players[+x.dataset.grave];showCards(+x.dataset.grave?'Сброс соперника':'Ваш сброс',p.grave,p.faction,{skins:p.graveSkins,variants:p.graveVariants});return;}if(e.target.closest('[data-deck]')){showCards('Ваша колода',state.decklist);return;}x=e.target.closest('[data-inspect]');if(x){inspect(+x.dataset.inspect);return;}if(e.target.closest('#swapBtn'))run('swap '+(selectedHand>=0?selectedHand:state.players[0].hand.indexOf(selected)));
 x=e.target.closest('.collection-grid [data-card]');if(x){let c=card(+x.dataset.card);$('#zoomBody').innerHTML=`<img src="${x.dataset.art}" alt="${esc(c.ru)}"><h2>${esc(c.ru)}</h2><p>${esc(desc(c))}</p>`;$('#zoom').showModal();return;}
 x=e.target.closest('[data-plus],[data-minus]');if(x){let f=+$('#faction').value,id=+(x.dataset.plus??x.dataset.minus),d=drafts[f];if(x.dataset.plus!==undefined&&d.filter(v=>v===id).length<card(id).copies)d.push(id);else if(x.dataset.minus!==undefined&&d.includes(id))d.splice(d.indexOf(id),1);let scroll=$('#content').scrollTop;editDeck();$('#content').scrollTop=scroll;return;}if(e.target.closest('#resetDraft')){drafts[$('#faction').value]=[...PRESETS[$('#faction').value]];editDeck();}if(e.target.closest('#saveDraft')){let s=deckSummary();if(s.units<22||s.specials>10){toast('Нужно минимум 22 бойца и не более 10 спецкарт.');return;}$('#content').close();editing=false;}
});
$('#passBtn').onclick=()=>{if(!state.ready)run('ready');else{let a=state.legal.find(a=>a.kind===3);if(a)run('act '+a.index);}};
$('#faction').onchange=()=>{setupFaction();renderFactionTabs();};$('#opponent').onchange=()=>setupFaction(true);$('#leader').onchange=()=>{saveLeaderChoice();leaderPreview();};$('#enemyLeader').onchange=()=>leaderPreview(true);$('#editDeck').onclick=editDeck;
$('#newBtn').onclick=()=>{if(!busy)$('#setup').showModal();};$('#closeSetup').onclick=()=>$('#setup').close();$('#closeContent').onclick=()=>{$('#content').close();editing=false;};
$('#startBtn').onclick=()=>{if(busy)return;enterFullscreen();let f=+$('#faction').value;resultShown=false;GwentEffects.stop();$('#setup').close();run(`new ${randomSeed()} ${f} ${$('#opponent').value} ${$('#leader').value} ${$('#enemyLeader').value} 1 ${$('#initiative').value} ${drafts[f].join(',')}`);};

$('#collectionBtn').onclick=()=>{editing=false;let f=state?.players[0].faction??+$('#faction').value;showContent(`<h2>Все карты набора</h2><div class="collection-tabs">${META.factions.map(x=>`<button data-collection="${x.id}">${esc(x.name)}</button>`).join('')}</div><div id="collectionCards"></div>`);collection(f);};
function collection(f){$('#collectionCards').innerHTML=`<h3>${esc(fname(f))}</h3><div class="collection-grid">${sortedCards(META.cards.filter(c=>c.faction===f||c.faction===-1).map(c=>c.id)).map(({id})=>{const c=card(id);return `<article>${picture(c.id,f)}<p>${esc(c.ru)}</p><small>${esc(ability(c))}</small></article>`;}).join('')}</div>`;}
document.addEventListener('click',e=>{let b=e.target.closest('[data-collection]');if(b)collection(+b.dataset.collection);});
$('#rulesBtn').onclick=()=>showContent(`<h2>Правила Гвинта</h2><div class="rules"><h3>Рубины и раунды</h3><p>У каждого игрока два рубина. После паса обоих игроков проигравший теряет один рубин. При равенстве оба теряют по рубину, кроме случая, когда только одна сторона играет Нильфгаардом: она побеждает. Потеря двух рубинов означает поражение; одновременная потеря последних — ничья.</p><h3>Ход и карты</h3><p>Стартовая рука — 10 карт, до двух замен. По очереди сыграйте карту, используйте активного лидера или спасуйте. Пас действует до конца раунда. Лекарь и его цепочка завершаются в рамках одного хода. Нового общего добора между раундами нет. Выигравшая сторона начинает следующий раунд; после ничьей первый игрок определяется случайно.</p><h3>Фракции</h3>${META.factions.map(f=>`<p><b>${esc(f.name)}.</b> ${esc(f.ability)}</p>`).join('')}<h3>Сила</h3><p>Погода меняет базовую силу, затем применяются Прочная связь, Прилив сил и Рог. Герои сохраняют свою силу. Казнь сравнивает текущую силу обычных бойцов. Бойцы отправляются в сброс той стороны, на которой стоят.</p><h3>Управление</h3><p>Нажмите карту в руке, затем подходящий ряд на поле. Казнь и Ясное небо можно разыграть нажатием на увеличенную карту. Для Чучела нажмите своего обычного бойца. Способность лидера применяется нажатием на его карту или выделенный ряд. После выбора проворного бойца для воскрешения укажите ряд на поле. У сброса видны количество и верхняя карта. Выбор бойца из сброса появляется при воскрешении или использовании соответствующего лидера. Состав своей колоды можно просмотреть отдельно. В новой партии можно составить колоду и выбрать любого лидера своей фракции.</p></div>`);
$('#closeZoom').onclick=()=>$('#zoom').close();
$('#again').onclick=()=>{$('#result').close();$('#setup').showModal();};$('#review').onclick=()=>$('#result').close();
(async()=>{try{[META,PRESETS]=await Promise.all([fetch('assets/catalog.json').then(r=>r.json()),fetch('assets/presets.json').then(r=>r.json())]);for(const l of META.leaders)if(l.asset)l.asset+='?v=edge1';loadDrafts();for(let id of ['#faction','#opponent'])$(id).innerHTML=META.factions.map(f=>`<option value="${f.id}">${esc(f.name)}</option>`).join('');$('#opponent').value='1';setupFaction();setupFaction(true);renderFactionTabs();try{state=await api('view');await prepareArtwork(state);if(!hadSavedDrafts&&state.decklist){drafts[state.players[0].faction]=[...state.decklist];saveDrafts();deckSummary();}render();if(state.ready&&state.turn===1&&state.winner<0)run('bot');}catch{$('#setup').showModal();}}catch(e){toast('Не удалось загрузить карты: '+e.message);}})();

function enterFullscreen(){const el=document.documentElement;const f=el.requestFullscreen||el.webkitRequestFullscreen;if(!f){toast('Полный экран в Safari: ⌃⌘F.');return;}if(f&&!document.fullscreenElement&&!document.webkitFullscreenElement){try{Promise.resolve(f.call(el)).catch(()=>toast('Полный экран: нажмите ⛶ или ⌃⌘F в Safari.'));}catch{toast('Полный экран: ⌃⌘F в Safari.');}}}
$('#fullscreenBtn').onclick=()=>{if(document.fullscreenElement||document.webkitFullscreenElement){const f=document.exitFullscreen||document.webkitExitFullscreen;f?.call(document);}else enterFullscreen();};
$('#historyBtn').onclick=()=>$('#historyModal').showModal();$('#closeHistory').onclick=()=>$('#historyModal').close();
$('#closeInspector').onclick=()=>selectCard(null,null);
$('#continueRound').onclick=()=>roundContinue?.();
document.addEventListener('keydown',e=>{if(e.key==='Escape'&&state)selectCard(null,null);});
