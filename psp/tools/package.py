"""Install only the PSP/GAME package; ISO generation is intentionally absent."""
from pathlib import Path
import hashlib
import shutil
import struct

port = Path(__file__).resolve().parents[1]
output = port.parent
install = output / 'EllisHousePSP'
install.mkdir(exist_ok=True)

pbp = port / 'EBOOT.PBP'
data = pbp.read_bytes()
assert data.startswith(b'\0PBP'), 'invalid PBP header'
offsets = struct.unpack_from('<8I', data, 8)
sfo = data[offsets[0]:offsets[1]]
data_psp = data[offsets[6]:offsets[7]]
assert data_psp.startswith(b'\x7fELF'), 'DATA.PSP must be an unencrypted PSP ELF'
assert struct.unpack_from('<H', data_psp, 16)[0] == 2, 'DATA.PSP must be a static ET_EXEC PSP ELF'
phoff = struct.unpack_from('<I', data_psp, 28)[0]
assert struct.unpack_from('<H', data_psp, 44)[0] == 1, 'PSP ELF must contain one legacy LOAD segment'
ptype, poffset, pvaddr = struct.unpack_from('<3I', data_psp, phoff)
pflags, palign = struct.unpack_from('<2I', data_psp, phoff + 24)
assert (ptype, poffset, pvaddr, pflags, palign) == (1, 0x1000, 0x08900000, 7, 0x1000), \
    'PSP ELF segment layout differs from the hardware-proven Celeste package'
assert b'CATEGORY\0' in sfo and b'MG\0' in sfo, 'PARAM.SFO must be a homebrew game'
assert b'EFAWARE\0' not in sfo, 'static ef0:/ homebrew must retain the PSP Go EF2 launch path'
assert b'"EllisHouse"\0' not in data_psp, 'C++ module name contains invalid quote characters'
assert b'EllisHouse\0' in data_psp, 'expected PSP module name is missing'

shutil.copyfile(pbp, install / 'EBOOT.PBP')
shutil.copytree(port / 'package/ASSETS', install / 'ASSETS', dirs_exist_ok=True)
for stale in (install / 'ASSETS').glob('SND_*.PCM'):
    stale.unlink()

print(f'PBP: {install / "EBOOT.PBP"} ({len(data)} bytes)')
print('SHA256:', hashlib.file_digest((install / 'EBOOT.PBP').open('rb'), 'sha256').hexdigest())
