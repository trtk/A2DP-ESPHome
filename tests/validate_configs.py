"""Run schema tests with disposable fake credentials; no user secrets required."""
from pathlib import Path
import base64
import copy
import secrets
import subprocess
import sys
import tempfile
import yaml

root = Path(__file__).resolve().parents[1]
class Loader(yaml.SafeLoader):
    pass
Loader.add_constructor("!secret", lambda loader, node: {"__secret__": loader.construct_scalar(node)})
raw = yaml.load((root / "examples/kallsup.yaml").read_text(), Loader=Loader)
values = {
    "wifi_ssid": "TEST_NETWORK", "wifi_password": "TEST_PASSWORD",
    "api_encryption_key": base64.b64encode(secrets.token_bytes(32)).decode(),
    "ota_password": "TEST_PASSWORD", "kallsup_mac": "AA:BB:CC:DD:EE:FF",
}
def resolve(value):
    if isinstance(value, dict):
        if set(value) == {"__secret__"}:
            return values[value["__secret__"]]
        return {key: resolve(item) for key, item in value.items()}
    if isinstance(value, list):
        return [resolve(item) for item in value]
    return value

base = resolve(raw)
base["external_components"][0]["source"] = {"type": "local", "path": str(root / "components")}
cases = [
    ("valid", {}, None),
    ("c3", {"board": "esp32-c3-devkitm-1"}, "original ESP32"),
    ("arduino", {"framework": {"type": "arduino"}}, "requires framework"),
    ("ble", {}, "owns the Classic Bluetooth stack"),
]
with tempfile.TemporaryDirectory() as temp:
    for name, patch, expected in cases:
        config = copy.deepcopy(base)
        config["esp32"].update(patch)
        if name == "ble":
            config["esp32_ble"] = {}
        path = Path(temp) / f"{name}.yaml"
        path.write_text(yaml.safe_dump(config, sort_keys=False))
        result = subprocess.run([sys.executable, "-m", "esphome", "config", str(path)], capture_output=True, text=True)
        if expected is None:
            assert result.returncode == 0, "Valid configuration rejected"
        else:
            assert result.returncode != 0 and expected in result.stdout + result.stderr, name
        print(f"PASS: {name}")
