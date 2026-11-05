"""Courier Letters Kit's plugin, CourierLettersKit.esp (ESL, master Skyrim.esm only):

    python3 build.py OUT.esp

  0x800 QUST  CLK_Quest        start game enabled (DNAM as Mathias' letter quests), runs CLK_QuestScript (WICourier = Skyrim.esm 00039F82)
  0x801-0x81E BOOK  CLK_Letter01..30   the letter slots: notes copied from the vanilla love letter (T02LoveLetter), with a
              placeholder name and text; the DLL gives each slot its Title and text from that slot's INI file
No dialogue, so no SEQ file is needed. Never renumber: saves hold the delivered notes.
"""
import struct, sys
sys.path.insert(0, '/home/deck/opt/esptools')
from esp import recs, subs

DATA = '/home/deck/.local/share/Steam/steamapps/common/Skyrim Special Edition/Data/'
WICOURIER = 0x039F82
LOVE_LETTER = 0x026EFE      # a folded note (inventory model, book menu as a note)
SLOTS = 30
PLACEHOLDER = ("This letter's text comes from Data/SKSE/Plugins/CourierLettersKit. If you can read this, "
               "CourierLettersKit.dll isn't loaded or no file gives this slot a text.")


def z(s): return s.encode('cp1252') + b'\0'
def ws(s):
    b = s.encode('cp1252'); return struct.pack('<H', len(b)) + b
def sub(t, v): return t + struct.pack('<H', len(v)) + v
def rec(t, fid, sl, flags=0):
    body = b''.join(sub(k, v) for k, v in sl)
    return t + struct.pack('<IIIHHHH', len(body), flags, fid, 0, 0, 44, 0) + body
def edit(sl, changes, drop=()):
    out, done = [], set()
    for k, v in sl:
        if k in drop: continue
        if k in changes and k not in done:
            done.add(k); out.append((k, changes[k])); continue
        out.append((k, v))
    return out


def build(out):
    sky = {}
    for t, fid, body in recs(open(DATA + 'Skyrim.esm', 'rb').read()):
        if fid == LOVE_LETTER: sky[fid] = (t, list(subs(body)))
    ME = 1 << 24
    records = []
    vmad = (struct.pack('<hhH', 5, 2, 1) + ws('CLK_QuestScript') + b'\0' + struct.pack('<H', 1)
            + ws('WICourier') + bytes([1, 1]) + struct.pack('<HhI', 0, -1, WICOURIER)
            + bytes.fromhex('02000000000000'))   # an empty quest fragment block (version 2, no fragments, no aliases)
    records.append((b'QUST', ME | 0x800, [(b'EDID', z('CLK_Quest')), (b'VMAD', vmad), (b'FULL', z('Courier Letters')),
                                          (b'DNAM', bytes.fromhex('010100000000000000000000')), (b'ANAM', bytes(4))]))
    _, sl = sky[LOVE_LETTER]
    for n in range(1, SLOTS + 1):
        records.append((b'BOOK', ME | (0x800 + n), edit(sl, {b'EDID': z(f'CLK_Letter{n:02d}'), b'FULL': z(f'Letter ({n})'),
                                                            b'DESC': z(PLACEHOLDER)}, drop=(b'CNAM', b'VMAD'))))
    groups = b''
    for gt in (b'QUST', b'BOOK'):
        g = b''.join(rec(t, fid, sl) for t, fid, sl in records if t == gt)
        groups += b'GRUP' + struct.pack('<I', 24 + len(g)) + gt + struct.pack('<iHHHH', 0, 0, 0, 0, 0) + g
    hdr = [(b'HEDR', struct.pack('<fII', 1.71, len(records), 0x800 + SLOTS + 1)), (b'CNAM', z('Claude')),
           (b'MAST', z('Skyrim.esm')), (b'DATA', bytes(8))]
    tes4 = b''.join(sub(k, v) for k, v in hdr)
    head = b'TES4' + struct.pack('<IIIHHHH', len(tes4), 0x200, 0, 0, 0, 44, 0) + tes4   # 0x200: light (ESL)
    open(out, 'wb').write(head + groups)
    print(out, len(head + groups), 'bytes,', len(records), 'records')


if __name__ == '__main__':
    build(sys.argv[1])
