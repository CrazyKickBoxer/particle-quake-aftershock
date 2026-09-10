"""Renderer smoke tests and retained screenshots for every material style."""
import json, os, pathlib, re, subprocess, sys

root = pathlib.Path(__file__).resolve().parent.parent
output = root / 'build/holo-matrix'
output.mkdir(exist_ok=True)
base = [str(root/'bin/vkQuake.exe'), '-basedir', r'C:\Program Files (x86)\Steam\steamapps\common\Quake',
        '-userdir', str(root/'build/holo-test-user'), '-window', '-width', '960', '-height', '540',
        '-renderer', 'particle', '-worldmode', 'faithful', '-physics', 'off', '-density', 'fine',
        '-condebug', '-test-holo', '-test-seed', '1234', '-frames', '100',
        '-capture-sequence', 'matrix', '-capture-start-tick', '60', '-capture-ticks', '200',
        '+r_holo_physics', '1', '+r_holo_phys_budget', '65536', '+r_holo_phys_debug', '1',
        '+r_holo_phys_gore_life', '45', '+as_structure', '11', '+as_gibs', '0', '+as_goo', '0',
        '+as_shake', '0', '+host_maxfps', '60', '+host_framerate', '0.016666667', '+map', 'e1m1']
si = subprocess.STARTUPINFO()
si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
si.wShowWindow = 0
env = os.environ.copy()
env['SDL_AUDIODRIVER'] = 'dummy'
cases = [(f'style-{i:02d}', ['+as_structure', '0', '+as_style', str(i)]) for i in range(14)]
cases += [(f'event-{i}', ['-test-holo-event', str(i)]) for i in [1, 3, 4, 5, 6]]
cases += [('menu', ['-test-holo-menu'])]
if len(sys.argv)>1: cases=[case for case in cases if case[0] in sys.argv[1:]]
results = []
for name, extra in cases:
    args = base.copy()
    args[args.index('-capture-sequence')+1] = name
    if extra[0].startswith('-'):
        args[1:1] = extra
    else:
        args += extra
    process = subprocess.Popen(args, cwd=root, env=env, startupinfo=si,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        code = process.wait(timeout=60)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        raise
    log = (pathlib.Path(os.environ['APPDATA'])/'vkQuake/qconsole.log').read_text(errors='replace')
    (output/f'{name}.log').write_text(log)
    source = root/'build/holo-test-user/id1'/('holo-menu.png' if name=='menu' else f'{name}-00060.png')
    if source.exists():
        (output/f'{name}.png').write_bytes(source.read_bytes())
    counts = [int(x) for x in re.findall(r'Holo GPU: active=(\d+)', log)]
    assert code == 0 and counts and max(counts) > 0, (name, code, counts)
    assert max(counts) <= 65536, (name, counts)
    assert source.exists(), name
    results.append({'case': name, 'peak_observed': max(counts), 'exit': code})
    print(name, max(counts), 'passed', flush=True)
(output/'results.json').write_text(json.dumps(results, indent=2))
