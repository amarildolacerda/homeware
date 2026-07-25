import os
from SCons.Script import Import

Import("env")

project_dir = env.get("PROJECT_DIR")
shared_dir = os.path.normpath(os.path.join(project_dir, "..", "..", "shared"))

# Add shared include path
env.Append(CPPPATH=[shared_dir])

# Add shared source files to build (PRE script runs before source discovery)
for f in os.listdir(shared_dir):
    if f.endswith(".cpp"):
        env.Append(PIOBUILDFILES=[env.File(os.path.join(shared_dir, f))])

print(f"build_shared.py: added {sum(1 for f in os.listdir(shared_dir) if f.endswith('.cpp'))} shared sources")
