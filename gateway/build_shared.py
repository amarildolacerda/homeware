import shutil, os

shared_src = os.path.join(os.path.dirname(__file__), '..', 'shared', 'src')
gateway_src = os.path.join(os.path.dirname(__file__), 'src')

files = ['common_ota.cpp', 'common_console.cpp']
for f in files:
    src = os.path.join(shared_src, f)
    dst = os.path.join(gateway_src, f)
    if os.path.exists(src):
        shutil.copy2(src, dst)
