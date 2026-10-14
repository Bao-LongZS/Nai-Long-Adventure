"""Validate current saved A/B spherical eyes against their pinned fixed body."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).resolve().parent/'validate_spherical_eyes.py'), run_name='__main__')
