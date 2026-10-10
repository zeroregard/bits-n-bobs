"""Write 'SOES Outfit Cycle.esp': one ESL-flagged, start-game-enabled quest running
SOESOutfitCycleScript. Record layout copied from SkyUI's script-only quests.
  build_esp.py <out.esp>
"""
import struct, sys

def sub(t, data): return t + struct.pack('<H', len(data)) + data
def wstr(s): return struct.pack('<H', len(s)) + s.encode()

script = 'SOESOutfitCycleScript'
vmad = struct.pack('<HHH', 5, 2, 1) + wstr(script) + b'\x00' + struct.pack('<H', 0)   # 1 script, no properties
vmad += b'\x02' + struct.pack('<H', 0) + wstr('') + struct.pack('<H', 0)               # no fragments, no aliases
qust = (sub(b'EDID', b'SOESOutfitCycleQuest\0') + sub(b'VMAD', vmad)
        + sub(b'DNAM', bytes.fromhex('110100ff0000000000000000'))                     # start game enabled, as SkyUI's
        + sub(b'NEXT', b'') + sub(b'ANAM', struct.pack('<I', 0)))
fid = 0x01000800                                                                       # own index 1 (after Skyrim.esm), ESL range
rec = b'QUST' + struct.pack('<III', len(qust), 0, fid) + struct.pack('<IHH', 0, 44, 0) + qust
grup = b'GRUP' + struct.pack('<I', 24 + len(rec)) + b'QUST' + struct.pack('<I', 0) + struct.pack('<IHH', 0, 0, 0) + rec
hdr = (sub(b'HEDR', struct.pack('<fII', 1.71, 1, 0x801)) + sub(b'CNAM', b'Claude\0')
       + sub(b'SNAM', b'Cycles favorite SOES NG outfits on one key\0')
       + sub(b'MAST', b'Skyrim.esm\0') + sub(b'DATA', struct.pack('<Q', 0)))
tes4 = b'TES4' + struct.pack('<III', len(hdr), 0x200, 0) + struct.pack('<IHH', 0, 44, 0) + hdr   # 0x200 = ESL
open(sys.argv[1], 'wb').write(tes4 + grup)
