ScriptName Transporter_SF Hidden
{Star Trek-style transporter for Starfield: the beam sequence (Teleport), the transporter
 menu's answers, and the settings they read. Split from CCFE-SF.
 Teleport is a global function that takes one string of space-separated arguments, so it works
 two ways:
   - typed as a short command, through Transporter_SF.dll and ModernCommandFramework:  Teleport 0001ABCD
   - typed with the vanilla console's CallGlobalFunction:  cgf "Transporter_SF.Teleport" "0001ABCD"
 Requires Cassiopeia Papyrus Extender; OSF Settings and OSF UI for the menu and in-game settings.}

; =====================================================================================
; Output, arguments, forms
; =====================================================================================

Function Print(String asText) global
	CassiopeiaPapyrusExtender.PrintToConsoleLog(asText)
EndFunction

; Splits the argument string on spaces. Empty pieces (from double spaces) are kept
; here and skipped by ArgCount and Arg, so no empty array is ever created.
String[] Function Tokens(String asArgs) global
	If asArgs == ""
		Return None
	EndIf
	Return CassiopeiaPapyrusExtender.SplitStrKC(asArgs, " ")
EndFunction

Int Function ArgCount(String[] akTokens) global
	If akTokens == None
		Return 0
	EndIf
	Int count = 0
	Int i = 0
	While i < akTokens.Length
		If akTokens[i] != ""
			count += 1
		EndIf
		i += 1
	EndWhile
	Return count
EndFunction

; Returns the Nth non-empty argument (0-based), or "" when there is none.
String Function Arg(String[] akTokens, Int aiIndex) global
	If akTokens == None
		Return ""
	EndIf
	Int seen = 0
	Int i = 0
	While i < akTokens.Length
		If akTokens[i] != ""
			If seen == aiIndex
				Return akTokens[i]
			EndIf
			seen += 1
		EndIf
		i += 1
	EndWhile
	Return ""
EndFunction

; True when any argument equals asFlag (Papyrus string comparison ignores case).
Bool Function HasFlag(String[] akTokens, String asFlag) global
	Int i = 0
	Int n = ArgCount(akTokens)
	While i < n
		If Arg(akTokens, i) == asFlag
			Return True
		EndIf
		i += 1
	EndWhile
	Return False
EndFunction

; "player", a hex form ID without 0x (14, 0001ABCD, FF00012A), or an EditorID.
Form Function FormFromToken(String asToken) global
	If asToken == ""
		Return None
	EndIf
	If asToken == "player"
		Return Game.GetPlayer()
	EndIf
	Form found = CassiopeiaPapyrusExtender.GetFormByHexFormID(asToken)
	If found
		Return found
	EndIf
	Form[] byEditorID = CassiopeiaPapyrusExtender.GetFormsByEditorID(asToken, False)
	If byEditorID == None || byEditorID.Length == 0
		byEditorID = CassiopeiaPapyrusExtender.GetFormsByEditorID(asToken, True)
	EndIf
	If byEditorID != None && byEditorID.Length > 0
		Return byEditorID[0]
	EndIf
	Return None
EndFunction

; EditorID [TYPE:HEXID] "Name", leaving out the parts a form doesn't have.
String Function FullID(Form akForm) global
	If akForm == None
		Return "None"
	EndIf
	String result = "[" + CassiopeiaPapyrusExtender.GetFormTypeAsString(akForm) + ":" + CassiopeiaPapyrusExtender.GetHexFormID(akForm) + "]"
	String editorID = CassiopeiaPapyrusExtender.GetFormEditorID(akForm)
	If editorID != ""
		result = editorID + " " + result
	EndIf
	String displayName
	ObjectReference asRef = akForm as ObjectReference
	If asRef
		displayName = CassiopeiaPapyrusExtender.GetReferenceName(asRef)
	Else
		displayName = CassiopeiaPapyrusExtender.GetTESFullName(akForm)
	EndIf
	If displayName != ""
		result += " \"" + displayName + "\""
	EndIf
	Return result
EndFunction

; Settings from Data\SFSE\Plugins\Transporter_SF.ini; asDefault when the key is missing or empty.
String Function Setting(String asSection, String asKey, String asDefault) global
	String value = CassiopeiaPapyrusExtender.ReadIni(".\\Data\\SFSE\\Plugins\\", "Transporter_SF", asSection, asKey)
	If value == ""
		Return asDefault
	EndIf
	Return value
