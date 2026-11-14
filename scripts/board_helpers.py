from pathlib import Path
Import("env")
board = Path(env.subst("$PROJECT_SRC_DIR")).parent
for script in ['build_config.py']:
    env.SConscript(str(board / "scripts" / script), exports="env")
