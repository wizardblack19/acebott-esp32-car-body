import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const bytes = readFileSync(new URL('body_test.wasm', import.meta.url));
const {instance} = await WebAssembly.instantiate(bytes, {});
const result = instance.exports.runTests();
assert.equal(result, 0, `C++ regression failed at body_test.cpp:${result}`);
console.log('PASS C++: protocol lengths, truncated packets, legacy stop, servo pin/angles, mode cancellation, music/shooting, watchdog, missing echo, stale light, tracking.');

const camera = new URL('../camara/acebott-esp32-car-camera/', import.meta.url);
assert.equal(readFileSync(new URL('../CommandProtocol.h', import.meta.url), 'utf8'),
  readFileSync(new URL('CommandProtocol.h', camera), 'utf8'));
const html = readFileSync(new URL('ControlPage.h', camera), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
new vm.Script(script);
const events = {}, elements = new Map(), requests = [];
function element(key) {
  if (!elements.has(key)) elements.set(key, {
    dataset: key === 'direction' ? {dir:'Forward'} : key === 'mode' ? {cmd:'Avoidance'} : {},
    listeners:{}, addEventListener(name, fn) {this.listeners[name] = fn;},
    setPointerCapture() {}, removeAttribute() {}
  });
  return elements.get(key);
}
const sandbox = {
  console, URLSearchParams, AbortController, Math, Date,
  setTimeout:() => 1, clearTimeout:() => {}, setInterval:fn => {events.heartbeat=fn;},
  location:{hostname:'192.168.4.1'},
  document:{hidden:false, querySelector:element,
    querySelectorAll:s => [element(s === '[data-dir]' ? 'direction' : 'mode')],
    addEventListener:(name,fn) => {events[name]=fn;}},
  window:{addEventListener:(name,fn) => {events[name]=fn;}},
  fetch:async url => { requests.push(String(url)); return {ok:true}; }
};
vm.runInNewContext(script,sandbox);
const settle = async () => { for(let i=0;i<10;i++) await Promise.resolve(); };
element('mode').onclick(); await settle();
assert(requests.at(-1).includes('cmd=Avoidance'));
events.heartbeat(); await settle(); assert(requests.at(-1).includes('cmd=ping'));
const direction=element('direction');
direction.listeners.pointerdown({pointerId:1,pointerType:'touch',preventDefault(){}});
await settle(); assert(requests.at(-1).includes('direction=Forward'));
direction.listeners.pointercancel({pointerId:1}); await settle();
assert(requests.at(-1).includes('direction=stop'));
events.blur(); await settle(); assert(requests.at(-1).includes('cmd=stopA'));
const count=requests.length; events.heartbeat(); await settle(); assert.equal(requests.length,count);
element('#video').onclick(); assert.equal(element('#camera').src,'http://192.168.4.1:82/stream');
console.log('PASS web: modes, session heartbeat, pointer cancellation, blur stop, separate video port.');