EndFunction

; -------------------------------------------------------------------------------------
; Transporter settings: OSF Settings (Pause > MOD SETTINGS, or F10) when it is ready,
; otherwise [Transporter] in Transporter_SF.ini. Both use the same keys (sOutLight, fFadeSeconds...),
; and the first letter gives the type: s string, f float, i int, b bool, e choice.
; They are read each time they are used, so changes apply on the next beam.
; -------------------------------------------------------------------------------------

String Function ModID() global
	Return "transporter-sf"
EndFunction

String Function ViewID() global
	Return "transporter-sf/menu"
EndFunction

Bool Function UseOSFSettings() global
	Return OSFSettings.IsReady()
EndFunction

String Function SettingS(String asKey, String asDefault) global
	If UseOSFSettings()
		Return OSFSettings.GetString(ModID(), asKey, asDefault)
	EndIf
	Return Setting("Transporter", asKey, asDefault)
EndFunction

Float Function SettingF(String asKey, Float afDefault) global
	If UseOSFSettings()
		Return OSFSettings.GetFloat(ModID(), asKey, afDefault)
	EndIf
	Return Setting("Transporter", asKey, afDefault as String) as Float
EndFunction

Int Function SettingI(String asKey, Int aiDefault) global
	If UseOSFSettings()
		Return OSFSettings.GetInt(ModID(), asKey, aiDefault)
	EndIf
	Return Setting("Transporter", asKey, aiDefault as String) as Int
EndFunction

Bool Function SettingB(String asKey, Bool abDefault) global
	If UseOSFSettings()
		Return OSFSettings.GetBool(ModID(), asKey, abDefault)
	EndIf
	String fallback = "0"
	If abDefault
		fallback = "1"
	EndIf
	Return (Setting("Transporter", asKey, fallback) as Int) != 0
EndFunction

String Function SettingE(String asKey, String asDefault) global
	If UseOSFSettings()
		Return OSFSettings.GetEnum(ModID(), asKey, asDefault)
	EndIf
	Return Setting("Transporter", asKey, asDefault)
EndFunction

; A form setting: hex form ID or EditorID; None when empty.
Form Function FormSettingS(String asKey) global
	Return FormFromToken(SettingS(asKey, ""))
EndFunction

; =====================================================================================
; Teleport: the transporter sequence
;   1. countdown                       iCountdownSeconds
;   2. activation gesture              watch idle, or a custom GLB animation through NAF/SAF
;   3. beam out (player + followers)   light, shader, spell, sound; screen effect once
;   4. fade, move, wait for arrival
;   5. beam in (player + followers)    the same slots for the "In" phase
; Every slot is read from OSF Settings (or [Transporter] in Transporter_SF.ini) each time, so the
; look can be tuned in game without recompiling. Each phase is its own function so the
; fast-travel hook can reuse them later; the transporter menu calls Teleport itself.
; =====================================================================================

