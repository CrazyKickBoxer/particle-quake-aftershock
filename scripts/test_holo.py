import subprocess, pathlib, os, json, sys
root=pathlib.Path(__file__).resolve().parent.parent
user=root/'build/holo-test-user';(user/'id1').mkdir(parents=True,exist_ok=True)
args=[str(root/'bin/vkQuake.exe'),'-basedir',r'C:\Program Files (x86)\Steam\steamapps\common\Quake','-userdir',str(user),'-window','-width','960','-height','540','-renderer','particle','-worldmode','faithful','-physics','off','-density','fine','-condebug','-test-holo','-test-seed','1234','-sim-checksum-tick','200','-frames','240','-capture-sequence','holo','-capture-start-tick','10','-capture-ticks','30','+r_holo_physics','1','+as_structure','11','+as_gibs','0','+as_goo','0','+as_shake','0','+r_tasks','1','+vid_vsync','0','+host_maxfps','60','+host_framerate','0.016666667','+map','e1m1']
if '--menu' in sys.argv: args.insert(1,'-test-holo-menu')
if '--pause' in sys.argv: args.insert(1,'-test-holo-pause')
if '--cycle' in sys.argv: args.insert(1,'-test-holo-cycle')
if '--style' in sys.argv: args += ['+as_structure','0','+as_style',sys.argv[sys.argv.index('--style')+1]]
if '--event' in sys.argv: args[1:1]=['-test-holo-event',sys.argv[sys.argv.index('--event')+1]]
if '--gore-life' in sys.argv: args += ['+r_holo_phys_gore_life',sys.argv[sys.argv.index('--gore-life')+1]]
if '--gore' in sys.argv: args.insert(1,'-test-holo-gore')
if '--blast' in sys.argv: args.insert(1,'-test-holo-blast')
if '--off' in sys.argv: args[args.index('+r_holo_physics')+1]='0'
if '--off' in sys.argv: args[args.index('-capture-sequence')+1]='off'
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
p=subprocess.Popen(args,cwd=root,env=env,startupinfo=si,creationflags=subprocess.CREATE_NO_WINDOW)
try: code=p.wait(timeout=70)
except subprocess.TimeoutExpired: p.kill();p.wait();raise
log=(pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace')
(root/'build/holo-last-command.json').write_text(json.dumps(args,indent=2))
(root/('build/holo-stage1-off.log' if '--off' in sys.argv else 'build/holo-stage1-engine.log')).write_text(log)
print('Exit',code)
print('\n'.join(line for line in log.splitlines() if 'Holo' in line or 'Error' in line))
if code: raise RuntimeError('engine failed')
print('\n'.join(line for line in log.splitlines() if 'gameplay' in line))
