// Keep a small, final-resolution portrait bitmap for each field card. Painting
// happens only when its source or physical size changes, never on an ordinary move.
function paintBoardPortraits(){
 for(const el of document.querySelectorAll('#battleRows .row-cards .card')){
  const img=el.querySelector('img'),canvas=el.querySelector('[data-portrait]');if(!img||!canvas||el.classList.contains('incinerating'))continue;
  if(!img.complete||!img.naturalWidth){if(!img._portraitPending){img._portraitPending=true;img.addEventListener('load',()=>{img._portraitPending=false;if(img.isConnected)paintBoardPortraits();},{once:true});}continue;}
  const r=el.getBoundingClientRect(),b=img.getBoundingClientRect(),dpr=devicePixelRatio||1,w=Math.max(1,Math.round(r.width*dpr)),h=Math.max(1,Math.round(r.height*dpr)),stamp=[img.currentSrc||img.src,w,h].join(':');
  if(canvas._portraitStamp===stamp)continue;
  canvas.width=w;canvas.height=h;const ctx=canvas.getContext('2d');ctx.imageSmoothingEnabled=true;ctx.imageSmoothingQuality='high';ctx.setTransform(w/r.width,0,0,h/r.height,0,0);ctx.drawImage(img,b.left-r.left,b.top-r.top,b.width,b.height);ctx.setTransform(1,0,0,1,0,0);canvas._portraitStamp=stamp;el.classList.add('portrait-ready');
 }
}
window.addEventListener('resize',paintBoardPortraits);
// Render at the final pixel size: scaling a composited card can briefly magnify
// Safari's small cached bitmap even when its source image is full resolution.
let boardPreviewSource=null,boardPreview=null,boardHoverTimer=null,boardHoverSource=null;
function closeBoardPreview(){clearTimeout(boardHoverTimer);boardHoverTimer=null;boardHoverSource=null;if(boardPreview){const closing=boardPreview;boardPreview=null;if(matchMedia('(prefers-reduced-motion: reduce)').matches)closing.remove();else closing.animate([{opacity:1},{opacity:0}],{duration:110,fill:'forwards'}).finished.then(()=>closing.remove(),()=>closing.remove());}boardPreviewSource?.classList.remove('magnified-source');boardPreviewSource=null;}
function showBoardPreview(source){
 if(source===boardPreviewSource||source.classList.contains('incinerating'))return;
 closeBoardPreview();
 const rect=source.getBoundingClientRect(),width=rect.width*1.65,height=rect.height*1.65;
 const wrapper=document.createElement('div');wrapper.className='board-magnifier row-cards';wrapper.setAttribute('aria-hidden','true');
 Object.assign(wrapper.style,{width:width+'px',height:height+'px',left:Math.max(4,Math.min(innerWidth-width-4,rect.left-(width-rect.width)/2))+'px',top:Math.max(4,Math.min(innerHeight-height-4,rect.top-(height-rect.height)/2))+'px'});
 const copy=source.cloneNode(true);copy.classList.remove('arriving','selected');copy.removeAttribute('id');copy.tabIndex=-1;
 wrapper.append(copy);document.body.append(wrapper);boardPreview=wrapper;boardPreviewSource=source;source.classList.add('magnified-source');
 if(!matchMedia('(prefers-reduced-motion: reduce)').matches)wrapper.animate([{opacity:0,translate:'0 5px'},{opacity:1,translate:'0 0'}],{duration:180,easing:'cubic-bezier(.2,.7,.2,1)'});
}
// A game update keeps a surviving preview and its decoded image, rather than
// closing/reopening it on every busy-state render and every dealt card.
function syncBoardPreview(){
 if(!boardPreviewSource||!boardPreview)return;
 if(!boardPreviewSource.isConnected){closeBoardPreview();return;}
 const source=boardPreviewSource,copy=boardPreview.firstElementChild;
 if(copy.innerHTML!==source.innerHTML)copy.replaceChildren(...[...source.childNodes].map(n=>n.cloneNode(true)));
 const r=source.getBoundingClientRect(),w=r.width*1.65,h=r.height*1.65;
 Object.assign(boardPreview.style,{width:w+'px',height:h+'px',left:Math.max(4,Math.min(innerWidth-w-4,r.left-(w-r.width)/2))+'px',top:Math.max(4,Math.min(innerHeight-h-4,r.top-(h-r.height)/2))+'px'});
}
const previewCard=target=>target instanceof Element?target.closest('#battleRows .row-cards .card:not(.incinerating)'):null;
document.addEventListener('pointerover',event=>{const source=previewCard(event.target);if(source&&source!==boardPreviewSource&&source!==boardHoverSource){closeBoardPreview();boardHoverSource=source;boardHoverTimer=setTimeout(()=>{if(source.isConnected)showBoardPreview(source);},150);}});
document.addEventListener('pointerout',event=>{const source=boardHoverSource||boardPreviewSource;if(source?.contains(event.target)&&!source.contains(event.relatedTarget))closeBoardPreview();});
document.addEventListener('focusin',event=>{const source=previewCard(event.target);if(source)showBoardPreview(source);});
document.addEventListener('focusout',closeBoardPreview);
document.addEventListener('pointerdown',closeBoardPreview,true);
window.addEventListener('resize',closeBoardPreview);
window.addEventListener('blur',closeBoardPreview);
