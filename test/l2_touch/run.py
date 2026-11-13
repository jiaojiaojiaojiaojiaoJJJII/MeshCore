"""Build and run the real portable L2 gesture/preview tests on Windows."""
import os
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
compiler = Path.home() / ".platformio/packages/toolchain-gccmingw32/bin/g++.exe"
if not compiler.exists():
    raise SystemExit("Install: pio pkg install --global --tool platformio/toolchain-gccmingw32")
env = dict(os.environ)
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
for name in ["gesture_test", "media_map_test"]:
    output = root / ("out/l2-" + name + ".exe")
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation", "-static", "-I", str(root / "examples/companion_radio/ui-seeed"), "-I", str(root / "variants/wio-tracker-l2-pro"), str(Path(__file__).with_name(name + ".cpp")), "-o", str(output)], check=True, env=env, cwd=root)
    subprocess.run([str(output)], check=True, env=env, cwd=root)
