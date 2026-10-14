"""Validate the current delivered eye, mouth and body revision."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).resolve().parent/'validate_eye_muzzle_delivery.py'),run_name='__main__')
