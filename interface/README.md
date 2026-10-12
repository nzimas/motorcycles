# Display-independent interface core

This is a portable state-machine implementation, not a installed firmware hook.
The adapter must translate verified PAGE, separate PadEvent, step KeyEvent,
relative encoder events and a UI-task clock into `mc_event_handle`.

PAGE-first + pads 1–4 selects Patch, Kit, Phrase or Pattern. The first selection
captures the track and later generator changes retain it. Steps 1–4 select
Abstract, Rhythmic, Melodic or Free. A selecting press cannot generate until
released; a fresh press of that selected step starts a 600 ms confirmation.
Each completed hold invokes publication once. Releasing PAGE after choosing a
generator keeps that generator active; tap PAGE again to exit and release LED
ownership. Owned pad/step releases remain consumed after exit. After a successful
Patch generation, briefly tapping and releasing the blinking selected path
auditions the current generated sound; holding it for 600 ms regenerates.
Audition runs only while the sequencer is fully stopped, and its note releases
after 150 ms or immediately when leaving the generator.

Density defaults by path are 25/45/35/40%; Brutality 40/55/0/45%.
Every generator/path has its own values. They persist during the session only;
`mc_init` resets them at boot. Density is active only for Phrase/Pattern.
Brutality is inactive in Melodic and in Abstract sequence generation.
Both controls use signed relative deltas clamped to 0–100; inactive controls
are consumed without changing values. The verified mapping is physical Pitch
(encoder index 2) for Density and Amp Decay (encoder index 3) for Brutality.
Actual interception and ordinary-control suppression remain adapter work.

The LED function returns a step on/off mask. Path LEDs remain available and
the selected path alternates at 400 ms per phase. A parameter adjustment adds
a rounded 12-segment count across steps 5–16 for one second. Success lights
these 12 steps for 800 ms; failure alternates a distinct pattern. No brightness
capability is assumed. Stock LED painting must be overridden in its verified
frame ownership mechanism while active and restored after exit.

Publication callback success means a complete transaction was committed.
Failure must preserve prior content. The firmware adapter also owes a brief
supplementary display message. This core does not claim audio publication,
display rendering, ordinary dispatch suppression, or hardware LED validation.

`controller.c` joins this state machine to the generator with a fresh seed per
attempt and caller-owned staging memory. A required platform commit callback
defines the publication boundary. Without it, confirmation fails safely.
The integrated host test uses semantic storage and proves all 16 paths, scope,
fresh repeated holds and failed-commit preservation; it is not a firmware commit.
