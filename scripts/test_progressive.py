import pathlib,subprocess,os,re,sys
root=pathlib.Path(__file__).resolve().parent.parent
u=root/'build/progressive-test';(u/'id1/maps').mkdir(parents=True,exist_ok=True)
subprocess.run([str(root/'build/aftershock/Release/aftershock_arena_generator.exe'),str(u/'id1/maps/progressive_arena.bsp'),'--progressive'],check=True)
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
args=[str(root/'engine/Windows/VisualStudio/Build-vkQuake/x64/Release/vkQuake.exe'),'-basedir',r'C:\Program Files (x86)\Steam\steamapps\common\Quake','-userdir',str(u),'-window','-width','960','-height','540','-renderer','particle','-worldmode','destruction','-physics','physx-cpu','-density','fine','-condebug','-test-progressive','-test-view','-test-panel',sys.argv[1] if len(sys.argv)>1 else '0','-frames','480','-capture-sequence','progressive-'+(sys.argv[1] if len(sys.argv)>1 else '0'),'-capture-start-tick','60','-capture-ticks','180','+as_progressive','1','+as_damage','6','+as_rocket_hits','4','+as_chip_radius','72','+neon_prism','1','+r_holo_physics','1','+as_shake','0','+vid_vsync','0','+host_maxfps','60','+host_framerate','0.016666667','+map','progressive_arena','+as_inspect']
p=subprocess.Popen(args,cwd=root,env=env,startupinfo=si,creationflags=subprocess.CREATE_NO_WINDOW)
try:code=p.wait(timeout=90)
except subprocess.TimeoutExpired:p.kill();p.wait();raise
log=(pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace');(u/'engine.log').write_text(log)
print('\n'.join(l for l in log.splitlines() if any(x in l for x in ['Progressive hit','closed wall',' panel ','Error','errors='])))
assert code==0
hits=[int(x) for x in re.findall(r'Progressive hit: tick=\d+ detached=(\d+)',log)]
assert len(hits)>=5 and hits[-1]>hits[0],hits
assert 'errors=0' in log