Function Teleport(String asArgs = "") global
	String[] args = Tokens(asArgs)
	If ArgCount(args) == 0 || Arg(args, 0) == "?"
		Print("Usage: Teleport <reference | cell | quest> [solo] [nofx] [ft]")
		Print("       Teleport ship [solo] [nofx]   (aboard your home ship, behind the pilot seat)")
		Print("       Teleport xyz <x> <y> <z> [solo] [nofx]   (coordinates in the current cell or worldspace)")
		Print("  solo: leave followers behind.  nofx: no countdown, gesture or effects.")
		Print("  ft: travel to a reference with the game's own fast travel (loading screen, time passes, followers follow by themselves).")
		Return
	EndIf

	Bool solo = HasFlag(args, "solo")
	Bool withFx = !HasFlag(args, "nofx")
	Bool useFastTravel = HasFlag(args, "ft")
	Actor player = Game.GetPlayer()

	; --- destination ---
	Bool toCoordinates = False
	Float destX = 0.0
	Float destY = 0.0
	Float destZ = 0.0
	Form destination = None
	ObjectReference destRef = None
	Cell destCell = None
	Quest destQuest = None
	Float behind = 0.0
	String label

	If Arg(args, 0) == "ship"
		destRef = ShipDestination()
		If destRef == None
			Print("Teleport: you have no home ship")
			Return
		EndIf
		; arrive fShipSeatOffset units behind the pilot seat (the offset turns with the seat)
		behind = SettingF("fShipSeatOffset", 96.0)
		label = "your ship"
	ElseIf Arg(args, 0) == "xyz"
		If ArgCount(args) < 4
			Print("Teleport: xyz needs three numbers, e.g. Teleport xyz 1200 -340 85")
			Return
		EndIf
		toCoordinates = True
		destX = Arg(args, 1) as Float
		destY = Arg(args, 2) as Float
		destZ = Arg(args, 3) as Float
		label = "X " + destX + ", Y " + destY + ", Z " + destZ
	Else
		destination = FormFromToken(Arg(args, 0))
		destRef = destination as ObjectReference
		destCell = destination as Cell
		destQuest = destination as Quest
		If destRef == None && destCell == None && destQuest == None
			Print("Teleport: \"" + Arg(args, 0) + "\" is not a reference, cell or quest (got " + FullID(destination) + ")")
			Return
		EndIf
		label = FullID(destination)
	EndIf
	If useFastTravel && destRef == None
		Print("Teleport: ft works with a reference destination only; moving directly instead")
		useFastTravel = False
	EndIf

	If player.IsInSpace()
		Print("Teleport: you are in space; moving out of a ship in flight is untested")
	EndIf

	; --- energize ---
	Actor[] travellers = GatherTravellers(player, solo)
	If withFx
		Countdown()
		PlayGestures(travellers)
	EndIf
	ObjectReference[] outLights = BeamPhase(travellers, "Out", withFx)
	Utility.Wait(SettingF("fDelayBeforeMove", 2.5))
	DeleteAll(outLights)

	Float fadeSeconds = SettingF("fFadeSeconds", 0.5)
	Bool fade = withFx && SettingB("bFade", True)
	If fade
		Game.FadeOutGame(True, True, 0.0, fadeSeconds, True)
		Utility.Wait(fadeSeconds)
	EndIf
	If withFx
		StopGestures(travellers)
	EndIf

	; --- transit ---
	Cell startCell = player.GetParentCell()
	If toCoordinates
		player.SetPosition(destX, destY, destZ)
	ElseIf useFastTravel
		Game.FastTravel(destRef)
	ElseIf destRef
		player.MoveTo(destRef, 0.0, -behind, 0.0, True, True)
	ElseIf destCell
		Debug.CenterOnCellAndWait(CassiopeiaPapyrusExtender.GetFormEditorID(destCell))
	ElseIf destQuest
		Debug.ExecuteConsole("MoveToQuestTarget " + CassiopeiaPapyrusExtender.GetFormEditorID(destQuest))
	EndIf
	WaitForArrival(player, startCell)
	Utility.Wait(SettingF("fDelayAfterMove", 1.0))

	; --- materialize ---
	If useFastTravel
		; the game brought the followers; pick up whoever arrived with the player
		travellers = GatherTravellers(player, solo)
	Else
		MoveFollowersToPlayer(travellers)
	EndIf
	If fade
		Game.FadeOutGame(False, True, 0.0, fadeSeconds, False)
	EndIf
	ObjectReference[] inLights = BeamPhase(travellers, "In", withFx)
	Utility.Wait(SettingF("fInLightSeconds", 2.0))
	DeleteAll(inLights)

	Print("Teleported to " + label)
EndFunction

; The player first, then each follower within fPassengerRadius (unless abSolo).
Actor[] Function GatherTravellers(Actor akPlayer, Bool abSolo) global
	Actor[] followers = None
	Int aboard = 0
	If !abSolo
		followers = Game.GetPlayerFollowers()
		If followers != None
			Float radius = SettingF("fPassengerRadius", 1024.0)
			Int i = 0
			While i < followers.Length
				If followers[i] != None && followers[i].GetDistance(akPlayer) <= radius
					aboard += 1
				Else
					followers[i] = None
				EndIf
				i += 1
			EndWhile
		EndIf
	EndIf
	Actor[] travellers = new Actor[aboard + 1]
	travellers[0] = akPlayer
	Int slot = 1
	If followers != None
		Int f = 0
		While f < followers.Length
			If followers[f] != None
				travellers[slot] = followers[f]
				slot += 1
			EndIf
			f += 1
		EndWhile
	EndIf
	Return travellers
EndFunction

