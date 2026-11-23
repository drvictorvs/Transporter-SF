ScriptName Transporter_SFNative Native Hidden
{Functions provided by Transporter_SF.dll. They connect the transporter menu to OSF UI from C++,
because OSF UI 2.0.0's own Papyrus functions stop being bound once a save loads.}

; True when the DLL found OSF UI 2.x and registered the page's endpoints.
Bool Function IsAvailable() global native

; Opens or closes an OSF UI view ("mod/view"); false when OSF UI doesn't know it
; or the DLL isn't connected.
Bool Function OpenView(String asViewId) global native
Bool Function CloseView(String asViewId) global native

; Answers the page's "targets" request held under asToken with a JSON array of the lines.
; False when that request was already answered or has timed out.
Bool Function ReplyTargets(String asToken, String[] akLines) global native

; Shows the grav-jump countdown box for afSeconds. Returns "" when it opened, otherwise why not.
String Function ShowCountdown(Float afSeconds) global native

; Map-marker references from the worldspaces' persistent cells whose location is under
; akPlanetLocation (the planet's Location). With None, those of akAnchor's worldspace.
ObjectReference[] Function FindMapMarkers(Location akPlanetLocation, ObjectReference akAnchor) global native

; The DLL's version ("0.7.0"); Transporter_SF.StartSession compares it with the script's.
String Function DllVersion() global native

; True when the DLL registered the "Open the transporter" hotkey with OSF Settings.
Bool Function HotkeyRegistered() global native
