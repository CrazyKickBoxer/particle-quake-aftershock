"""Compare the optional Neon Prism look and its reflection toggle."""
import json, os, pathlib, subprocess
root=pathlib.Path(__file__).resolve().parent.parent
user=root/'build/neon-prism'
(user/'id1').mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
for name in ['classic','prism','prism-dry']:
    args=[str(root/'bin/vkQuake.exe'),'-basedir',r'C:\Program Files (x86)\Steam\steamapps\common\Quake',
          '-userdir',str(user),'-window','-width','1280','-height','720','-renderer','particle',
          '-physics','off','-density','fine','-condebug','-test-seed','1234','-frames','150',
          '-capture-sequence',name,'-capture-start-tick','120','-capture-ticks','200',
          '-benchmark-json',str(user/f'{name}.json'),'+r_holo_physics','0','+as_gibs','0',
          '+as_goo','0','+as_shake','0','+con_notifytime','0','+as_structure','11',
          '+as_layers','3','+as_fidelity','1','+vid_vsync','0','+host_maxfps','60',
          '+host_framerate','0.016666667','+neon_prism','0' if name=='classic' else '1',
          '+as_reflections','0' if name=='prism-dry' else '1','+map','e1m1']
    (user/f'{name}-command.json').write_text(json.dumps(args,indent=2))
    p=subprocess.Popen(args,cwd=root,env=env,startupinfo=si,creationflags=subprocess.CREATE_NO_WINDOW)
    try: code=p.wait(timeout=60)
    except subprocess.TimeoutExpired: p.kill();p.wait();raise
    log=(pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace')
    (user/f'{name}.log').write_text(log)
    assert code==0 and (user/'id1'/f'{name}-00120.png').exists(),(name,code)
    print(name,'passed',flush=True)

