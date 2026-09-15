"""Deterministically embed the local portal without writing the user's filesystem partition."""
from pathlib import Path
import gzip

def generate(project):
    source = project / "web" / "portal.html"
    # Git may check out CRLF on Windows; embed identical UTF-8/LF on every host.
    raw = source.read_text(encoding='utf-8').encode('utf-8')
    packed = gzip.compress(raw, compresslevel=9, mtime=0)
    # Python/zlib versions differ in gzip's informational OS byte; use unknown.
    packed = packed[:9] + b'\xff' + packed[10:]
    output = project / "src" / "web" / "PortalPage.generated.cpp"
    lines = [','.join(f'0x{b:02x}' for b in packed[i:i+24]) for i in range(0, len(packed), 24)]
    code = '#include "web/PortalPage.h"\nnamespace aurageek::web {\nconst uint8_t kPortalPage[]={\n' + ',\n'.join(lines) + '\n};\nconst size_t kPortalPageSize=sizeof(kPortalPage);\n}\n'
    if not output.exists() or output.read_text(encoding='utf-8') != code:
        output.write_text(code, encoding='utf-8')
    print(f'[PORTAL] HTML {len(raw)} bytes -> gzip {len(packed)} bytes')

try:
    Import("env")
except NameError:
    if __name__ == '__main__':
        generate(Path(__file__).resolve().parents[1])
else:
    generate(Path(env.subst("$PROJECT_DIR")))
