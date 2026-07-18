import shutil, os

# Pre-scripts run with cwd = project dir; __file__ may be undefined under SCons.
here = os.getcwd()

shared_src = os.path.join(here, '..', 'shared', 'src')
gateway_src = os.path.join(here, 'src')

files = ['common_ota.cpp', 'common_console.cpp']
for f in files:
    src = os.path.join(shared_src, f)
    dst = os.path.join(gateway_src, f)
    if os.path.exists(src):
        shutil.copy2(src, dst)
