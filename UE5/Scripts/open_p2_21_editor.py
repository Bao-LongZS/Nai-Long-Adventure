"""Open the P2 2-1 development map and sync the accepted egg asset in the Content Browser."""
import unreal


MAP_PATH = "/Game/Naiwa/P2/Maps/L_NaiwaP2_21"
EGG_PATH = "/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg"


def main():
    egg = unreal.load_asset(EGG_PATH)
    if egg is None or not isinstance(egg, unreal.SkeletalMesh):
        raise RuntimeError(f"Could not load accepted NaiwaEgg Skeletal Mesh: {EGG_PATH}")
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    if world is None:
        raise RuntimeError(f"Could not open P2 2-1 development map: {MAP_PATH}")
    unreal.EditorAssetLibrary.sync_browser_to_objects([EGG_PATH])
    unreal.log(f"NAIWA_P2_EDITOR_READY map={MAP_PATH} egg={EGG_PATH}")


main()
