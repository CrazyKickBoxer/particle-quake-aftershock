"""Render six deterministic chapters, record engine PCM, and encode a 60s reel."""
import argparse, json, os, shutil, subprocess, sys, time
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
WORK=ROOT/'build'/'showcase'
USER=WORK/'userdata'
PYTHON=sys.executable
FFMPEG=shutil.which('ffmpeg')
SCENES=[('NEON CATHEDRAL','Layered light. Living architecture.','e1m2'),
        ('NAIL RICOCHETS','Glowing streaks / pressure ripples / wall chips','e1m1'),
        ('BREAK THE WORLD','Rocket impacts / fracture heat / physical rubble','e1m1'),
        ('AFTER THE IMPACT','Directional gibs / floor slides / surface splashes','aftershock_arena'),
        ('NEON HUNTERS','Luminous silhouettes / layered particles / reflections','e1m2'),
        ('AFTERSHOCK','A world made of particles.','e1m1')]

def run(args,log,cwd=WORK):
    with open(log,'w',encoding='utf8') as out:
        p=subprocess.run([str(a) for a in args],cwd=cwd,stdout=out,stderr=subprocess.STDOUT)
    if p.returncode: raise RuntimeError(f'Command failed: {log}')

def capture(scene,preview=False):
    title,subtitle,mapname=SCENES[scene-1]
    folder=WORK/f'scene-{scene:02d}';folder.mkdir(exist_ok=True)
    name=f'showcase-{scene:02d}'+('-preview' if preview else '')
    args=[ROOT/'bin/vkQuake.exe','-basedir',r'C:\Program Files (x86)\Steam\steamapps\common\Quake',
          '-userdir',USER,'-window','-width','1920','-height','1080','-renderer','particle',
          '-worldmode','destruction' if scene in (2,3,6) else 'faithful',
          '-physics','physx-cpu' if scene in (2,3,6) else 'off','-density','fine','-condebug',
          '-showcase',str(scene),'-test-panel','9','-test-seed','1234',
          '-capture-sequence',name,'-capture-start-tick','31','-capture-ticks','30' if preview else '1',
          '-capture-audio',folder/('preview.pcm' if preview else 'audio.pcm'),'-frames','125' if preview else '335',
          '+as_structure','11','+as_layers','3','+as_fidelity','1','+as_reflections','1',
          '+as_effects','1','+as_gibs','0' if scene in (2,3,6) else '1','+as_goo','0' if scene in (2,3,6) else '1','+as_nails','1','+as_particle_amount','3',
          '+as_reduced_flashes','0','+as_shake','0','+vid_fsaa','0','+r_oit','1','+r_tasks','1',
          '+fov','65' if scene==5 else '90','+r_scale','1','+viewsize','120','+crosshair','0','+r_drawviewmodel','0','+con_notifytime','-1',
          '+scr_showfps','0','+vid_vsync','0','+host_maxfps','30','+host_framerate','0.033333333',
          '+bgmvolume','0','+volume','1','+snd_noextraupdate','1','+map',mapname]
    args=[str(a) for a in args]
    (folder/'command.json').write_text(json.dumps(args,indent=2))
    env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
    print(f'Rendering scene {scene}: {title}',flush=True)
    with open(folder/'process.log','w') as log:
        proc=subprocess.Popen(args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,env=env,creationflags=subprocess.CREATE_NO_WINDOW)
        try:code=proc.wait(timeout=420)
        except subprocess.TimeoutExpired:proc.kill();proc.wait();raise
    console=Path(os.environ['APPDATA'])/'vkQuake/qconsole.log'
    shutil.copy2(console,folder/'engine.log')
    log=(folder/'engine.log').read_text(errors='replace')
    if code or any(x in log for x in ['Host_Error','Sys_Error','Unknown command']):raise RuntimeError(f'Scene {scene} failed: {folder}/engine.log')
    frames=sorted((USER/'id1').glob(name+'-*.png'))
    if preview:
        for file in frames: shutil.move(str(file),folder/file.name)
        print(f'Preview {scene}: {len(frames)} frames',flush=True);return
    expected=[USER/'id1'/f'{name}-{tick:05d}.png' for tick in range(31,331)]
    if not all(f.exists() for f in expected):raise RuntimeError(f'Missing scene {scene} frames')
    pcm=folder/'audio.pcm'
    if not pcm.exists() or pcm.stat().st_size!=44100*10*4:raise RuntimeError(f'Audio duration mismatch: {pcm.stat().st_size if pcm.exists() else 0}')
    print(f'Encoding scene {scene}: 300 frames + 10 seconds stereo game audio',flush=True)
    (WORK/'title.txt').write_text(f'{scene:02d} / {title}')
    (WORK/'subtitle.txt').write_text(subtitle)
    filters="fade=t=in:st=0:d=0.12,fade=t=out:st=9.85:d=0.15"
    filters+=",drawbox=x=68:y=76:w=5:h=69:color=0x23ddff@0.9:t=fill:enable='between(t,0.4,3.7)'"
    filters+=",drawtext=fontfile=title.ttf:textfile=title.txt:fontsize=32:fontcolor=white:x=92:y=75:shadowcolor=black@0.8:shadowx=2:shadowy=2:enable='between(t,0.4,3.7)'"
    filters+=",drawtext=fontfile=title.ttf:textfile=subtitle.txt:fontsize=20:fontcolor=0xb5d9df:x=94:y=122:shadowcolor=black@0.8:shadowx=2:shadowy=2:enable='between(t,0.4,3.7)'"
    if scene==1:
        filters+=",drawtext=fontfile=title.ttf:text='PARTICLE QUAKE':fontsize=76:fontcolor=white:x=(w-tw)/2:y=h*0.72:shadowcolor=black@0.8:shadowx=3:shadowy=3:enable='between(t,0.8,3.7)'"
    if scene==6:
        filters+=",drawtext=fontfile=title.ttf:text='AFTERSHOCK':fontsize=80:fontcolor=white:x=(w-tw)/2:y=h*0.74:shadowcolor=black@0.8:shadowx=3:shadowy=3:enable='between(t,7.5,9.85)'"
    run([FFMPEG,'-y','-framerate','30','-start_number','31','-i',USER/'id1'/f'{name}-%05d.png',
         '-f','s16le','-ar','44100','-ac','2','-i',pcm,'-vf',filters,
         '-af','afade=t=in:d=0.06,afade=t=out:st=9.9:d=0.1','-t','10',
         '-c:v','libx264','-preset','fast','-crf','15','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k',
         '-movflags','+faststart',folder/'chapter.mp4'],folder/'encode.log')
    # Only delete this run's enumerated capture frames after successful encoding.
    for f in frames:
        assert f.resolve().parent==(USER/'id1').resolve() and f.name.startswith(name+'-')
        f.unlink()
    print(f'Finished scene {scene}',flush=True)