; Your Skyrim handler never reset its loop counter before moving followers, so they
; stayed behind. Here each follower gets a place in a small grid behind the player
; (offsets turn with the player's facing).
; Puts followers beside the player after the beam. The game also moves followers after a
; cell change, on its own schedule, and an NPC moved onto a spot without navmesh (often the
; case at map markers and ship seats) is pushed to the nearest navmesh, which can be metres
; away. So placement is checked for a second here, then for a few more seconds in the
; background while the beam-in plays (GuardFollowers).
Function MoveFollowersToPlayer(Actor[] akTravellers) global
	If akTravellers.Length < 2
		Return
	EndIf
	Actor player = akTravellers[0]
	Int[] tries = new Int[16]
	Int pass = 0
	While pass < 4
		Int i = 1
		While i < akTravellers.Length && i < 16
			tries[i] = PlaceFollower(akTravellers[i], player, i, tries[i], pass == 0)
			i += 1
		EndWhile
		Utility.Wait(0.25)
		pass += 1
	EndWhile
	Var[] args = new Var[6]
	args[0] = player
	Int k = 1
	While k < 6
		If k < akTravellers.Length
			args[k] = akTravellers[k]
		EndIf
		k += 1
	EndWhile
	Utility.CallGlobalFunctionNoWait("Transporter_SF", "GuardFollowers", args)
EndFunction

; Keeps up to five followers beside the player for about four seconds after arrival.
Function GuardFollowers(Actor akPlayer, Actor akF1, Actor akF2, Actor akF3, Actor akF4, Actor akF5) global
	Int t1 = 1
	Int t2 = 1
	Int t3 = 1
	Int t4 = 1
	Int t5 = 1
	Int pass = 0
	While pass < 16
		t1 = PlaceFollower(akF1, akPlayer, 1, t1, False)
		t2 = PlaceFollower(akF2, akPlayer, 2, t2, False)
		t3 = PlaceFollower(akF3, akPlayer, 3, t3, False)
		t4 = PlaceFollower(akF4, akPlayer, 4, t4, False)
		t5 = PlaceFollower(akF5, akPlayer, 5, t5, False)
		Utility.Wait(0.25)
		pass += 1
	EndWhile
	If akF1
		Debug.Trace("Transporter_SF: follower " + akF1 + " ends " + (akF1.GetDistance(akPlayer) as Int) + " units from the player", 0)
	EndIf
EndFunction

; Moves akFollower to its slot behind the player when abForce, or when it has ended up in
; another space or farther than about two body widths beyond its slot. After two tries at
; the slot it goes on the player's own spot, which has navmesh under it. Returns the tries.
Int Function PlaceFollower(Actor akFollower, Actor akPlayer, Int aiSlot, Int aiTries, Bool abForce) global
	If !akFollower || !akPlayer
		Return aiTries
	EndIf
	Float spacing = SettingF("fPassengerSpacing", 96.0)
	Bool sameSpace = CassiopeiaPapyrusExtender.IsInSameSpace(akFollower, akPlayer)
	Float distance = -1.0
	If sameSpace
		distance = akFollower.GetDistance(akPlayer)
	EndIf
	If !abForce && sameSpace && distance <= spacing * 2.0 + 128.0
		Return aiTries
	EndIf
	If aiTries < 2
		akFollower.MoveTo(akPlayer, spacing * ((aiSlot % 3) - 1), -spacing * ((aiSlot / 3) + 1), 0.0, True, True)
	Else
		akFollower.MoveTo(akPlayer, 0.0, 0.0, 0.0, True, False)
	EndIf
	If !abForce
		Debug.Trace("Transporter_SF: follower " + akFollower + " was " + (distance as Int) + " units away (try " + aiTries + "); moved back", 0)
	EndIf
	Return aiTries + 1
EndFunction

; Waits until the player has left asStartCell and has 3D again, for at most about 10 seconds
; of game time. Scripts don't run during the loading screen, so that time isn't counted.
Function WaitForArrival(Actor akPlayer, Cell akStartCell) global
	Int tries = 0
	While tries < 20 && (akPlayer.GetParentCell() == akStartCell || !akPlayer.Is3DLoaded())
		Utility.Wait(0.5)
		tries += 1
	EndWhile
EndFunction

; The countdown before energizing, iCountdownSeconds long (0 = none), in the eCountdownStyle:
;   gravJump       the grav-jump countdown box, shown by Transporter_SF.dll (Transporter_SFNative.ShowCountdown).
;                  The box ends with a short closing animation, so this waits 99/91 of the
;                  countdown. Falls back to notifications when the DLL can't show it.
;   notifications  "Transporter lock in 3", "2", "1"
Function Countdown() global
	Int seconds = SettingI("iCountdownSeconds", 3)
	If seconds <= 0
		Return
	EndIf
	If SettingE("eCountdownStyle", "gravJump") == "gravJump"
		String problem = Transporter_SFNative.ShowCountdown(seconds as Float)
		If problem == ""
			Utility.Wait((seconds as Float) * 99.0 / 91.0)
			Return
		EndIf
		Debug.Trace("Transporter_SF: countdown box unavailable (" + problem + "); using notifications", 1)
	EndIf
	String heading = SettingS("sCountdownText", "Transporter lock in")
	While seconds > 0
		Debug.Notification(heading + " " + seconds)
		Utility.Wait(1.0)
		seconds -= 1
	EndWhile
EndFunction

; The activation gesture: a vanilla idle (sPlayerIdle / sFollowerIdle), and/or a custom
; animation file played through an animation framework's global function
; (sAnimScript.sAnimPlayFunction(Actor, String file)), called by name so Transporter_SF
; compiles and runs without that framework installed. Waits fGestureSeconds.
Function PlayGestures(Actor[] akTravellers) global
	Int i = 0
	While i < akTravellers.Length
		If i == 0
			PlayGesture(akTravellers[i], "Player")
		Else
			PlayGesture(akTravellers[i], "Follower")
		EndIf
		i += 1
	EndWhile
	Utility.Wait(SettingF("fGestureSeconds", 2.0))
EndFunction

Function PlayGesture(Actor akActor, String asWho) global
	Idle gesture = FormSettingS("s" + asWho + "Idle") as Idle
	If gesture
		akActor.PlayIdle(gesture)
	EndIf
	String animFile = SettingS("s" + asWho + "AnimFile", "")
	String animScript = SettingS("sAnimScript", "")
	If animFile != "" && animScript != ""
		Var[] params = new Var[2]
		params[0] = akActor
		params[1] = animFile
		Utility.CallGlobalFunctionNoWait(animScript, SettingS("sAnimPlayFunction", "PlayAnimation"), params)
	EndIf
EndFunction

; Stops framework animations that loop until stopped (sAnimStopFunction(Actor)).
Function StopGestures(Actor[] akTravellers) global
	String animScript = SettingS("sAnimScript", "")
	String stopFunction = SettingS("sAnimStopFunction", "")
	If animScript == "" || stopFunction == ""
		Return
	EndIf
	Int i = 0
	While i < akTravellers.Length
		Var[] params = new Var[1]
		params[0] = akTravellers[i]
		Utility.CallGlobalFunctionNoWait(animScript, stopFunction, params)
		i += 1
	EndWhile
EndFunction

; Plays one phase ("Out" or "In") on every traveller and returns the lights it placed,
; for the caller to delete once the phase is over.
ObjectReference[] Function BeamPhase(Actor[] akTravellers, String asPhase, Bool abFx) global
	ObjectReference[] lights = new ObjectReference[akTravellers.Length]
	If !abFx
		Return lights
	EndIf
	ImageSpaceModifier screenEffect = FormSettingS("s" + asPhase + "Imod") as ImageSpaceModifier
	If screenEffect
		screenEffect.Apply(1.0)
	EndIf
	Form lightBase = FormSettingS("s" + asPhase + "Light")
	Float lightHeight = SettingF("fLightHeight", 64.0)
	Int i = 0
	While i < akTravellers.Length
		If lightBase
			lights[i] = akTravellers[i].PlaceAtMe(lightBase, 1, False, False, True, None, None, False)
			If lights[i]
				lights[i].MoveTo(akTravellers[i], 0.0, 0.0, lightHeight, False, False)
			EndIf
		EndIf
		PlayBeamEffects(akTravellers[i], asPhase)
		i += 1
	EndWhile
	Return lights
EndFunction

Function DeleteAll(ObjectReference[] akRefs) global
	Int i = 0
	While i < akRefs.Length
		If akRefs[i]
			akRefs[i].Delete()
		EndIf
		i += 1
	EndWhile
EndFunction

; The per-actor slots: sOutShader, sOutSpell, sOutSound / sInShader, sInSpell, sInSound.
Function PlayBeamEffects(Actor akActor, String asPhase) global
	If akActor == None
		Return
	EndIf
	EffectShader shader = FormSettingS("s" + asPhase + "Shader") as EffectShader
	If shader
		shader.Play(akActor, SettingF("f" + asPhase + "ShaderSeconds", 2.0))
	EndIf
	Spell applier = FormSettingS("s" + asPhase + "Spell") as Spell
	If applier
		applier.Cast(akActor, akActor)
	EndIf
	WwiseEvent beamSound = FormSettingS("s" + asPhase + "Sound") as WwiseEvent
	If beamSound
		beamSound.Play(akActor, None, None)
	EndIf
EndFunction

; =====================================================================================
; Transporter menu (OSF UI view transporter-sf/menu) and its OSF Settings hotkey
;   The page talks to Transporter_SF.dll, not to this script directly: OSF UI 2.0.0's own Papyrus
;   functions (OSFUI.Open, OSFUI.RegisterRequestStatic...) stop being bound once a save
;   loads, so the DLL registers the page's endpoints and calls back in here:
;     "targets" request -> AnswerTargets(token) -> Transporter_SFNative.ReplyTargets
;     "beam" send       -> the DLL closes the view -> BeamFromMenu(target)
;   The "Open the transporter" hotkey is registered by the DLL with OSF Settings' C++
;   interface, once for the whole run, and opens the view directly. Nothing calls into this
;   script while a save loads.
;   StartSession  diagnostics only:  cgf "Transporter_SF.StartSession" "verbose"
; =====================================================================================

; The build of these scripts; Transporter_SF.dll reports its own through Transporter_SFNative.DllVersion.
; Keep it equal to set_version in xmake.lua.
String Function ScriptVersion() global
	Return "0.7.0"
EndFunction

; "" when the scripts and Transporter_SF.dll are from the same build, otherwise what differs. The
; DLL's version is "" when Transporter_SFNative.pex is from an older build.
String Function BuildMismatch() global
	String dll = Transporter_SFNative.DllVersion()
	If dll == ScriptVersion()
		Return ""
	EndIf
	Return "Transporter-SF files from different builds: scripts " + ScriptVersion() + ", Transporter_SF.dll " + dll + ". Recompile and copy both .pex files."
EndFunction

; Prints the state of the menu's connections:  cgf "Transporter_SF.StartSession" "verbose"
; (Named for older builds, where it registered things after each load.)
Function StartSession(String asArgs = "") global
	String report = "Transporter_SF: script " + ScriptVersion() + ", DLL " + Transporter_SFNative.DllVersion() + ", hotkey " + Transporter_SFNative.HotkeyRegistered() + ", menu bridge connected " + Transporter_SFNative.IsAvailable() + ", OSF Settings ready " + OSFSettings.IsReady()
	If asArgs == "verbose"
		Print(report)
	Else
		Debug.Trace(report, 0)
	EndIf
	String mismatch = BuildMismatch()
	If mismatch != ""
		Debug.Notification(mismatch)
	EndIf
EndFunction

; Also a console command: TransporterMenu (or cgf "Transporter_SF.TransporterMenu" "").
Function TransporterMenu(String asArgs = "") global
	String reason = TransporterBlockedReason()
	If reason != ""
		Debug.Notification(reason)
		Return
	EndIf
	If !Transporter_SFNative.OpenView(ViewID())
		If Transporter_SFNative.IsAvailable()
			Debug.Notification("Transporter menu unavailable: OSF UI doesn't know the view " + ViewID())
		Else
			Debug.Notification("Transporter menu unavailable: Transporter_SF.dll isn't connected to OSF UI (see Transporter_SF.log)")
		EndIf
	EndIf
EndFunction

; The page asked for the ship and local points of interest; the DLL holds its request.
Function AnswerTargets(String asToken) global
	Transporter_SFNative.ReplyTargets(asToken, ListTargets())
EndFunction

; The page's Energize button: the DLL has already closed the view.
Function BeamFromMenu(String asTarget) global
	Utility.Wait(0.1) ; game time: continues once the menu has closed and the game runs again
	String reason = TransporterBlockedReason()
	If reason != ""
		Debug.Notification(reason)
		Return
	EndIf
	Teleport(asTarget)
EndFunction

; Why the transporter can't be used right now, or "" when it can.
String Function TransporterBlockedReason() global
	Actor player = Game.GetPlayer()
	If SettingB("bBlockInCombat", True) && player.IsInCombat()
		Return "Transporter unavailable in combat"
	EndIf
	Return ""
EndFunction

; The pilot seat of the home ship, or the ship itself when the seat isn't found.
ObjectReference Function ShipDestination() global
	ObjectReference seat = CassiopeiaPapyrusExtender.GetPlayerHomeSpaceshipPilotSeatRef()
	If seat
		Return seat
	EndIf
	Return Game.GetPlayerHomeSpaceShip()
EndFunction

; The menu's rows, as "|"-separated strings (format in main.js):
;   meta|units per meter|blocked reason|area name
;   ship|ship|distance|available 1/0|note|ship name
;   poi|hex id|distance|1|note|marker name        (nearest first)
; Aboard your ship, points of interest are looked for around the ship.
String[] Function ListTargets() global
	Actor player = Game.GetPlayer()
	SpaceshipReference homeShip = Game.GetPlayerHomeSpaceShip()
	Bool aboard = homeShip != None && player.GetCurrentShipRef() == homeShip
	ObjectReference anchor = player
	If aboard
		anchor = homeShip
	EndIf
	Float unitsPerMeter = SettingF("fUnitsPerMeter", 70.0)
	If unitsPerMeter <= 0.0
		unitsPerMeter = 70.0
	EndIf

	String area = ""
	Location here = anchor.GetCurrentLocation()
	If here
		area = CassiopeiaPapyrusExtender.GetTESFullName(here)
	EndIf
	If aboard
		If area != ""
			area = " - " + area
		EndIf
		area = "Aboard " + ShipName(homeShip) + area
	EndIf

	String[] lines = new String[0]
	String blocked = BuildMismatch() ; shown in the menu, so a stale .pex can't go unnoticed
	If blocked == ""
		blocked = TransporterBlockedReason()
	EndIf
	lines.Add("meta|" + unitsPerMeter + "|" + blocked + "|" + ScriptVersion() + "|" + area)
	lines.Add(ShipLine(player, homeShip, aboard))
	AddPoiLines(lines, anchor, unitsPerMeter)
	Return lines
EndFunction

String Function ShipName(SpaceshipReference akShip) global
	String name = CassiopeiaPapyrusExtender.GetReferenceName(akShip)
	If name == ""
		name = "Home ship"
	EndIf
	Return name
EndFunction

; Whether the ship row can be used follows eShipRule: landed, samePlanet or anywhere.
String Function ShipLine(Actor akPlayer, SpaceshipReference akShip, Bool abAboard) global
	If akShip == None
		Return "ship|ship|-1|0|You have no home ship.|No home ship"
	EndIf
	If abAboard
		Return "ship|ship|-1|0|You are aboard.|" + ShipName(akShip)
	EndIf
	Bool sameSpace = CassiopeiaPapyrusExtender.IsInSameSpace(akPlayer, akShip)
	Planet playerPlanet = akPlayer.GetCurrentPlanet()
	Bool samePlanet = playerPlanet != None && playerPlanet == akShip.GetCurrentPlanet()
	Int units = -1
	If sameSpace
		units = akPlayer.GetDistance(akShip) as Int
	EndIf

	String rule = SettingE("eShipRule", "samePlanet")
	Bool available = samePlanet
	String shipNote = ""
	If rule == "anywhere"
		available = True
		If !samePlanet
			shipNote = "Not on or above this planet: beaming across space is experimental."
		EndIf
	ElseIf rule == "landed"
		available = sameSpace
		If !available
			shipNote = "The ship is not landed in this area."
		EndIf
	ElseIf !available
		shipNote = "The ship is not on or above this planet."
	EndIf
	String digit = "0"
	If available
		digit = "1"
	EndIf
	Return "ship|ship|" + units + "|" + digit + "|" + shipNote + "|" + ShipName(akShip)
EndFunction

; Map markers around akAnchor, nearest first: those found by sMapMarkerBase in the loaded
; area, plus the map markers among the area's persistent references (most markers are
; persistent, so this also reaches ones outside the loaded cells). Each must have a map
; name and be in the same space as the anchor; bDiscoveredOnly also needs it on the map.
; The planet a reference is on, directly or through the location it is in (the Lodge ->
; Jemison); None in space or when neither knows.
Planet Function PlanetOf(ObjectReference akRef) global
	If !akRef
		Return None
	EndIf
	Planet found = akRef.GetCurrentPlanet()
	If !found
		Location where = akRef.GetCurrentLocation()
		If where
			found = where.GetCurrentPlanet()
		EndIf
	EndIf
	Return found
