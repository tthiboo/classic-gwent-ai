/* Presentation stages keep the old layout through destruction, then settle the new layout. */
const GwentEffects=(()=>{
 const reduced=()=>document.hidden||matchMedia('(prefers-reduced-motion: reduce)').matches;
 const key=u=>[u.id,u.row,u.skin??'',u.variant??0].join(':');
 function difference(before,after){
  const pool=new Map();for(const u of before){const k=key(u);if(!pool.has(k))pool.set(k,[]);pool.get(k).push(u);}
  const added=[],paired=[];for(const u of after){const old=pool.get(key(u))?.shift();if(old)paired.push([old,u]);else added.push(u);}
  return {added,paired,removed:[...pool.values()].flat()};
 }
 function plan(before,after,c){
  if(!before)return null;const round=after.round!==before.round||after.winner>=0&&before.winner<0;
  return {round,boards:before.players.map((p,i)=>difference(p.board,after.players[i].board)),weatherAdded:after.weather&~before.weather,weatherRemoved:before.weather&~after.weather,clear:c?.name==='Clear Weather'||before.weather!==0&&after.weather===0&&!round,burn:!round&&(c?.name==='Scorch'||!!(c?.flags&(32|4096|8192|16384))||before.players.some((p,i)=>!p.used&&after.players[i].used&&[2,7,17].includes(p.leader)))};
 }
 let layer=null,animations=new Set(),timers=new Set(),canvases=new Set();
 function root(){if(!layer){layer=document.createElement('div');layer.id='table-effects';layer.setAttribute('aria-hidden','true');document.body.append(layer);}return layer;}
 function animate(el,frames,options={},remove=false){
  if(reduced()){if(remove)el.remove();return;}
  const a=el.animate(frames,{duration:600,fill:'backwards',easing:'cubic-bezier(.35,0,.2,1)',...options});animations.add(a);
  const done=()=>{animations.delete(a);if(remove)el.remove();};a.finished.then(done,done);return a;
 }
 const timing={burn:3000,place:600,shift:650,ruby:1900,weather:2400,clear:2500,signal:1600};
 async function settled(){await Promise.all([...animations].map(a=>a.finished.catch(()=>{})));}
 function ghost(snapshot){if(!snapshot?.rect.width)return null;const el=snapshot.copy.cloneNode(true);el.removeAttribute('id');el.className='effect-card '+(snapshot.small?'small':'');el.tabIndex=-1;Object.assign(el.style,{left:snapshot.rect.left+'px',top:snapshot.rect.top+'px',width:snapshot.rect.width+'px',height:snapshot.rect.height+'px'});root().append(el);return el;}
 function shot(el){const rect=el.getBoundingClientRect();const img=el.querySelector('img');return {rect,copy:el.cloneNode(true),portraitElement:el.querySelector('[data-portrait]'),imageElement:img,imageSource:img?.currentSrc||img?.src,imageRect:img?.getBoundingClientRect(),powerSource:el.querySelector('.current-power')?.style.backgroundImage.match(/url\(["']?([^"')]+)/)?.[1],powerRect:el.querySelector('.current-power')?.getBoundingClientRect(),small:el.classList.contains('small')};}
 function capture(beforeState,after,c){
  const planned=beforeState&&after?plan(beforeState,after,c):null,removed=planned?new Set(planned.boards.flatMap((d,side)=>d.removed.map(u=>side+':'+u.index))):null;
  const out={cards:new Map(),handRects:new Map(),weather:[],gems:[]};if(reduced())return out;
  document.querySelectorAll('#battleRows [data-board]').forEach(el=>{const key=el.dataset.side+':'+el.dataset.board;if(!removed||removed.has(key))out.cards.set(key,shot(el));});
  
  if(!planned||planned.weatherRemoved)document.querySelectorAll('.weather-fx').forEach(el=>{const row=el.closest('.battle-row'),copy=el.cloneNode(true),computed=getComputedStyle(el);copy.style.clipPath=computed.clipPath;
   // Continue each texture at its current phase while Clear Weather dissolves it.
   // A fresh clone would restart the rainfall/fog loop for a visible jump.
   [...el.children].forEach((child,i)=>{const a=child.getAnimations().find(a=>typeof a.animationName==='string');if(a&&typeof a.currentTime==='number')copy.children[i].style.animationDelay=((a.effect?.getTiming().delay||0)-a.currentTime)+'ms';});
   out.weather.push({side:+row.dataset.side,row:+row.dataset.row,copy,opacity:computed.opacity});});
  if(!planned||planned.round)document.querySelectorAll('[data-ruby].alive').forEach(el=>out.gems.push({key:el.dataset.ruby,...shot(el)}));

  return out;
 }
 const node=(side,u)=>document.querySelector(`#battleRows [data-side="${side}"][data-board="${u.index}"]`);
 function rowPulse(row,type,delay=0){if(!row||reduced())return;const el=document.createElement('span');el.className='row-signal '+type;el.setAttribute('aria-hidden','true');row.append(el);
  if(type==='frost-wave'||type==='sun-wave'){el.innerHTML='<i></i>';animate(el.firstChild,[{transform:'translateX(-100%)'},{transform:'translateX(100%)'}],{duration:type==='sun-wave'?timing.clear:timing.weather,easing:'linear',delay});animate(el,[{opacity:0},{opacity:1,offset:.12},{opacity:1,offset:.85},{opacity:0}],{duration:type==='sun-wave'?timing.clear:timing.weather,delay},true);}
  else {el.innerHTML='<i class=signal-ripple></i><i class=signal-ripple></i><span class=signal-dust></span>';for(const [i,ripple] of [...el.querySelectorAll('.signal-ripple')].entries())animate(ripple,[{opacity:0,transform:'scaleX(.1)'},{opacity:.7,offset:.35},{opacity:0,transform:'scaleX(1)'}],{duration:timing.signal-i*180,delay:delay+i*180});animate(el,[{opacity:0},{opacity:1,offset:.18},{opacity:1,offset:.75},{opacity:0}],{duration:timing.signal,delay},true);}
 }

 function arrival(el,delay=0){if(!el||reduced())return;
  // Cards remain fully opaque. A small landing motion replaces dissolving ghosts.
  animate(el,[{top:'-8px'},{top:'0px'}],{duration:timing.place,delay});
 }
 function burn(shots){
  if(!shots.length||reduced())return Promise.resolve();
  const left=Math.min(...shots.map(x=>x.rect.left))-28,top=Math.min(...shots.map(x=>x.rect.top))-60,right=Math.max(...shots.map(x=>x.rect.right))+28,bottom=Math.max(...shots.map(x=>x.rect.bottom))+28;
  const canvas=document.createElement('canvas'),dpr=devicePixelRatio||1,w=right-left,h=bottom-top;canvas.width=Math.ceil(w*dpr);canvas.height=Math.ceil(h*dpr);canvas.className='scorch-canvas';Object.assign(canvas.style,{left:left+'px',top:top+'px',width:w+'px',height:h+'px'});root().append(canvas);const ctx=canvas.getContext('2d');ctx.scale(dpr,dpr);
  const clamp=x=>Math.max(0,Math.min(1,x));
  const noise=(x,y,n)=>{const v=Math.sin(x*127.1+y*311.7+n*91.3)*43758.5453;return v-Math.floor(v);};
  const surface=(width,height)=>{const c=document.createElement('canvas');c.width=width;c.height=height;return c;};
  const sprites=shots.map((s,n)=>{
   const r=s.rect,image=surface(Math.ceil(r.width*dpr),Math.ceil(r.height*dpr)),c=image.getContext('2d');c.scale(dpr,dpr);const b=s.imageRect||r;
   // Reuse the already decoded portrait, at the actual screen resolution.
   if(s.portraitElement?._portraitStamp)c.drawImage(s.portraitElement,0,0,r.width,r.height);else if(s.imageElement?.complete)c.drawImage(s.imageElement,b.left-r.left,b.top-r.top,b.width,b.height);
   if(s.portraitElement&&!s.portraitElement._portraitStamp){s.portraitElement.width=image.width;s.portraitElement.height=image.height;s.portraitElement.parentElement.classList.add('portrait-ready');}
   const power=s.powerElement;if(power?.complete&&s.powerRect){const p=s.powerRect;c.drawImage(power,p.left-r.left,p.top-r.top,p.width,p.height);}
   const charred=surface(image.width,image.height),ash=charred.getContext('2d');
   // Prepare charcoal without synchronous GPU readback or a canvas filter.
   ash.drawImage(image,0,0);ash.globalCompositeOperation='source-atop';ash.globalAlpha=.84;ash.fillStyle='#080707';ash.fillRect(0,0,image.width,image.height);ash.globalAlpha=1;ash.globalCompositeOperation='source-over';
   // Fine permanent fractures in the charred layer, not a tint over intact art.
   ash.strokeStyle='#aaa6';ash.lineWidth=dpr*.5;for(let i=0;i<13;i++){const x=noise(i,1,n)*image.width,y=noise(i,2,n)*image.height;ash.beginPath();ash.moveTo(x,y);ash.lineTo(x+(noise(i,3,n)-.5)*24*dpr,y+17*dpr);ash.lineTo(x+(noise(i,4,n)-.5)*29*dpr,y+31*dpr);ash.stroke();}ash.globalCompositeOperation='destination-in';ash.drawImage(image,0,0);ash.globalCompositeOperation='source-over';
   const mw=Math.ceil(r.width/3),mh=Math.ceil(r.height/3),masks=Array.from({length:3},()=>surface(mw,mh)),contexts=masks.map(c=>c.getContext('2d')),data=contexts.map(c=>c.createImageData(mw,mh)),thresholds=new Float32Array(mw*mh);
   const seeds=[[.16,.68],[.79,.29],[.45,.9]];
   for(let y=0;y<mh;y++)for(let x=0;x<mw;x++){
    const u=x/mw,v=y/mh;let distance=Infinity;for(let i=0;i<seeds.length;i++){const [sx,sy]=seeds[i];distance=Math.min(distance,Math.hypot((u-sx)*.8,(v-sy)*1.15)+i*.045);}
    const coarse=noise(Math.floor(x/4),Math.floor(y/5),n),fine=noise(x,y,n);
    const index=y*mw+x;thresholds[index]=.06+distance*.69+coarse*.09+fine*.035;for(let j=0;j<2;j++){const p=data[j].data,k=index*4;p[k]=p[k+1]=p[k+2]=255;}const p=data[2].data,k=index*4;p[k]=255;p[k+1]=Math.round(80+140*noise(index,3,n));p[k+2]=18;
   }
   return {x:r.left-left,y:r.top-top,w:r.width,h:r.height,display:s.portraitElement,image,charred,work:surface(image.width,image.height),masks,contexts,data,thresholds,mw,mh,n};
  });
  const fragments=Array.from({length:Math.min(144,shots.length*14)},(_,i)=>{const s=sprites[i%sprites.length],u=noise(i,7,0),v=noise(i,8,0),at=s.thresholds[Math.min(s.mh-1,Math.floor(v*s.mh))*s.mw+Math.min(s.mw-1,Math.floor(u*s.mw))];return {s,u,v,at:at+.13,drift:(noise(i,9,0)-.5)*42,lift:18+noise(i,10,0)*52,size:1+noise(i,11,0)*3,phase:noise(i,12,0)*6};});
  // Let the last fragments erode near the end, leaving only a short ash tail.
  const extent=Math.max(...sprites.map(s=>Math.max(...s.thresholds)))+.33;
  const started=performance.now();let frame=0,finish;const finished=new Promise(resolve=>finish=resolve);const job={finished,stop(){cancelAnimationFrame(frame);canvas.remove();canvases.delete(job);finish();}};canvases.add(job);
  function draw(now){if(document.hidden){job.stop();return;}const t=Math.min(1,(now-started)/timing.burn),progress=t*extent/.94;ctx.clearRect(0,0,w,h);
   for(const s of sprites){
    for(let i=0;i<s.thresholds.length;i++){
     const age=progress-s.thresholds[i],intact=1-clamp(age/.075),char=clamp(age/.055)*(1-clamp((age-.12)/.21)),heat=clamp(1-Math.abs(age-.055)/.07)*(1-clamp((t-.78)/.13));
     const k=i*4;s.data[0].data[k+3]=Math.round(255*intact);s.data[1].data[k+3]=Math.round(255*char);s.data[2].data[k+3]=Math.round(heat*220);
    }
    for(let j=0;j<3;j++)s.contexts[j].putImageData(s.data[j],0,0);
    const c=s.work.getContext('2d'),display=s.display?.getContext('2d');if(display){display.setTransform(1,0,0,1,0,0);display.clearRect(0,0,s.display.width,s.display.height);}
    for(let j=0;j<2;j++){c.clearRect(0,0,s.work.width,s.work.height);c.globalCompositeOperation='source-over';c.drawImage(j?s.charred:s.image,0,0);c.globalCompositeOperation='destination-in';c.drawImage(s.masks[j],0,0,s.work.width,s.work.height);if(display)display.drawImage(s.work,0,0,s.display.width,s.display.height);else ctx.drawImage(s.work,s.x,s.y,s.w,s.h);}
    c.clearRect(0,0,s.work.width,s.work.height);c.globalCompositeOperation='source-over';c.drawImage(s.masks[2],0,0,s.work.width,s.work.height);c.globalCompositeOperation='destination-in';c.drawImage(s.image,0,0);ctx.save();ctx.globalCompositeOperation='screen';ctx.drawImage(s.work,s.x,s.y,s.w,s.h);ctx.restore();
   }
   for(const e of fragments){const age=progress-e.at;if(age<0||age>.43)continue;
    if(age<.12&&t<.82){const heat=Math.sin(Math.PI*age/.12),x=e.s.x+e.s.w*e.u,y=e.s.y+e.s.h*e.v,height=(9+e.size*3)*heat,bend=Math.sin(t*19+e.phase)*5;ctx.save();ctx.globalAlpha=.7*heat;ctx.fillStyle='#e39a45';ctx.beginPath();ctx.moveTo(x-2,y);ctx.bezierCurveTo(x-4,y-height*.4,x+bend,y-height*.6,x+bend+2,y-height);ctx.bezierCurveTo(x+bend+4,y-height*.4,x+4,y-height*.2,x+2,y);ctx.closePath();ctx.fill();ctx.restore();}
    const life=age/.43,px=e.s.x+e.s.w*e.u+e.drift*life,py=e.s.y+e.s.h*e.v-e.lift*life+13*life*life;ctx.save();ctx.translate(px,py);ctx.rotate(e.phase+life*3);ctx.globalAlpha=(1-life)*Math.min(1,life*12);ctx.fillStyle=life<.16?'#db9a55':e.phase>3?'#77736d':'#262524';ctx.beginPath();ctx.moveTo(-e.size,-e.size*.6);ctx.lineTo(e.size*.9,0);ctx.lineTo(0,e.size);ctx.closePath();ctx.fill();ctx.restore();}
   if(t<1)frame=requestAnimationFrame(draw);else job.stop();
  }draw(started);return finished;
 }
 function stage(beforeState,after,c){
  const p=plan(beforeState,after,c);if(!p?.burn||p.round||!p.boards.some(d=>d.removed.length)||!p.boards.some(d=>d.added.length))return null;
  const staged=structuredClone(beforeState);
  for(let side=0;side<2;side++){
   const source=after.players[side],player=staged.players[side];let index=Math.max(-1,...player.board.map(u=>u.index))+1;
   for(const u of p.boards[side].added)player.board.push({...u,index:index++});
   for(const field of ['hand','handSkins','handVariants','handCount','deckCount','used'])if(source[field]!==undefined)player[field]=structuredClone(source[field]);
   player.score=player.board.reduce((sum,u)=>sum+u.power,0);
  }return staged;
 }
 async function before(beforeState,after,c,snapshot){const p=plan(beforeState,after,c);if(!p?.burn||reduced())return;
  const shots=[],held=[];
  for(let side=0;side<2;side++)for(const u of p.boards[side].removed){const saved=snapshot.cards.get(side+':'+u.index),el=node(side,u);if(saved){if(saved.powerSource){const img=new Image();img.src=saved.powerSource;await img.decode().catch(()=>{});saved.powerElement=img;}shots.push(saved);if(el){if(el.classList.contains('magnified-source'))closeBoardPreview();held.push(el);el.classList.add('incinerating');if(!saved.portraitElement)el.style.visibility='hidden';}}}
  // Mutate the actual portrait surfaces. They stay burnt/empty until removal;
  // restoring the intact DOM portrait at the end would cause a visible pop.
  try{await burn(shots);}catch(error){for(const el of held){el.classList.remove('incinerating');el.style.visibility='';const canvas=el.querySelector('[data-portrait]');if(canvas)canvas._portraitStamp='';}if(typeof paintBoardPortraits==='function')paintBoardPortraits();throw error;}
 }
 function clearSky(){
  // Rows are absolutely positioned, so their parent has no intrinsic height.
  // Use their union to cover the entire battlefield, including the centre gap.
  const rows=[...document.querySelectorAll('#battleRows .battle-row')].map(el=>el.getBoundingClientRect());if(!rows.length||reduced())return;
  const left=Math.min(...rows.map(r=>r.left)),top=Math.min(...rows.map(r=>r.top)),right=Math.max(...rows.map(r=>r.right)),bottom=Math.max(...rows.map(r=>r.bottom)),el=document.createElement('div');el.className='sky-clearing';el.innerHTML='<i class="sky-canopy"></i><i class="sky-light"></i><i class="sky-air"></i>';Object.assign(el.style,{left:left+'px',top:top+'px',width:(right-left)+'px',height:(bottom-top)+'px'});root().append(el);
  animate(el.querySelector('.sky-light'),[{transform:'translateX(-45%)'},{transform:'translateX(45%)'}],{duration:timing.clear,easing:'linear'});
  animate(el.querySelector('.sky-canopy'),[{opacity:.6,transform:'translateY(0)'},{opacity:0,transform:'translateY(-35%)'}],{duration:timing.clear});
  animate(el,[{opacity:0},{opacity:1,offset:.22},{opacity:.85,offset:.7},{opacity:0}],{duration:timing.clear},true);
 }

 function weather(p,snapshot){
  if(p.clear)clearSky();
  for(let side=0;side<2;side++)for(let r=0;r<3;r++){
   const row=document.querySelector(`.battle-row[data-side="${side}"][data-row="${r}"]`);if(!row)continue;
   if(p.weatherAdded&(1<<r)){const fx=row.querySelector('.weather-fx');if(fx){animate(fx,r===0?[{clipPath:'inset(0 100% 0 0)',opacity:.2},{clipPath:'inset(0 0 0 0)',opacity:1}]:[{opacity:0},{opacity:1}],{duration:r===0?timing.weather:r===1?2800:2600});if(r===2)for(const [i,el] of [...fx.children].entries())animate(el,[{opacity:0},{opacity:getComputedStyle(el).opacity}],{duration:1800,delay:i*350});}if(r===0)rowPulse(row,'frost-wave');}
   if(p.weatherRemoved&(1<<r)){const old=snapshot.weather.find(x=>x.side===side&&x.row===r);if(old){const remnant=document.createElement('span');remnant.className='weather-remnant weather-'+r;old.copy.style.opacity=old.opacity;remnant.append(old.copy);row.append(remnant);animate(remnant,p.clear?[{opacity:1},{opacity:.65,offset:.4},{opacity:0}]:[{opacity:1},{opacity:0}],{duration:p.clear?timing.clear:1200},true);}}

  }
 }
 function bond(side,units){if(units.length<2||reduced())return;const nodes=units.map(u=>node(side,u)).filter(Boolean),boxes=nodes.map(el=>el.getBoundingClientRect());if(boxes.length<2)return;
  const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.classList.add('bond-thread');const y=boxes[0].top+boxes[0].height*.16,left=Math.min(...boxes.map(r=>r.left+r.width*.17)),right=Math.max(...boxes.map(r=>r.left+r.width*.17));Object.assign(svg.style,{left:left+'px',top:(y-7)+'px',width:(right-left+1)+'px',height:'14px'});svg.setAttribute('viewBox',`0 0 ${right-left+1} 14`);const line=document.createElementNS(svg.namespaceURI,'path');line.setAttribute('d',`M0 7 Q${(right-left)/2} 0 ${right-left} 7`);svg.append(line);root().append(svg);animate(svg,[{opacity:0,transform:'scaleX(.7)'},{opacity:1,offset:.4,transform:'none'},{opacity:0}],{duration:1500},true);for(const el of nodes){const medal=el.querySelector('.current-power');if(medal)animate(medal,[{opacity:.35},{opacity:1}],{duration:900});}
 }
 async function transition(before,after,c,snapshot){const p=plan(before,after,c);if(!p||reduced())return;weather(p,snapshot);
  if(p.round){for(let side=0;side<2;side++){for(const [i,u] of p.boards[side].removed.entries()){const old=ghost(snapshot.cards.get(side+':'+u.index));if(old)animate(old,[{opacity:1},{opacity:0,transform:'translateY(12px) scale(.96)'}],{duration:650,delay:Math.min(150,i*25)},true);}for(const [i,u] of p.boards[side].added.entries())arrival(node(side,u),Math.min(180,i*35));}return settled();}
  for(let side=0;side<2;side++){
   const d=p.boards[side];for(const u of d.removed){const old=snapshot.cards.get(side+':'+u.index);if(p.burn){continue;}else if(c?.name==='Decoy'){continue;}else {const oldGhost=ghost(old);if(oldGhost)animate(oldGhost,[{opacity:1},{opacity:0}],{duration:600},true);}}
   d.added.forEach((u,i)=>arrival(node(side,u),Math.min(240,i*80)));
   for(const [old,u] of d.paired)if(old.power!==u.power){const medal=node(side,u)?.querySelector('.current-power');if(medal)animate(medal,[{opacity:.3},{opacity:1}],{duration:700});}
   const groups=new Map();for(const u of after.players[side].board){const x=card(u.id);if(x.flags&8){const group=(x.group>=0?x.group:x.id)+':'+u.row;if(!groups.has(group))groups.set(group,[]);groups.get(group).push(u);}}
   for(const units of groups.values())if(units.some(u=>d.added.includes(u)))bond(side,units);
   for(let r=0;r<3;r++){const row=document.querySelector(`.battle-row[data-side="${side}"][data-row="${r}"]`);if(d.added.some(u=>u.row===r&&(card(u.id).flags&128)))rowPulse(row,'summon-wave');if((after.players[side].horn&~before.players[side].horn)&(1<<r)||d.added.some(u=>u.row===r&&(card(u.id).name==="Commander's Horn"||card(u.id).flags&64)))rowPulse(row,'horn-wave');if(d.added.some(u=>u.row===r&&/Vildkaarl/.test(card(u.id).name)))rowPulse(row,'transform-wave');if(before.pending===side&&d.added.some(u=>u.row===r))rowPulse(row,'revive-wave');}
  }
  for(let side=0;side<2;side++)if(!before.players[side].used&&after.players[side].used){const slot=document.querySelector(`[data-leader="${side}"]`);if(slot)animate(slot,[{filter:'brightness(1.6)'},{filter:'brightness(1)'}],{duration:1200});if(after.players[side].leader===20)for(const pile of document.querySelectorAll('.deck-stack'))animate(pile,[{opacity:.5,transform:'translateY(3px)'},{opacity:1,transform:'none'}],{duration:900});}
  if(before.ready&&after.ready){
   const handUnits=p=>(p.hand||[]).map((id,index)=>({id,index,row:-1,skin:p.handSkins?.[index]??p.faction,variant:p.handVariants?.[index]??0}));
   const hand=difference(handUnits(before.players[0]),handUnits(after.players[0]));

   hand.added.forEach((u,i)=>arrival(document.querySelector(`#hand [data-hand="${u.index}"]`),Math.min(160,i*80)));
  }

  for(const side of [0,1])if(after.players[side].score!==before.players[side].score){const el=document.querySelector(side?'#enemyPanel .score strong':'#ownPanel .score strong');if(el)animate(el,[{opacity:.5,transform:'translateY(3px)'},{opacity:1,transform:'none'}],{duration:500});}
  for(const side of [0,1])if(after.players[side].handCount>before.players[side].handCount){const indicator=document.querySelector(side?'#enemyPanel .player-hand-count':'#ownPanel .player-hand-count');if(indicator)animate(indicator,[{transform:'translateY(-3px)'},{transform:'none'}],{duration:700});}
  return settled();
 }
 async function round(snapshot,losses,outcome){if(reduced())return;
  for(let side=0;side<2;side++)for(let n=0;n<losses[side];n++){const gems=snapshot.gems.filter(x=>x.key.startsWith(side+':')),s=gems[gems.length-1-n];if(!s)continue;
   const whole=ghost(s);if(whole){whole.className='ruby-cracking';whole.style.backgroundImage='url(assets/ui/ruby.svg)';whole.style.backgroundSize='contain';whole.style.backgroundRepeat='no-repeat';whole.innerHTML='<svg viewBox="0 0 52 64"><path d="m26 6-5 15 9 10-12 17 6 10m6-27 12-9m-24 26-9-2"/></svg>';animate(whole,[{opacity:1},{opacity:1,offset:.55},{opacity:0,offset:.7},{opacity:0}],{duration:timing.ruby},true);}
   const clips=['polygon(0 0,55% 0,40% 60%,0 75%)','polygon(55% 0,100% 0,100% 100%,60% 65%,40% 60%)','polygon(0 75%,40% 60%,60% 65%,100% 100%,0 100%)'];for(let i=0;i<3;i++){const shard=ghost(s);if(!shard)continue;shard.className='ruby-shard';shard.style.clipPath=clips[i];animate(shard,[{opacity:0,transform:'none'},{opacity:0,offset:.48,transform:'none'},{opacity:1,offset:.55,transform:'none'},{opacity:0,transform:`translate(${(i-1)*19}px,${15+i*6}px) rotate(${(i-1)*25}deg)`}],{duration:timing.ruby},true);}
  }
  const arena=document.querySelector('.arena');if(arena)rowPulse(arena,'round-'+outcome);return settled();
 }
 function deal(){} // Initial hand appears directly, without fading or flying duplicates.
 function result(el){if(!el||reduced())return;animate(el,[{opacity:0,transform:'translateY(14px) scale(.97)'},{opacity:1,transform:'none'}],{duration:700});}
 function stop(){for(const a of animations)a.cancel();animations.clear();for(const id of timers)clearTimeout(id);timers.clear();for(const c of [...canvases])c.stop();layer?.replaceChildren();document.querySelectorAll('.row-signal,.weather-remnant,.sky-clearing').forEach(el=>el.remove());}
 document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});window.addEventListener('pagehide',stop);window.addEventListener('resize',stop);
 return {capture,stage,before,transition,round,deal,result,stop,settled,timing,plan,difference,active:()=>({animations:animations.size,canvases:canvases.size,timers:timers.size})};
})();
