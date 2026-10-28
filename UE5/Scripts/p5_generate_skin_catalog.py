"""Generate the runtime P5/P6 skin catalog C++ from its versioned JSON source."""
from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "UE5/SourceAssets/P5/Manifests/P5_SharedSkinCatalog_v01.json"
TARGET = ROOT / "UE5/NaiwaP0/Source/NaiwaRuntime/P5/Data/NaiwaP5SkinCatalog.cpp"
HEADER = ROOT / "UE5/NaiwaP0/Source/NaiwaRuntime/P5/Data/NaiwaP5SkinCatalog.h"


def cpp_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'TEXT("{escaped}")'


def cpp_float(value: float | int) -> str:
    return f"{float(value):.4f}f"


def generate() -> str:
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    if data.get("schema") != "naiwa.p5-p6.shared-skin-catalog.v1":
        raise ValueError("Unexpected shared skin catalog schema")
    version_match = re.search(r"CatalogVersion\s*=\s*(\d+)", HEADER.read_text(encoding="utf-8"))
    if not version_match or int(version_match.group(1)) != int(data["catalog_version"]):
        raise ValueError("Update CatalogVersion in NaiwaP5SkinCatalog.h with the JSON source")
    seen: set[str] = set()
    rows: list[str] = []
    for skin in data["skins"]:
        skin_id = skin["skin_id"]
        if skin_id in seen:
            raise ValueError(f"Duplicate SkinId: {skin_id}")
        seen.add(skin_id)
        ability_id = f'FName({cpp_string(skin["ability_id"])})' if skin["ability_id"] else "NAME_None"
        values = [
            f'FName({cpp_string(skin_id)})',
            ability_id,
            str(int(skin["ticket_price"])),
            cpp_float(skin["landing_tolerance_delta_degrees"]),
            cpp_float(skin["landing_speed_multiplier"]),
            cpp_float(skin["landing_speed_duration_seconds"]),
            cpp_float(skin["obstacle_warning_distance_meters"]),
            cpp_float(skin["avalanche_pushback_meters"]),
            cpp_float(skin["avalanche_pushback_cooldown_seconds"]),
            "true" if skin["ignores_tree_hit_penalty"] else "false",
            "true" if skin["ignores_tree_slowdown"] else "false",
            cpp_string(skin["display_name"]),
            cpp_string(skin["ability_description"]),
        ]
        rows.append("\t\t{ " + ", ".join(values) + " }")
    expected = {
        "NaiwaSkin_Base", "NaiwaSkin_NaiYu", "NaiwaSkin_NaiCha",
        "NaiwaSkin_NaiHao", "NaiwaSkin_NaiSha",
    }
    if seen != expected:
        raise ValueError("The shared catalog must contain the base skin and four ability skins")
    if sum(int(skin["ticket_price"]) for skin in data["skins"]) != 320:
        raise ValueError("P5/P6 shared skin prices must total 320 tickets")
    row_text = ",\n".join(rows)
    return (
        "#include \"NaiwaP5SkinCatalog.h\"\n\n"
        "const TArray<FNaiwaP5SkinRow>& FNaiwaP5SkinCatalog::GetRows()\n{\n"
        "\tstatic const TArray<FNaiwaP5SkinRow> Rows = {\n"
        + row_text
        + "\n\t};\n\treturn Rows;\n}\n"
    )


if __name__ == "__main__":
    TARGET.write_text(generate(), encoding="utf-8")
    print(f"Generated shared P5/P6 skin catalog: {TARGET}")
