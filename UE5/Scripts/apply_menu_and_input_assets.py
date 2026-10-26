"""Import the level card images and update the P1 Enhanced Input mapping."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = (
    ROOT / "UE5/NaiwaP0/Tools/create_menu_assets.py",
    ROOT / "UE5/Scripts/configure_gait_input.py",
)

for script_path in SCRIPTS:
    scope = {"__file__": str(script_path), "__name__": "__main__"}
    source = script_path.read_text(encoding="utf-8")
    exec(compile(source, str(script_path), "exec"), scope)
