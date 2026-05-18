from pathlib import Path

# def norm_path(path: Path) -> str:
#     return path.as_posix()

CONFIG_DIR = Path(__file__).resolve().parent
ROOT_DIR = CONFIG_DIR.parent.parent
ASSETS_DIR = ROOT_DIR / "assets"
THIRD_PARTY_DIR = ROOT_DIR / "third_party"


if __name__ == "__main__":
    print("CONFIG_DIR:", CONFIG_DIR)
    print("ROOT_DIR:", ROOT_DIR)
    print("ASSETS_DIR:", ASSETS_DIR)
    print("THIRD_PARTY_DIR:", THIRD_PARTY_DIR)
