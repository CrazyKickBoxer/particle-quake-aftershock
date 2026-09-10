import subprocess,pathlib,os,sys,json,re
root=pathlib.Path(__file__).resolve().parent.parent
cases=sys.argv[1:] or ['off','fine','nolights','nowake']
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
for case in cases:
 user=root/'build/holo-profile'/case;(user/'id1').mkdir(parents=True,exist_ok=True)
 actual=case.split('-')[0]
 budget={'play':16384,'showcase':131072}.get(actual,65536)
 a=[str(root/'bin/vkQuake.exe'),'-basedir',r'C:\Program Files (x86)\Steam\steamapps\common\Quake','-userdir',str(user),'-window','-width','1280','-height','720','-renderer','particle','-worldmode','faithful','-physics','off','-density','fine','-condebug','-timedemo-exit','-holo-profile',str(user/'timings.csv'),'+r_holo_physics','0' if actual=='off' else '1','+r_holo_phys_debug','0','+r_holo_phys_budget',str(budget),'+r_holo_phys_debris_light','0' if case=='nolights' else '1','+r_holo_phys_player_wake','0' if case=='nowake' else '.5','+as_structure','11','+as_layers','3','+as_fidelity','1','+as_reflections','1','+as_gibs','0','+as_goo','0','+as_shake','0','+host_maxfps','1000','+host_framerate','0','+vid_vsync','0','+timedemo','demo1']
 if 'verify' in case:
  a[1:1]=['-holo-verify','-test-seed','1234']
  a[a.index('+host_framerate')+1]='0.016666667'
 if actual in ['play','fine','showcase']: a[-2:-2]=['+r_holo_phys_gore_max','4096' if actual=='play' else '20000','+r_holo_phys_settle',{'play':'1','fine':'1.2','showcase':'1.8'}[actual],'+r_holo_phys_dust','0' if actual=='play' else '.8']
 (user/'command.json').write_text(json.dumps(a,indent=2))
 p=subprocess.Popen(a,env=env,startupinfo=si,creationflags=subprocess.CREATE_NO_WINDOW)
 try:code=p.wait(timeout=90)
 except subprocess.TimeoutExpired:p.kill();p.wait();raise
 log=(pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace');(user/'engine.log').write_text(log)
 lines=[l for l in log.splitlines() if 'frames' in l or 'Error' in l]
 print(case,code,lines,flush=True)
