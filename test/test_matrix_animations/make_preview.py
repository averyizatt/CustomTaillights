"""Export a standalone browser preview from frames rendered by the C++ test binary."""
import json
from pathlib import Path
import subprocess
import sys

binary = Path(sys.argv[1] if len(sys.argv) > 1 else ".pio/test_matrix_animations.exe").resolve()
frames = subprocess.check_output([str(binary), "--preview"], text=True)
json.loads(frames)  # Catch an accidental diagnostic mixed into the data.
template = r'''<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Foxbody matrix animation preview</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#0e1014;color:#eef0f6;font:16px system-ui,sans-serif;padding:28px;max-width:1180px;margin:auto}
h1{font-size:28px;margin:0 0 10px}p{color:#aab2c1;line-height:1.5;max-width:850px}
.controls{display:flex;gap:14px;align-items:center;flex-wrap:wrap;margin:22px 0}button,select{background:#222835;border:1px solid #465069;color:white;border-radius:8px;padding:10px;font:inherit}
input{accent-color:#fb725e}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:18px}
article{background:#171b24;border:1px solid #303849;border-radius:14px;padding:17px}h2{font-size:19px;margin:0 0 7px}.tag{color:#ffaf90;font-size:12px;text-transform:uppercase;letter-spacing:.08em}
canvas{width:100%;height:auto;background:#080a0f;border-radius:8px;margin-top:14px}article p{font-size:13px;margin-bottom:0;min-height:40px}footer{font-size:12px;color:#939cab;margin-top:24px}
</style>
<h1>Foxbody · Matrix Collection</h1>
<p>Eight new patterns rendered from the firmware. Each lamp has two 21×5 strips and a 17×10 panel. This schematic matrix view shows mirrored driver/passenger lamps with a gap at the vehicle center.</p>
<div class="controls"><button id="play">Pause</button><label>Speed <select id="speed"><option value=".5">½×</option><option value="1" selected>1×</option><option value="2">2×</option></select></label><label>Preview exposure <input id="exposure" type="range" min="1" max="4" step=".25" value="2"></label></div>
<main></main><footer>Frames use the actual animation and serpentine-mapping code with an RGB host stub. Colors are schematic; lens optics, diffusion, thermal limiting, and the power budget affect physical output. Exposure changes this preview only. Edge Lock repeats here to demonstrate activation; on the car it settles to solid.</footer>
<script>
const data=__FRAMES__;
const info=[['Running · 4','Steady panel outlines with a slow perimeter highlight.'],['Running · 5','Three raked blades and fine strip rails with a soft passing sheen.'],['Brake · 6','Immediate bright fill; the perimeter locks inward to full intensity.'],['Turn · 6','A chevron fills from the vehicle center outward, holds, then blanks.'],['Turn · 7','Three defined bars latch outward in sequence, hold, then blank.'],['Show · 33','Exhaust rings glow across the panels with warm amber on the clear strip.'],['Show · 34','Perspective gates emerge through a field of converging guide rails.'],['Show · 35','Two diagonal ribbons alternate over and under at their crossings.']];
const cards=data.map((item,i)=>{const el=document.createElement('article');el.innerHTML='<div class="tag">'+info[i][0]+'</div><h2>'+item.name+'</h2><p>'+info[i][1]+'</p><canvas width="504" height="272" aria-label="'+item.name+' mirrored LED matrix preview"></canvas>';document.querySelector('main').append(el);return el.querySelector('canvas').getContext('2d')});
let playing=true,elapsed=0,last=performance.now();document.querySelector('#play').onclick=()=>{playing=!playing;document.querySelector('#play').textContent=playing?'Pause':'Play'};
function paint(ctx,hex){ctx.clearRect(0,0,504,272);const exposure=+document.querySelector('#exposure').value;for(let side=0;side<2;side++){let k=0;for(let seg=0;seg<3;seg++){const cols=seg===2?17:21,rows=seg===2?10:5,yBase=[16,78,140][seg];for(let r=0;r<rows;r++)for(let c=0;c<cols;c++){const value=hex.slice(k,k+6);k+=6;const rgb=[0,2,4].map(n=>Math.min(255,parseInt(value.slice(n,n+2),16)*exposure));const x=12+side*262+(seg===2?22:0)+(side?cols-1-c:c)*10.5;const y=yBase+r*11;ctx.fillStyle='rgb('+rgb.join(',')+')';ctx.beginPath();ctx.roundRect(x,y,7.8,7.8,2);ctx.fill();}}}ctx.fillStyle='#7f8ba1';ctx.font='10px system-ui';ctx.fillText('DRIVER',12,261);ctx.fillText('PASSENGER',274,261)}
function loop(now){if(playing)elapsed+=(now-last)*(+document.querySelector('#speed').value);last=now;data.forEach((item,i)=>paint(cards[i],item.frames[Math.floor((elapsed%item.period)/item.period*item.frames.length)]));requestAnimationFrame(loop)}requestAnimationFrame(loop);
</script></html>'''
output = Path(".pio/new-animations-preview.html")
output.write_text(template.replace("__FRAMES__", frames), encoding="utf-8")
print(output.resolve())
