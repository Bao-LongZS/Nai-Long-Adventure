"""Render the candidate and independently validate its saved geometry."""
import runpy,sys
from pathlib import Path
OUT=Path(__file__).resolve().parent
runpy.run_path(str(OUT/'preview_lip_eye.py'),run_name='__main__')
sys.argv.append('--draft')
runpy.run_path(str(OUT/'validate_lip_eye_refinement.py'),run_name='__main__')
