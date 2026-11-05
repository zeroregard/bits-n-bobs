"""Compile scripts/*.psc (CLK_QuestScript, CLK_Native) with Caprica under Proton 11 wine; own stubs in stubs/.

    python3 build.py OUTDIR

Imports SKSE's script sources and Data/Source/Scripts; a type neither provides (the vanilla sources are not installed)
gets a stub `Scriptname X extends Form Hidden` and the script is compiled again."""
import os, re, shutil, subprocess, sys, tempfile

HOME = os.path.expanduser('~')
GAME = f'{HOME}/.local/share/Steam/steamapps/common/Skyrim Special Edition'
WINE = f'{HOME}/.local/share/Steam/steamapps/common/Proton 11.0/files/bin/wine'
CAPRICA = f'{HOME}/opt/caprica/Caprica.exe'
SKSE_SRC = f'{HOME}/Documents/Skyrim Mods/24/skse64_2_02_06/Data/Scripts/Source'
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OWN_STUBS = os.path.join(HERE, 'stubs')
FLAGS = f'{HOME}/src/bits-n-bobs/soes-outfit-cycle/stubs/TESV_Papyrus_Flags.flg'

out = os.path.abspath(sys.argv[1]); os.makedirs(out, exist_ok=True)
work = tempfile.mkdtemp(); stubs = f'{work}/stubs'; os.makedirs(stubs)
shutil.copy(FLAGS, work)
env = dict(os.environ, WINEPREFIX=f'{work}/pfx', WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=')
for psc in os.listdir(f'{HERE}/scripts'):   # all of them first: one script uses another's functions
    shutil.copy(f'{HERE}/scripts/{psc}', work)   # Caprica resolves a script's namespace from its working folder
for psc in sorted(os.listdir(f'{HERE}/scripts')):
    for _ in range(40):
        r = subprocess.run([WINE, CAPRICA, '--game', 'skyrim', '--import', f'Z:{OWN_STUBS};Z:{stubs};Z:{SKSE_SRC};Z:{GAME}/Data/Source/Scripts',
                            '--flags', 'TESV_Papyrus_Flags.flg', '--output', f'Z:{out}', psc],
                           cwd=work, env=env, capture_output=True, text=True)
        missing = re.search(r"resolve type '([A-Za-z_0-9]+)'", r.stdout + r.stderr)
        if missing:
            open(f'{stubs}/{missing.group(1)}.psc', 'w').write(f'Scriptname {missing.group(1)} extends Form Hidden\n')
            continue
        if not os.path.exists(f'{out}/{psc[:-4]}.pex') or 'Fatal' in r.stdout + r.stderr:
            sys.exit(f'{psc}:\n{(r.stdout + r.stderr)[-2000:]}')
        print('compiled', psc); break
shutil.rmtree(work)