EndFunction

; Sort key for markers in another worldspace of the planet.
Float Function kElsewhere() global
	Return 1000000000.0
EndFunction

Function AddPoiLines(String[] akLines, ObjectReference akAnchor, Float afUnitsPerMeter) global
	Int maxCount = SettingI("iPoiMax", 40)
	If maxCount < 1
		maxCount = 1
	ElseIf maxCount > 100
		maxCount = 100
	EndIf
	Float radius = SettingF("fPoiRadius", 0.0) * afUnitsPerMeter ; 0 = no limit
	Bool discoveredOnly = SettingB("bDiscoveredOnly", True)

	ObjectReference[] found = new ObjectReference[0]
	Float[] distances = new Float[0]

	Form markerBase = FormSettingS("sMapMarkerBase")
	If markerBase
		Float searchRadius = radius
		If searchRadius <= 0.0
			searchRadius = 1000000.0
		EndIf
		ObjectReference[] nearby = akAnchor.FindAllReferencesOfType(markerBase, searchRadius)
		AddMarkers(found, distances, nearby, akAnchor, radius, discoveredOnly, maxCount)
	EndIf
	; The DLL reads the worldspaces' persistent cells: every map marker on this planet,
	; loaded or not, including other worldspaces of the planet (cities, landing sites).
	; Indoors (the Lodge, a ship's interior) the reference may have no planet of its own,
	; so it is taken from the location the anchor is in (the Lodge -> Jemison).
	Planet here = PlanetOf(akAnchor)
	If !here
		here = PlanetOf(Game.GetPlayer())
	EndIf
	Location planetLocation = None
	If here
		planetLocation = here.GetLocation()
	EndIf
	If Transporter_SFNative.IsAvailable()
		AddMarkers(found, distances, Transporter_SFNative.FindMapMarkers(planetLocation, akAnchor), akAnchor, radius, discoveredOnly, maxCount)
	EndIf
	Cell persistentCell = CassiopeiaPapyrusExtender.GetPersistentCell(akAnchor)
	If persistentCell
		AddMarkers(found, distances, CassiopeiaPapyrusExtender.GetReferencesInCell(persistentCell), akAnchor, radius, discoveredOnly, maxCount)
	EndIf

	Int i = 0
	While i < found.Length
		Int shown = distances[i] as Int
		String poiNote = ""
		If distances[i] >= kElsewhere()
			shown = -1 ; another worldspace of this planet: no meaningful distance
			poiNote = "elsewhere on this planet"
		EndIf
		akLines.Add("poi|" + CassiopeiaPapyrusExtender.GetHexFormID(found[i]) + "|" + shown + "|1|" + poiNote + "|" + CassiopeiaPapyrusExtender.GetMapMarkerName(found[i]))
		i += 1
	EndWhile
EndFunction

; Keeps akFound sorted by distance and at most aiMax long, without duplicates.
Function AddMarkers(ObjectReference[] akFound, Float[] afDistances, ObjectReference[] akCandidates, ObjectReference akAnchor, Float afRadius, Bool abDiscoveredOnly, Int aiMax) global
	If akCandidates == None
		Return
	EndIf
	Int c = 0
	While c < akCandidates.Length
		ObjectReference marker = akCandidates[c]
		If marker && CassiopeiaPapyrusExtender.GetMapMarkerName(marker) != "" && akFound.Find(marker) < 0
			If !abDiscoveredOnly || marker.IsMapMarkerVisible()
				Bool sameSpace = CassiopeiaPapyrusExtender.IsInSameSpace(marker, akAnchor)
				Float distance = kElsewhere() ; sorts after everything in this space
				If sameSpace
					distance = akAnchor.GetDistance(marker)
				EndIf
				If (afRadius <= 0.0 && (sameSpace || SettingB("bWholePlanet", True))) || (sameSpace && distance <= afRadius)
					Int at = 0
					While at < afDistances.Length && afDistances[at] <= distance
						at += 1
					EndWhile
					If at < aiMax
						akFound.Insert(marker, at)
						afDistances.Insert(distance, at)
						If akFound.Length > aiMax
							akFound.RemoveLast()
							afDistances.RemoveLast()
						EndIf
					EndIf
				EndIf
			EndIf
		EndIf
		c += 1
	EndWhile
EndFunction
