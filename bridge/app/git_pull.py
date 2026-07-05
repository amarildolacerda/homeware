from __future__ import annotations
import os
import subprocess
import sys
import tempfile

INSTALL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_URL = "https://github.com/amarildolacerda/homeware.git"
BRANCH = "main"

async def git_pull() -> dict:
    tmp = None
    try:
        tmp = tempfile.mkdtemp()
        subprocess.run(
            ["git", "clone", "--depth", "1", "--branch", BRANCH, REPO_URL, tmp],
            capture_output=True, text=True, timeout=30, check=True
        )
        src = os.path.join(tmp, "bridge")
        if not os.path.isdir(src):
            return {"success": False, "updated": False, "message": "pasta bridge/ nao encontrada no repo"}

        result = subprocess.run(
            ["rsync", "-av", "--delete",
             "--exclude=venv/", "--exclude=__pycache__/", "--exclude=*.pyc",
             "--exclude=bridge.log",
             f"{src}/", f"{INSTALL_DIR}/"],
            capture_output=True, text=True, timeout=30
        )
        updated = result.returncode == 0
        return {"success": True, "updated": updated, "message": "Atualizado com sucesso" if updated else "Ja esta atualizado"}
    except subprocess.TimeoutExpired:
        return {"success": False, "updated": False, "message": "Timeout"}
    except subprocess.CalledProcessError as e:
        return {"success": False, "updated": False, "message": e.stderr.strip()}
    except Exception as e:
        return {"success": False, "updated": False, "message": str(e)}
    finally:
        if tmp and os.path.isdir(tmp):
            subprocess.run(["rm", "-rf", tmp], capture_output=True)
