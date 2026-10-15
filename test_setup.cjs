const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('game/web/app.js','utf8');
const META=JSON.parse(fs.readFileSync('game/web/assets/catalog.json','utf8'));
const nodes={'#faction':{value:'0'},'#leader':{},'#leaderPreview':{dataset:{},replaceChildren(...children){this.children=children;}}};
const saved=new Map(),pending=[];
const c={META,$:s=>nodes[s],lmeta:id=>META.leaders.find(l=>l.code===id),localStorage:{getItem:k=>saved.get(k),setItem:(k,v)=>saved.set(k,v)},document:{createElement:()=>({})},Image:class{decode(){return new Promise(resolve=>pending.push(resolve));}}};
vm.createContext(c);vm.runInContext(source.slice(source.indexOf('async function leaderPreview'),source.indexOf('function editDeck')),c);
(async()=>{
 const leaders=META.leaders.filter(l=>l.faction===0);
 nodes['#leader'].value=leaders[1].code;c.saveLeaderChoice();
 nodes['#faction'].value='1';nodes['#leader'].value=META.leaders.find(l=>l.faction===1).code;c.saveLeaderChoice();
 assert.equal(c.loadLeaderChoice(0),leaders[1].code);assert.equal(c.loadLeaderChoice(4),null);
 nodes['#leader'].value=leaders[0].code;const first=c.leaderPreview();
 nodes['#leader'].value=leaders[1].code;const second=c.leaderPreview();
 pending[1]();await second;pending[0]();await first;
 assert.equal(nodes['#leaderPreview'].children[0].alt,leaders[1].name);
 console.log('PASS: per-faction leader persistence and out-of-order artwork loading');
})().catch(e=>{console.error(e);process.exitCode=1});
