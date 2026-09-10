"""Finish the DotG capture as a credited 60-second MP4 with stereo game audio."""
import pathlib, subprocess, shutil, json
ROOT=pathlib.Path(__file__).resolve().parent.parent
WORK=ROOT/'build/dotg-movie'
ffmpeg=shutil.which('ffmpeg');ffprobe=shutil.which('ffprobe')
edit=json.loads((WORK/'edit.json').read_text())
start=edit['edit_start_seconds']
output=ROOT/'exports/ParticleQuake-Dimension-of-the-Gibbed-60s.mp4'
# Remove the screenshot notification strip, retain source scale, and letterbox.
video=("setpts=PTS-STARTPTS,crop=1280:648:0:72,pad=1280:720:0:36,"
       "drawtext=fontfile=build/dotg-movie/SegoeUI.ttf:text='PARTICLE QUAKE  /  DIMENSION OF THE GIBBED':fontcolor=0x75e9ff:fontsize=16:x=24:y=10,"
       "drawtext=fontfile=build/dotg-movie/SegoeUI.ttf:text='NEON PRISM + HOLO PHYSICS':fontcolor=white:fontsize=12:x=24:y=695,"
       "drawtext=fontfile=build/dotg-movie/SegoeUI.ttf:text='Original demo - Team SDA / Morfans':fontcolor=white:fontsize=12:x=w-tw-24:y=695,"
       "fade=t=in:d=0.2,fade=t=out:st=59.6:d=0.4")
args=[ffmpeg,'-y','-ss',str(start),'-i',str(WORK/'dotg-full.mp4'),
      '-f','s16le','-ar','44100','-ac','2','-ss',str(start),'-i',str(WORK/'game.pcm'),'-t','60',
      '-vf',video,'-af','asetpts=PTS-STARTPTS,afade=t=in:d=0.1,afade=t=out:st=59.6:d=0.4,alimiter=limit=0.9:level=0',
      '-c:v','libx264','-preset','medium','-crf','19','-maxrate','16M','-bufsize','32M','-pix_fmt','yuv420p',
      '-r','30','-c:a','aac','-b:a','192k','-movflags','+faststart',
      '-metadata','title=Particle Quake - Dimension of the Gibbed',
      '-metadata','comment=Original demo: Team SDA / Richard Morfans Skidmore. https://speeddemosarchive.com/quake/projects/dotg/',str(output)]
with (WORK/'movie-finish.log').open('w') as log:
 subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True,creationflags=subprocess.CREATE_NO_WINDOW)
probe=json.loads(subprocess.check_output([ffprobe,'-v','error','-show_streams','-show_format','-of','json',str(output)]))
(WORK/'movie-probe.json').write_text(json.dumps(probe,indent=2))
assert abs(float(probe['format']['duration'])-60)<0.1
assert any(s['codec_type']=='audio' and s['channels']==2 for s in probe['streams'])
for t in [5,20,40,55]:
 subprocess.run([ffmpeg,'-y','-ss',str(t),'-i',str(output),'-frames:v','1',str(WORK/f'final-{t}.jpg')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True,creationflags=subprocess.CREATE_NO_WINDOW)
print(output,output.stat().st_size,flush=True)

