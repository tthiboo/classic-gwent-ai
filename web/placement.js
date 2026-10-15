// Destination validation is shared with click-to-play and the server's legal moves.
let draggingHand=false;
function placementReady(){return !!state&&state.ready&&state.turn===0&&state.winner<0&&!busy&&state.pending<0&&selectedHand>=0&&typeof selected==='number'&&state.players[0].hand[selectedHand]===selected;}
function targetCardAction(el){
 if(!placementReady()||card(selected).name!=='Decoy'||!el||+el.dataset.side!==0)return null;
 return state.legal.find(a=>a.kind===0&&a.card===selected&&a.target===+el.dataset.board)||null;
}
function placementAction(target){
 if(!placementReady())return null;
 const unit=target.closest('[data-board]'),specific=targetCardAction(unit);
 if(specific)return specific;
 const row=target.closest('.battle-row');return row?rowAction(+row.dataset.side,+row.dataset.row):null;
}
function highlightDestinations(){
 document.querySelectorAll('.battle-row').forEach(row=>{
  const legal=(rowAction(+row.dataset.side,+row.dataset.row)||[...row.querySelectorAll('[data-board]')].some(targetCardAction));
  row.classList.toggle('legal-destination',!!legal);row.classList.remove('drag-destination');
 });
 document.querySelectorAll('#battleRows [data-board]').forEach(el=>el.classList.toggle('legal-card-target',!!targetCardAction(el)));
}
let handPointer=null,handGhost=null,suppressHandClick=false;
function endHandDrag(){
 draggingHand=false;handPointer=null;handGhost?.remove();handGhost=null;
 document.body.classList.remove('dragging-hand');
 document.querySelectorAll('.drag-destination').forEach(el=>el.classList.remove('drag-destination'));
}
document.addEventListener('dragstart',event=>{if(event.target.closest('#hand [data-hand]'))event.preventDefault();});
document.addEventListener('pointerdown',event=>{
 const el=event.target.closest('#hand [data-hand]');if(!el||event.button!==0)return;
 if(!state?.ready||state.turn!==0||state.winner>=0||busy||state.pending>=0)return;
 const id=+el.dataset.card;if(!state.legal.some(a=>a.kind===0&&a.card===id))return;
 handPointer={el,id,pointerId:event.pointerId,x:event.clientX,y:event.clientY};
});
document.addEventListener('pointermove',event=>{
 const p=handPointer;if(!p||p.pointerId!==event.pointerId)return;
 if(!draggingHand){
  if(Math.hypot(event.clientX-p.x,event.clientY-p.y)<8)return;
  closeBoardPreview();selected=p.id;selectedHand=+p.el.dataset.hand;selectedArt=p.el.dataset.art;
  draggingHand=true;document.body.classList.add('dragging-hand');highlightDestinations();
  const rect=p.el.getBoundingClientRect();handGhost=p.el.cloneNode(true);handGhost.removeAttribute('id');handGhost.setAttribute('aria-hidden','true');
  handGhost.className='card hand-drag-ghost';Object.assign(handGhost.style,{width:rect.width+'px',height:rect.height+'px'});document.body.append(handGhost);
  p.el.setPointerCapture(event.pointerId);
 }
 event.preventDefault();handGhost.style.left=event.clientX+'px';handGhost.style.top=event.clientY+'px';
 document.querySelectorAll('.drag-destination').forEach(el=>el.classList.remove('drag-destination'));
 const target=document.elementFromPoint(event.clientX,event.clientY);
 if(target&&placementAction(target))target.closest('.battle-row')?.classList.add('drag-destination');
});
document.addEventListener('pointerup',event=>{
 if(!handPointer||handPointer.pointerId!==event.pointerId)return;
 if(!draggingHand){handPointer=null;return;}
 const target=document.elementFromPoint(event.clientX,event.clientY),action=target&&placementAction(target);
 suppressHandClick=true;setTimeout(()=>suppressHandClick=false,0);endHandDrag();
 if(action)run('act '+action.index);else renderInspector();
});
document.addEventListener('click',event=>{if(suppressHandClick){event.preventDefault();event.stopImmediatePropagation();}},true);
document.addEventListener('pointercancel',endHandDrag);
window.addEventListener('blur',endHandDrag);
