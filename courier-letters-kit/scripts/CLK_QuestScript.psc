Scriptname CLK_QuestScript extends Quest
{Courier Letters Kit: every PollSeconds asks the DLL which letter is due and hands it, with its enclosures, to the vanilla
courier (WICourier), who finds the player as for any other delivery. What was delivered, and on which game day, is kept
here in the save, so a letter comes only once per character.}

Quest Property WICourier Auto
{The vanilla courier quest, Skyrim.esm 00039F82.}
float Property FirstDelay = 20.0 Auto
float Property PollSeconds = 30.0 Auto

float[] delivered

Event OnInit()
	Setup()
	RegisterForSingleUpdate(FirstDelay)
EndEvent

Function Setup()
	if !delivered
		delivered = new float[30]
		int i = 0
		while i < 30
			delivered[i] = -1.0
			i += 1
		endwhile
	endif
EndFunction

Event OnUpdate()
	Setup()
	Actor player = Game.GetPlayer()
	if !Utility.IsInMenuMode() && !player.IsOnMount()
		int slot = CLK_Native.NextDue(delivered)
		if slot > 0
			Send(slot)
		endif
	endif
	RegisterForSingleUpdate(PollSeconds)
EndEvent

Function Send(int slot)
	WICourierScript courier = WICourier as WICourierScript
	Form note = CLK_Native.Letter(slot)
	if !courier || !note
		Debug.Trace("[CourierLettersKit] can't send letter " + slot + ": courier " + courier + ", note " + note)
		return
	endif
	courier.addItemToContainer(note, 1)
	Form[] items = CLK_Native.Items(slot)
	int[] counts = CLK_Native.Counts(slot)
	int i = 0
	while i < items.Length
		if items[i]
			courier.addItemToContainer(items[i], counts[i])
		endif
		i += 1
	endwhile
	delivered[slot - 1] = Utility.GetCurrentGameTime()
	CLK_Native.Delivered(slot)
	Debug.Trace("[CourierLettersKit] letter " + slot + " handed to the courier")
EndFunction

; from the console: cqf CLK_Quest Explain (the reasons go to Data/../SKSE/CourierLettersKit.log)
Function Explain()
	Setup()
	CLK_Native.Explain(delivered)
EndFunction

; from the console: cqf CLK_Quest Resend 3 (forget that letter 3 was delivered, so it can come again)
Function Resend(int slot)
	Setup()
	if slot >= 1 && slot <= 30
		delivered[slot - 1] = -1.0
	endif
EndFunction
