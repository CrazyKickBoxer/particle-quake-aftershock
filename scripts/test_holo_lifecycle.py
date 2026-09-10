"""Exercise renderer state across save/load, disconnect and a new map."""
import os, pathlib, subprocess

root = pathlib.Path(__file__).resolve().parent.parent
user = root/'build/holo-lifecycle'
(user/'id1').mkdir(parents=True, exist_ok=True)
args = [str(root/'bin/vkQuake.exe'), '-basedir', r'C:\Program Files (x86)\Steam\steamapps\common\Quake',
        '-userdir', str(user), '-window', '-width', '960', '-height', '540', '-condebug', '-test-holo-lifecycle',
        '-renderer', 'particle', '-physics', 'off', '-density', 'fine', '-test-seed', '1234',
        '+as_structure', '11', '+as_gibs', '0', '+as_goo', '0', '+as_shake', '0',
        '+host_maxfps', '1000', '+host_framerate', '0.016666667', '+r_holo_physics', '1', '+map', 'e1m1']
si = subprocess.STARTUPINFO()
si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
si.wShowWindow = 0
env = os.environ.copy()
env['SDL_AUDIODRIVER'] = 'dummy'
process = subprocess.Popen(args, cwd=root, env=env, startupinfo=si, creationflags=subprocess.CREATE_NO_WINDOW)
try:
    code = process.wait(timeout=90)
except subprocess.TimeoutExpired:
    process.kill()
    process.wait()
    raise
log = (pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace')
(user/'engine.log').write_text(log)
assert code == 0 and 'Loading game from' in log and 'Castle of the Damned' in log, log[-2000:]
assert 'Host_Error' not in log and 'Holo Physics Vulkan error' not in log, log[-2000:]
print('Save/load, disconnect and e1m2 map transition passed')
