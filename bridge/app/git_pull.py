from __future__ import annotations
import os
import subprocess

_SCRIPT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

GIT_REMOTE = "https://github.com/amarildolacerda/homeware.git"
GIT_BRANCH = "dev"

async def git_pull() -> dict:
    try:
        branch = GIT_BRANCH
        remote = GIT_REMOTE
        subprocess.run(
            ["git", "fetch", remote, branch],
            cwd=_SCRIPT_DIR, capture_output=True, text=True, timeout=15
        )
        result = subprocess.run(
            ["git", "merge", f"{remote}/{branch}", "--ff-only"],
            cwd=_SCRIPT_DIR, capture_output=True, text=True, timeout=15
        )
        success = result.returncode == 0
        updated = "Already up to date" not in result.stdout and "Updating" in result.stdout
        message = result.stdout.strip() or result.stderr.strip()
        return {"success": success, "updated": updated, "message": message}
    except Exception as e:
        return {"success": False, "updated": False, "message": str(e)}
