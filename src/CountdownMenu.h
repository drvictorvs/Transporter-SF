#pragma once

// The grav-jump countdown box, shown on foot before the transporter beams out.
//
// A small menu of our own (Interface/Transporter_SF_Countdown.swf) imports the game's
// GravJumpComponent.swf, or a replacement such as Grav Jump Component UI Overhaul. This
// code creates that file's DefaultGravJumpComponent and feeds it a pretend grav jump,
// the four values the ship HUD normally sends it, so the real box counts down with its
// own animation. Its sounds are played from here, since the box can't reach the game's
// audio outside the ship HUD.
namespace CountdownMenu
{
	// Call at kPostDataLoad.
	bool Register();

	// Whether Register() succeeded (the game's UI knows the menu).
	bool IsRegistered();

	// Shows the box counting down from a_seconds (it reads 00:00 at the end, then plays its
	// closing frames and goes). If it is already showing, it starts again. Returns nullptr
	// when the menu was asked to open, or why it couldn't be.
	const char* Show(float a_seconds);

	// How long the whole box stays up for a countdown of a_seconds, so the script can wait it out.
	float TotalSeconds(float a_seconds);
}
