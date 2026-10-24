# Release verification

- x86 DLL loaded, exports/interface/version/instance checks passed.
- All 40 race/armor routes and 80 up/down variant leases passed, including wrong-actor/armor/race rejection and native Tarutaru sharing.
- Asset bounds, lifetime and 10,000 concurrent toggle reads passed against the shipped DATs.
- Peer update, gesture timing/layer, saved Animate change in fresh processes, stale cast-bar, movement/jitter, and unload/failed-detach regression checks passed.
- Real native hook/trampoline fixtures and 100 full-DAT parser calls passed.
- All 35 shipped asset pairs match the user's in-game-approved test set byte-for-byte; seven gesture packages are included.
- Production binary scan found no old diagnostic loading text, race/armor preview UI or Apply preview/Restore appearance controls.
- Release has no editor state, absolute export-path configuration, game logs or backups. The raw build log is retained privately outside the repository package.

Native tests verify the release cleanup. The newly built DLL has not been installed into a fresh live game session during packaging.
