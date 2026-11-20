"""가상보드 프로그램(I2R_IoT 02_PLC_Simulator)을 i2r-02/03/04용 .bin으로 만든다.

시뮬레이터 화면의 "🔌 USB로 실제 보드 찾기 → 가상보드 프로그램 다운로드"가 이 파일들을 브라우저로 보드에 쓴다.
I2R_IoT나 I2R_PLC 라이브러리가 바뀌면 이 스크립트를 다시 돌리고 커밋한다.

  python build_firmware.py                  arduino-cli를 PATH 또는 Arduino IDE 안에서 찾는다
  python build_firmware.py --cli <경로>     arduino-cli 경로를 직접 준다

필요한 것: arduino-cli, esp32:esp32 보드 패키지, 라이브러리 I2R_IoT, I2R_PLC, PubSubClient, ArduinoJson
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
FQBN = "esp32:esp32:esp32:FlashSize=8M,PartitionScheme=huge_app"
SKETCH = "02_PLC_Simulator"
KINDS = {"i2r-02": "I2R_02", "i2r-03": "I2R_03", "i2r-04": "I2R_04"}
BOARD_LINE = "I2R_PLC plc(I2R_03);"
# ESP32(클래식)의 쓰는 위치. 0x9000 ~ 0xDFFF(NVS: Wi-Fi 설정)는 쓰지 않으므로 설정이 남는다
PARTS = [("bootloader.bin", 0x1000), ("partitions.bin", 0x8000), ("boot_app0.bin", 0xE000)]
APP_ADDR = 0x10000


def find_cli(given):
    if given:
        return given
    c = shutil.which("arduino-cli")
    if c:
        return c
    ide = r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    if os.path.exists(ide):
        return ide
    sys.exit("arduino-cli를 찾지 못했습니다. --cli 로 경로를 주세요.")


def run_json(cli, *args):
    out = subprocess.run([cli, *args, "--format", "json"], capture_output=True, check=True)
    return json.loads(out.stdout.decode("utf-8"))


def lib_info(cli, name):
    for x in run_json(cli, "lib", "list").get("installed_libraries", []):
        lib = x.get("library", {})
        if lib.get("name") == name:
            return lib
    sys.exit(f"라이브러리 {name}가 없습니다: arduino-cli lib install {name}")


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description="가상보드 프로그램 .bin 만들기")
    ap.add_argument("--cli", default=None, help="arduino-cli 경로")
    a = ap.parse_args()
    cli = find_cli(a.cli)

    iot = lib_info(cli, "I2R_IoT")
    plc = lib_info(cli, "I2R_PLC")
    src = os.path.join(iot["install_dir"], "examples", SKETCH, SKETCH + ".ino")
    with open(src, encoding="utf-8") as f:
        code = f.read()
    if BOARD_LINE not in code:
        sys.exit(f"{src}에 '{BOARD_LINE}' 줄이 없습니다. 예제가 바뀌었는지 확인하세요.")
    core = next((p.get("installed_version") for p in run_json(cli, "core", "list").get("platforms", [])
                 if p.get("id") == "esp32:esp32"), None)

    manifest = {"sketch": SKETCH, "built": time.strftime("%Y-%m-%d"), "fqbn": FQBN, "esp32_core": core,
                "I2R_IoT": iot.get("version"), "I2R_PLC": plc.get("version"),
                "parts": [{"file": n, "addr": addr} for n, addr in PARTS], "kinds": {}}

    with tempfile.TemporaryDirectory() as tmp:
        for kind, const in KINDS.items():
            sk = os.path.join(tmp, kind, SKETCH)
            os.makedirs(sk)
            with open(os.path.join(sk, SKETCH + ".ino"), "w", encoding="utf-8") as f:
                f.write(code.replace(BOARD_LINE, f"I2R_PLC plc({const});"))
            build = os.path.join(tmp, kind, "build")
            print(f"[{kind}] 컴파일 중...", flush=True)
            r = subprocess.run([cli, "compile", "--fqbn", FQBN, "--build-path", build, sk],
                               capture_output=True)
            if r.returncode != 0:
                sys.stdout.write(r.stdout.decode("utf-8", "replace") + r.stderr.decode("utf-8", "replace"))
                sys.exit(f"[{kind}] 컴파일 실패")
            app = f"simcheck-{kind}.bin"
            shutil.copy(os.path.join(build, SKETCH + ".ino.bin"), os.path.join(HERE, app))
            manifest["kinds"][kind] = {"file": app, "addr": APP_ADDR, "sha256": sha256(os.path.join(HERE, app))}
            if kind == "i2r-03":   # 부트로더, 파티션은 보드 종류와 상관없이 같다
                shutil.copy(os.path.join(build, SKETCH + ".ino.bootloader.bin"), os.path.join(HERE, "bootloader.bin"))
                shutil.copy(os.path.join(build, SKETCH + ".ino.partitions.bin"), os.path.join(HERE, "partitions.bin"))
                shutil.copy(os.path.join(build, "boot_app0.bin"), os.path.join(HERE, "boot_app0.bin"))
            print(f"[{kind}] {app} ({os.path.getsize(os.path.join(HERE, app)) // 1024} KB)", flush=True)

    for p in manifest["parts"]:
        p["sha256"] = sha256(os.path.join(HERE, p["file"]))
    with open(os.path.join(HERE, "manifest.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"완료: I2R_IoT {manifest['I2R_IoT']}, I2R_PLC {manifest['I2R_PLC']}, esp32 {core}")


if __name__ == "__main__":
    main()
