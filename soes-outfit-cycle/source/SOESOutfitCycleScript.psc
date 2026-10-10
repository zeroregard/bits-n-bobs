Scriptname SOESOutfitCycleScript extends Quest
{Steps the player through their favorite Skyrim Outfit Equipment System NG outfits
on one key (default ' = DXScanCode 40, a Steam Controller back button here).}

Int Property CycleKey = 40 Auto

Event OnInit()
	RegisterForKey(CycleKey)
EndEvent

Event OnKeyDown(Int keyCode)
	If keyCode != CycleKey || Utility.IsInMenuMode()
		Return
	EndIf
	Actor player = Game.GetPlayer()
	String[] outfits = SkyrimOutfitEquipmentSystemNativeFuncs.ListOutfits(True)
	If outfits.Length == 0
		Debug.Notification("No favorite outfits - mark some in the SOES menu")
		Return
	EndIf
	Int current = outfits.Find(SkyrimOutfitEquipmentSystemNativeFuncs.GetSelectedOutfit(player))
	String nextOutfit = outfits[(current + 1) % outfits.Length]
	SkyrimOutfitEquipmentSystemNativeFuncs.SetSelectedOutfit(player, nextOutfit)
	SkyrimOutfitEquipmentSystemNativeFuncs.RefreshArmorFor(player)
	Debug.Notification(nextOutfit)
EndEvent
