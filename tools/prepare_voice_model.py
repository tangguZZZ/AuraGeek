"""Pinned ESP-SR Chinese model in previously unused upper flash; no data relocation.

Model data license (retain with redistributed model bundles):
ESPRESSIF MIT License
Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>

Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
it is free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""
from pathlib import Path
import hashlib
import struct
import urllib.request

REV = "27da4f945f779bab2d238889924622f7988b1b1c"
FILES = {
    "_MODEL_INFO_": "251cee555d800867c5279cb5c88ef2cca10d0d250a17baf48f87830d502244aa",
    "mn7_data": "21bf13f3dc4792bdac9a9d6d6d3e69c782ded4071bba24df3da98ee14151120f",
    "mn7_index": "8a018cc23671dcee23a80ff343c37e87f3b893f144198a2dba09bcb9f745af75",
    "vocab": "7505578a7543455554a3d0a45cad7e2fd2f258097ba3b5f381d18d20d3f7ed51",
}
FACES = ["surprised-03-alert.gif", "thinking-02-peeking.gif", "happy-01-gentle-smile.gif",
         "sad-02-frustrated-1to1.gif", "angry-02-frowning.gif", "loving-02-heart-eyes.gif"]

def prepare(root):
    cache = root / ".pio/voice_models"
    cache.mkdir(parents=True, exist_ok=True)
    blobs = []
    for name, expected in FILES.items():
        target = cache / name
        data = target.read_bytes() if target.exists() else b""
        if hashlib.sha256(data).hexdigest() != expected:
            url = f"https://raw.githubusercontent.com/espressif/esp-sr/{REV}/model/multinet_model/mn7_cn/{name}"
            with urllib.request.urlopen(url, timeout=60) as response:
                data = response.read(4 * 1024 * 1024)
            if hashlib.sha256(data).hexdigest() != expected:
                raise RuntimeError(f"Voice model checksum mismatch: {name}")
            target.write_bytes(data)
        blobs.append((name, data))
    face_dir = root / "Doc/UI素材/机器人表情包gif_20260820/macbot表情包gif_第一版"
    faces = [(f"face{i}.gif", (face_dir / name).read_bytes()) for i, name in enumerate(FACES)]
    # Same ESP-SR container format, with an additional inert group of raw GIF files.
    # MultiNet7 requires its FST command resource even when commands are replaced later.
    groups = [(b"mn7_cn", blobs), (b"fst", [("commands_cn.txt", b"1,ni hao xiao chen\n")]), (b"ag_faces", faces)]
    offset = 4 + 36 * len(groups) + 40 * sum(len(items) for _, items in groups)
    header = struct.pack("<I", len(groups))
    payload = b""
    for group, items in groups:
        header += struct.pack("<32sI", group, len(items))
        for name, data in items:
            header += struct.pack("<32sII", name.encode(), offset + len(payload), len(data))
            payload += data
    packed = header + payload
    output = cache / "mn7_cn.bin"
    if not output.exists() or output.read_bytes() != packed:
        output.write_bytes(packed)
    if len(packed) > 0x800000:
        raise RuntimeError("Voice resource partition overflow")
    print(f"[VOICE MODEL] mn7_cn + {len(FACES)} selected GIFs verified, {len(packed)} bytes")

try:
    Import("env")
except NameError:
    prepare(Path(__file__).resolve().parent.parent)
else:
    prepare(Path(env.subst("$PROJECT_DIR")))
    model_path = str(Path(env.subst("$PROJECT_DIR")) / ".pio/voice_models/mn7_cn.bin")
    env.Append(FLASH_EXTRA_IMAGES=[("0x800000", model_path)])
    # The platform uploader flags are assembled before post scripts execute.
    if "uploadfs" not in __import__("SCons.Script", fromlist=["COMMAND_LINE_TARGETS"]).COMMAND_LINE_TARGETS:
        env.Append(UPLOADERFLAGS=["0x800000", model_path])
