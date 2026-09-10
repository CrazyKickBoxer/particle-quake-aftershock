import ctypes, pathlib, subprocess, os, json
root=pathlib.Path(__file__).resolve().parent.parent
command=(root/'build/aftershock/Release/launcher-selftest.txt').read_text(encoding='utf-8').split('\n',1)[1]
shell=ctypes.windll.shell32
shell.CommandLineToArgvW.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)]
shell.CommandLineToArgvW.restype=ctypes.POINTER(ctypes.c_wchar_p)
n=ctypes.c_int();p=shell.CommandLineToArgvW(command,ctypes.byref(n));args=[p[i] for i in range(n.value)];ctypes.windll.kernel32.LocalFree(p)
args[0]=str(root/'bin/vkQuake.exe')
user=root/'build/launcher-smoke';user.mkdir(exist_ok=True)
args[args.index('-userdir')+1]=str(user)
# Keep the generated settings; limit this verification run to a few seconds.
args+=['-frames','90','-condebug','-benchmark-json',str(user/'result.json'),'+vid_vsync','0','+host_maxfps','60','+host_framerate','0.016666667']
assert len(args)<256
(root/'build/launcher-smoke/command.json').write_text(json.dumps(args,indent=2))
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
p=subprocess.Popen(args,cwd=root,env=env,startupinfo=si,creationflags=subprocess.CREATE_NO_WINDOW)
try: code=p.wait(timeout=60)
except subprocess.TimeoutExpired: p.kill();p.wait();raise
log=(pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace')
(user/'engine.log').write_text(log)
assert code==0,(code,log[-2000:])
assert 'Unknown command' not in log,log
print('Engine launch PASS:',len(args),'arguments, exit',code)
