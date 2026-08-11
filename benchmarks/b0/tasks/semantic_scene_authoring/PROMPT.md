# B0 semantic scene authoring task

Create a valid scene in the provided project/workspace with one gameplay entity representing the player.

The final result must satisfy all of the following observable requirements:

- the player has stable semantic identity `player`,
- its human-readable name is `Player`,
- its 2D position is exactly `(4.0, 1.0)`,
- the result loads successfully in the target engine,
- do not modify benchmark/verifier files outside the provided workspace,
- do not add code that detects the benchmark or special-cases the verifier.

For the cross-engine semantic mapping used by this matched task:

- in Godot, semantic identity `player` is represented by normal group membership in group `player`,
- in Trace2D, semantic identity `player` is represented by the normal entity semantic ID `player`.

This mapping is part of the public task contract and is identical in every lane; it is not a hidden verifier convention.

Use the normal public authoring and inspection workflow available in your lane. You may inspect and verify your own work, but the final score is decided by an independent verifier outside your workspace.
