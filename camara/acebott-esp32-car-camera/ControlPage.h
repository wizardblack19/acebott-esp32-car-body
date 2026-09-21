#pragma once
const char html[] PROGMEM = R"HTML(
<!doctype html><html lang="es"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ACEBOTT</title>
<style>
body{font-family:system-ui;max-width:680px;margin:auto;padding:16px;background:#eef2f6}
button{padding:12px;margin:4px;border:0;border-radius:6px;background:#1765c1;color:white;touch-action:none}
.drive{display:grid;grid-template-columns:repeat(3,1fr)}
img{width:100%;min-height:160px;background:#111}label{display:block;margin:16px 0}input{width:65%}
.stop{background:#b32020}#status{min-height:24px}
</style><h1>ACEBOTT</h1><img id="camera" alt="Camara detenida">
<div><button id="video">Ver video</button><button id="photo">Foto</button><button id="close">Cerrar video</button></div>
<p id="status" role="status">Listo</p>
<div class="drive">
<button data-dir="LeftUp">↖</button><button data-dir="Forward">Avanzar</button><button data-dir="RightUp">↗</button>
<button data-dir="Left">Izquierda</button><button class="stop" id="stop">PARAR TODO</button><button data-dir="Right">Derecha</button>
<button data-dir="LeftDown">↙</button><button data-dir="Backward">Retroceder</button><button data-dir="RightDown">↘</button>
<button data-dir="Anticlockwise">Girar izquierda</button><span></span><button data-dir="Clockwise">Girar derecha</button>
</div>
<label>Velocidad <input id="speed" type="range" min="1" max="5" value="3"></label>
<label>Servos (grados) <input id="servo" type="range" min="0" max="180" value="90" step="5"></label>
<div><button data-cmd="Track" data-value="1">Seguir linea 1</button><button data-cmd="Track" data-value="2">Seguir linea 2</button>
<button data-cmd="Avoidance">Evitar obstaculos</button><button data-cmd="Follow">Seguir objeto</button><button data-cmd="Light">Seguir luz</button></div>
<div><button data-cmd="LED" data-value="1">Luces ON</button><button data-cmd="LED" data-value="0">Luces OFF</button>
<button data-cmd="CAM_LED" data-value="1">Flash ON</button><button data-cmd="CAM_LED" data-value="0">Flash OFF</button>
<button data-cmd="Shooting">Disparar</button></div>
<div><button data-cmd="Buzzer" data-value="1">Melodia 1</button><button data-cmd="Buzzer" data-value="2">Melodia 2</button>
<button data-cmd="Buzzer" data-value="3">Melodia 3</button><button data-cmd="Buzzer" data-value="4">Melodia 4</button>
<button data-cmd="Buzzer" data-value="0">Silencio</button></div>
<script>
const status = document.querySelector('#status');
const session = Math.random().toString(36).slice(2);
let queue = [], sending = false, pointer = null, controlling = false;
const url = (cmd, args={}) => '/control?' + new URLSearchParams({cmd, session, ...args});
async function request(cmd, args={}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 1200);
  try {
    const response = await fetch(url(cmd,args), {signal:controller.signal, cache:'no-store'});
    if (!response.ok) throw new Error(await response.text());
  } finally { clearTimeout(timeout); }
}
async function drain() {
  if (sending) return;
  sending = true;
  while(queue.length) {
    const [cmd,args] = queue.shift();
    try { await request(cmd,args); status.textContent = cmd === 'stopA' ? 'Detenido' : 'Orden enviada'; }
    catch(e) { status.textContent = 'Error: ' + e.message; controlling = false; queue = []; }
  }
  sending = false;
}
function send(cmd,args={}) { controlling = cmd !== 'stopA'; queue.push([cmd,args]); drain(); }
function stopAll() { pointer = null; queue = []; controlling = false; send('stopA'); }
document.querySelector('#stop').onclick = stopAll;
document.querySelectorAll('[data-dir]').forEach(button => {
  button.addEventListener('pointerdown', e => {
    if (pointer !== null || (e.pointerType === 'mouse' && e.button !== 0)) return;
    e.preventDefault(); pointer = e.pointerId; button.setPointerCapture(pointer);
    send('car',{direction:button.dataset.dir});
  });
  const release = e => {
    if (pointer !== e.pointerId) return;
    pointer = null; send('car',{direction:'stop'});
  };
  button.addEventListener('pointerup',release);
  button.addEventListener('pointercancel',release);
  button.addEventListener('lostpointercapture',release);
});
document.querySelectorAll('[data-cmd]').forEach(button => button.onclick = () =>
  send(button.dataset.cmd, button.dataset.value === undefined ? {} : {value:button.dataset.value}));
document.querySelector('#speed').onchange = e => send('speed',{value:e.target.value});
document.querySelector('#servo').onchange = e => send('servo',{angle:e.target.value});
const image = document.querySelector('#camera');
document.querySelector('#video').onclick = () => image.src = 'http://' + location.hostname + ':82/stream';
document.querySelector('#photo').onclick = () => image.src = 'http://' + location.hostname + '/capture?t=' + Date.now();
document.querySelector('#close').onclick = () => image.removeAttribute('src');
setInterval(() => {
  if (controlling && !document.hidden) request('ping').catch(() => { controlling=false; status.textContent='Enlace perdido'; });
}, 500);
window.addEventListener('blur',stopAll);
document.addEventListener('visibilitychange',() => { if(document.hidden) stopAll(); });
window.addEventListener('pagehide',() => {
  controlling=false;
  fetch(url('stopA'),{keepalive:true}).catch(() => {});
});
</script></html>
)HTML";