def assemble():
    import numpy as np
    rate=44100;t=np.arange(60*rate,dtype=np.float64)/rate
    # Original, restrained electronic bed under the captured game sound.
    rng=np.random.default_rng(8128)
    pad=np.zeros_like(t)
    for frequency,phase in [(55,0),(110,.2),(130.8128,.5),(164.8138,.9)]:
        pad+=np.sin(2*np.pi*frequency*t+.18*np.sin(t*.35+phase))*.007
    beat=np.mod(t,60/96)
    kick=np.sin(2*np.pi*(45*beat+3*(1-np.exp(-beat*22))))*np.exp(-beat*16)*.045
    pulse=np.sin(2*np.pi*220*t)*np.exp(-np.mod(t,.3125)*26)*.006
    fade=np.minimum(np.minimum(t/2,(60-t)/2),1).clip(0,1)
    left=(pad+kick+pulse)*fade
    right=(pad+kick+pulse*np.cos(t*.8)) * fade
    stereo=np.stack([left,right],axis=1)
    import wave
    with wave.open(str(WORK/'score.wav'),'wb') as w:
        w.setnchannels(2);w.setsampwidth(2);w.setframerate(rate);w.writeframes((np.clip(stereo,-1,1)*32767).astype('<i2').tobytes())
    listing=''.join(f"file 'scene-{i:02d}/chapter.mp4'\n" for i in range(1,7))
    (WORK/'chapters.txt').write_text(listing)
    output=ROOT/'exports'/'ParticleQuake-Aftershock-60s.mp4';output.parent.mkdir(exist_ok=True)
    run([FFMPEG,'-y','-f','concat','-safe','0','-i','chapters.txt','-i','score.wav',
         '-filter_complex','[0:a]volume=1.35[game];[game][1:a]amix=inputs=2:duration=first:normalize=0,alimiter=limit=0.94[a]',
         '-map','0:v','-map','[a]','-vf','drawbox=x=0:y=0:w=iw:h=64:color=black:t=fill,drawbox=x=0:y=ih-64:w=iw:h=64:color=black:t=fill,setpts=PTS-STARTPTS','-r','30','-c:v','libx264','-preset','medium','-crf','19','-maxrate','48M','-bufsize','96M','-pix_fmt','yuv420p','-c:a','aac','-b:a','256k','-t','60','-movflags','+faststart',output],WORK/'final-encode.log')
    print(f'Finished: {output}',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--scene',type=int,choices=range(1,7));parser.add_argument('--preview',action='store_true');parser.add_argument('--assemble',action='store_true');a=parser.parse_args()
    WORK.mkdir(parents=True,exist_ok=True);(USER/'id1/maps').mkdir(parents=True,exist_ok=True)
    shutil.copy2(ROOT/'build/userdata/id1/maps/aftershock_arena.bsp',USER/'id1/maps')
    shutil.copy2(Path(os.environ['WINDIR'])/'Fonts/seguisb.ttf',WORK/'title.ttf')
    if a.assemble:assemble()
    else:
        for scene in ([a.scene] if a.scene else range(1,7)):capture(scene,a.preview)
        if not a.preview and not a.scene:assemble()
