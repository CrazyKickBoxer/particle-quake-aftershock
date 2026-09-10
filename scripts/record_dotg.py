"""Run the real timedemo, then film demo1 at normal speed and trim to 60 s."""
import json, os, shutil, subprocess, time
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
WORK=ROOT/'build/dotg-movie'; USER=WORK/'userdata'; FRAMES=USER/'dotg'
WORK.mkdir(parents=True,exist_ok=True);FRAMES.mkdir(parents=True,exist_ok=True)
FFMPEG=shutil.which('ffmpeg');FFPROBE=shutil.which('ffprobe')
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
common=[ROOT/'bin/vkQuake.exe','-basedir',r'C:\Program Files (x86)\Steam\steamapps\common\Quake',
        '-userdir',USER,'-game','dotg','-window','-width','1280','-height','720','-renderer','particle',
        '-worldmode','faithful','-physics','off','-density','fine','-condebug',
        '+cl_startdemos','0','+as_structure','11','+as_layers','3','+as_fidelity','1','+as_reflections','1',
        '+as_effects','1','+as_gibs','1','+as_goo','1','+as_nails','1','+as_particle_amount','2',
        '+neon_prism','1','+r_holo_physics','1','+r_holo_phys_budget','65536','+as_reduced_flashes','0','+as_shake','0','+vid_fsaa','0','+r_oit','1','+r_tasks','1',
        '+r_scale','1','+viewsize','100','+crosshair','0','+r_drawviewmodel','1','+con_notifytime','-1',
        '+scr_showfps','0','+vid_vsync','0','+bgmvolume','0','+volume','1','+snd_noextraupdate','1']
def launch(args,name):
    args=[str(a) for a in args];(WORK/f'{name}-command.json').write_text(json.dumps(args,indent=2))
    log=open(WORK/f'{name}-process.log','w')
    return subprocess.Popen(args,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,
                            startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW),log
def archive_log(name):
    shutil.copy2(Path(os.environ['APPDATA'])/'vkQuake/qconsole.log',WORK/f'{name}-engine.log')

print('Recording Dimension of the Gibbed: cinematic Necropolis',flush=True)
args=common+['-capture-demo','-capture-sequence','dotg-hd','-capture-ticks','1',
             '-capture-audio',WORK/'game.pcm','+host_maxfps','30','+host_framerate','0.033333333','+playdemo','e1m3c']
with open(WORK/'video-encode.log','w') as encode_log:
    encoder=subprocess.Popen([FFMPEG,'-y','-f','image2pipe','-vcodec','mjpeg','-framerate','30','-i','pipe:0',
                              '-an','-c:v','libx264','-preset','fast','-crf','18','-maxrate','32M','-bufsize','64M',
                              '-pix_fmt','yuv420p','-movflags','+faststart',str(WORK/'dotg-full.mp4')],
                             stdin=subprocess.PIPE,stdout=encode_log,stderr=subprocess.STDOUT,
                             creationflags=subprocess.CREATE_NO_WINDOW)
    game,log=launch(args,'capture');index=0;started=time.monotonic()
    try:
        while True:
            current=FRAMES/f'dotg-hd-{index:05d}.jpg';following=FRAMES/f'dotg-hd-{index+1:05d}.jpg'
            done=game.poll() is not None
            # The next file or process exit guarantees the preceding image is closed.
            if current.exists() and (following.exists() or done):
                encoder.stdin.write(current.read_bytes());current.unlink();index+=1
                if index%300==0:print(f'Captured and encoded {index/30:.0f} seconds',flush=True)
            elif done:break
            else:time.sleep(.01)
            if time.monotonic()-started>1200:raise TimeoutError('Demo capture timed out')
        code=game.wait();log.close();archive_log('capture')
        encoder.stdin.close();encoded=encoder.wait(timeout=180)
        if code or encoded:raise RuntimeError('Demo capture/encode failed')
    except BaseException:
        if game.poll() is None:game.kill();game.wait()
        encoder.kill();encoder.wait();raise

duration=index/30
if duration<60:raise RuntimeError(f'Demo capture unexpectedly short: {duration}')
pcm=WORK/'game.pcm';expected=index*1470*4
if abs(pcm.stat().st_size-expected)>1470*4:raise RuntimeError('Audio/video frame counts differ')
start=round((duration-60)/2*30)/30
print(f'Trimming {duration:.3f} seconds to the central 60 seconds, starting at {start:.3f}s',flush=True)
output=ROOT/'exports/ParticleQuake-Dimension-of-the-Gibbed-60s.mp4'
with open(WORK/'final-encode.log','w') as log:
    args=[FFMPEG,'-y','-ss',str(start),'-i',str(WORK/'dotg-full.mp4'),
          '-f','s16le','-ar','44100','-ac','2','-ss',str(start),'-i',str(pcm),
          '-t','60','-vf','setpts=PTS-STARTPTS,fade=t=in:d=0.2,fade=t=out:st=59.7:d=0.3',
          '-af','asetpts=PTS-STARTPTS,afade=t=in:d=0.1,afade=t=out:st=59.7:d=0.3,alimiter=limit=0.9:level=0',
          '-c:v','libx264','-preset','medium','-crf','19','-maxrate','32M','-bufsize','64M','-pix_fmt','yuv420p',
          '-r','30','-c:a','aac','-b:a','192k','-movflags','+faststart',str(output)]
    result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError('Final encode failed')
(WORK/'edit.json').write_text(json.dumps({'source':'Dimension of the Gibbed cinematic e1m3c.dem � Team SDA / Richard Morfans Skidmore','capture_frames':index,
    'capture_seconds':duration,'edit_start_seconds':start,'edit_duration_seconds':60,
    'capture_playback':'original camera, normal speed','audio':'original stereo game mix'},indent=2))
print(f'Finished: {output}',flush=True)

