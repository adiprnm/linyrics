/* ------------------------------------------------------------------
   Linyrics — one instance per session, and a way to poke it.

   Starting the app again must show/hide the running window instead of
   launching a second copy, and `--toggle` / `--quit` have to work from a
   hotkey. Both need a tiny bit of IPC:

     Linux/macOS: pid file in the runtime dir + SIGUSR1/SIGTERM
     Windows:     named mutex (single instance) + named events (signals)

   The Windows events are polled from the main loop, so the UI never
   waits on IPC.
   ------------------------------------------------------------------ */
#pragma once

/* Takes the single-instance lock. Returns false when another instance owns it. */
bool instance_start();

/* Sends "toggle window" (quit == false) or "quit" to the running instance.
   Returns true when it was delivered. */
bool instance_signal(bool quit);

/* Polls for pending signals (Win32). No-op on POSIX, where the signal handlers
   already set the flags. */
void instance_poll();

/* Returns true once per pending request. */
bool instance_take_toggle();
bool instance_take_quit();

/* Releases the lock / handles (also called on the way out). */
void instance_stop();
