#!/usr/bin/env python3
"""Patch SkyUI's Interface/SkyUI/config.txt for the active effect tabs.

Replaces the TIME LEFT column of the active effect view with a TYPE column
(@aecType: Equipped, Potion, Spell, Blessing, Disease, ...), set by patch_aec.py.

  patch_config.py <config.txt>     edits in place (keeps the inode, so Limo
                                   hardlinks stay intact) and is idempotent.
"""
import sys
p = sys.argv[1]
raw = open(p, 'rb').read()
nl = b'\r\n' if b'\r\n' in raw else b'\n'
s = raw.decode('latin-1')
NL = nl.decode()

if 'effectTypeColumn' in s:
    print("already patched"); sys.exit(0)

view_old = 'views.activeEffectView.columns = <equipColumn, magicIconColumn, magicNameColumn, effectDurationColumn, effectSourceColumn>'
view_new = 'views.activeEffectView.columns = <equipColumn, magicIconColumn, magicNameColumn, effectTypeColumn, effectSourceColumn>'
assert s.count(view_old) == 1, "activeEffectView columns line not found"
s = s.replace(view_old, view_new)

anchor = '; CRAFTING --- ICON COLUMN'
assert s.count(anchor) == 1, "insertion anchor not found"
block = NL.join([
    "; EFFECT --- TYPE COLUMN (Active Effect Categories) ---------------------------",
    "columns.effectTypeColumn.type = TEXT",
    "columns.effectTypeColumn.name = 'TYPE'",
    "columns.effectTypeColumn.states = 2",
    "columns.effectTypeColumn.width = 0.333",
    "columns.effectTypeColumn.label.textFormat.align = center",
    "columns.effectTypeColumn.entry.textFormat.align = center",
    "columns.effectTypeColumn.hidden = false",
    "",
    "columns.effectTypeColumn.state1.label.text = 'TYPE'",
    "columns.effectTypeColumn.state1.label.arrowDown = false",
    "columns.effectTypeColumn.state1.entry.text = @aecType",
    "columns.effectTypeColumn.state1.sortAttributes = <aecType, text>",
    "columns.effectTypeColumn.state1.sortOptions = <{ASCENDING | CASEINSENSITIVE}, {ASCENDING | CASEINSENSITIVE}>",
    "",
    "columns.effectTypeColumn.state2.label.text = 'TYPE'",
    "columns.effectTypeColumn.state2.label.arrowDown = true",
    "columns.effectTypeColumn.state2.entry.text = @aecType",
    "columns.effectTypeColumn.state2.sortAttributes = <aecType, text>",
    "columns.effectTypeColumn.state2.sortOptions = <{DESCENDING | CASEINSENSITIVE}, {ASCENDING | CASEINSENSITIVE}>",
    "", "", ""])
s = s.replace(anchor, block + anchor)

with open(p, 'r+b') as f:
    f.write(s.encode('latin-1')); f.truncate()
print("patched: TIME LEFT -> TYPE column")
