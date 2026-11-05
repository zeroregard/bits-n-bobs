Scriptname CLK_Native Native Hidden
{Courier Letters Kit: the DLL's side (CourierLettersKit.dll). Letters are INI files in Data/SKSE/Plugins/CourierLettersKit.}

; the slot (1-30) of the first letter whose conditions all hold now and that isn't delivered yet, 0 when none.
; afDelivered[slot - 1] is the game day it was delivered, below 0 when not yet
int Function NextDue(float[] afDelivered) global native
; the note record of a slot
Form Function Letter(int aiSlot) global native
; what comes with a letter (Item = / Gold = lines), and how many of each
Form[] Function Items(int aiSlot) global native
int[] Function Counts(int aiSlot) global native
; tells the DLL a letter went to the courier (for its log)
Function Delivered(int aiSlot) global native
; writes to CourierLettersKit.log why each letter is or isn't due (from the console: cqf CLK_Quest Explain)
Function Explain(float[] afDelivered) global native
