# Test findings

All regular tests match the current project behavior and pass.

One implementation detail remains important: `RuleEngine` checks blockers against the logical board. A moving piece remains in its source cell until arrival, so it may block a new path even after its virtual position has changed.
