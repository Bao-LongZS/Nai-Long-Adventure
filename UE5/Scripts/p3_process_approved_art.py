"""Slice user-approved P3 M2 art atlases into clean transparent PNG assets."""
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ATLAS_DIR = ROOT / "UE5/Records/p3_m2/art_preview"
PORTRAIT_DIR = ROOT / "UE5/SourceAssets/P3/Portraits/Processed"
SPRITE_DIR = ROOT / "UE5/SourceAssets/P3/Sprites"
PADDING = 20


def extract_cell(source_name: str, columns: int, rows: int, column: int, row: int) -> Image.Image:
    source = Image.open(ATLAS_DIR / source_name).convert("RGBA")
    left = round(column * source.width / columns)
    right = round((column + 1) * source.width / columns)
    top = round(row * source.height / rows)
    bottom = round((row + 1) * source.height / rows)
    cell = source.crop((left, top, right, bottom))

    alpha = cell.getchannel("A")
    alpha = alpha.point(lambda value: 0 if value < 3 else value)
    cell.putalpha(alpha)
    crop_mask = alpha.point(lambda value: 255 if value >= 8 else 0)
    bounds = crop_mask.getbbox()
    if not bounds:
        raise RuntimeError(f"No visible art in {source_name} cell ({column}, {row})")
    art = cell.crop(bounds)
    padded = Image.new("RGBA", (art.width + PADDING * 2, art.height + PADDING * 2), (0, 0, 0, 0))
    padded.alpha_composite(art, (PADDING, PADDING))
    return padded


def save_asset(directory: Path, name: str, image: Image.Image) -> dict:
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / name
    image.save(path, format="PNG", optimize=True)
    return {"asset": str(path.relative_to(ROOT)), "size": [image.width, image.height], "bytes": path.stat().st_size}


def main() -> None:
    outputs = []
    portrait_cells = [
        ("SPR_Player_Neutral_A.png", "01_player_naicha_feedback_approved.png", 2, 2, 0, 0),
        ("SPR_Player_Hit_A.png", "01_player_naicha_feedback_approved.png", 2, 2, 1, 0),
        ("SPR_Naicha_Chase_A.png", "01_player_naicha_feedback_approved.png", 2, 2, 0, 1),
        ("SPR_Naicha_Catch_A.png", "01_player_naicha_feedback_approved.png", 2, 2, 1, 1),
        ("SPR_Naiyu_Neutral_A.png", "05_naiyu_naisha_naihao_approved.png", 2, 2, 0, 0),
        ("SPR_Naiyu_Attack_A.png", "05_naiyu_naisha_naihao_approved.png", 2, 2, 1, 0),
        ("SPR_Naisha_Rage_A.png", "05_naiyu_naisha_naihao_approved.png", 2, 2, 0, 1),
        ("SPR_Naihao_Neutral_A.png", "05_naiyu_naisha_naihao_approved.png", 2, 2, 1, 1),
    ]
    for name, atlas, columns, rows, column, row in portrait_cells:
        outputs.append(save_asset(PORTRAIT_DIR, name,
            extract_cell(atlas, columns, rows, column, row)))

    sprite_cells = [
        ("SPR_Bomb_Standard_A.png", "02_bombs_approved_except_tea.png", 2, 2, 0, 0),
        ("SPR_Bomb_SoyBigCross_A.png", "02_bombs_approved_except_tea.png", 2, 2, 1, 0),
        ("SPR_Bomb_RatSplit_A.png", "02_bombs_approved_except_tea.png", 2, 2, 1, 1),
    ]
    for name, atlas, columns, rows, column, row in sprite_cells:
        outputs.append(save_asset(SPRITE_DIR, name,
            extract_cell(atlas, columns, rows, column, row)))
    outputs.append(save_asset(SPRITE_DIR, "SPR_Bomb_TeaRadial_A.png",
        extract_cell("06_tea_bomb_clean_approved.png", 1, 1, 0, 0)))

    loot_names = [
        "SPR_Loot_BombCapacity_A.png", "SPR_Loot_BombRange_A.png",
        "SPR_Loot_ChickenJump_A.png", "SPR_Loot_SpeedBoost_A.png",
        "SPR_Loot_Shield_A.png", "SPR_Loot_Xray_A.png",
        "SPR_Loot_ChaosReverse_A.png", "SPR_GoldenEgg_A.png",
    ]
    for index, name in enumerate(loot_names):
        outputs.append(save_asset(SPRITE_DIR, name,
            extract_cell("03_loot_icons_approved.png", 4, 2, index % 4, index // 4)))

    mechanic_names = [
        "SPR_Tile_SpeedPad_A.png", "SPR_Tile_Portal_A.png",
        "SPR_Tile_Spike_Inactive_A.png", "SPR_Tile_Spike_Active_A.png",
        "SPR_Tile_CrackedFloor_A.png", "SPR_Tile_CollapsedFloor_A.png",
        "SPR_Tile_PressurePlate_Idle_A.png", "SPR_Tile_PressurePlate_Active_A.png",
        "SPR_Tile_VaultGate_A.png", "SPR_Tile_GateDoor_A.png",
        "SPR_Tile_Conveyor_Left_A.png", "SPR_Tile_Conveyor_Right_A.png",
        "SPR_Tile_Exit_Locked_A.png", "SPR_Tile_Exit_Open_A.png",
        "SPR_Tile_WoodCrate_A.png", "SPR_Tile_BrokenCrate_A.png",
    ]
    for index, name in enumerate(mechanic_names):
        outputs.append(save_asset(SPRITE_DIR, name,
            extract_cell("04_mechanics_tiles_approved.png", 4, 4, index % 4, index // 4)))

    for output in outputs:
        print(f"{output['asset']}: {output['size'][0]}x{output['size'][1]}, {output['bytes']:,} bytes")
    print(f"Extracted {len(outputs)} approved transparent assets.")


if __name__ == "__main__":
    main()
