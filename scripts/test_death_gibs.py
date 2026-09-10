import pathlib,subprocess,os,re,json
root=pathlib.Path(__file__).resolve().parent.parent
w=root/'build/death-gib-test';w.mkdir(exist_ok=True)
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
for mode,holo,gibs in [('holo',1,1),('legacy',0,1),('disabled',1,0)]:
 u=w/mode;u.mkdir(exist_ok=True)
 args=[str(root/'bin/vkQuake.exe'),'-basedir',r'C:\Program Files (x86)\Steam\steamapps\common\Quake','-userdir',str(u),'-window','-width','960','-height','540','-renderer','particle','-physics','off','-density','play','-condebug','-test-death-gibs','-timedemo-exit','+r_holo_physics',str(holo),'+as_gibs',str(gibs),'+as_goo','1','+as_effects','1','+as_structure','11','+as_layers','2','+as_fidelity','1','+as_reflections','1','+vid_vsync','0','+host_maxfps','1000','+timedemo','demo1']
 p=subprocess.Popen(args,cwd=root,env=env,startupinfo=si,creationflags=subprocess.CREATE_NO_WINDOW)
 try:code=p.wait(timeout=100)
 except subprocess.TimeoutExpired:p.kill();p.wait();raise
 log=(pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace');(w/f'{mode}.log').write_text(log)
 bursts=re.findall(r'NPC death gib burst: entity=(\d+) count=(\d+)',log)
 print(mode,'exit',code,'bursts',bursts,flush=True)
 assert code==0
 assert bool(bursts)==bool(gibs)
 assert len(set(e for e,c in bursts))==len(bursts),'Repeated burst on same corpse'
