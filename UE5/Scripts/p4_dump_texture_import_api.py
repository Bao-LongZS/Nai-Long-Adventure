"""Write Unreal Python texture enum names for import-pipeline diagnostics."""
import json
from pathlib import Path

import unreal

root = Path(__file__).resolve().parents[2]
report = {
    "engine": unreal.SystemLibrary.get_engine_version(),
    "texture_compression_settings": [
        name for name in dir(unreal.TextureCompressionSettings) if name.startswith("TC_")
    ],
    "texture_mip_gen_settings": [
        name for name in dir(unreal.TextureMipGenSettings) if name.startswith("TMGS_")
    ],
}
output = root / "UE5/Records/p4_alpha/evidence/A1/P4_TextureImportApi_20261030.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
unreal.log("NAIWA_P4_TEXTURE_IMPORT_API " + json.dumps(report))
