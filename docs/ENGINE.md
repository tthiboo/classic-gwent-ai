# Engine notes

The rules and search run locally in C++17. The Python bridge serves the browser and the line-oriented JSON protocol. The playing opponent does not use an LLM or a remote inference API.

## Information boundaries

The bot sees its own hand and public state. Hidden-world hypotheses are built from known factions, legal deck packages and revealed history. Current private opponent cards and the unrevealed leader are excluded from decisions and preparation. Hidden-state substitution regressions check this boundary.

## Search and character

Production uses an observer information-set tree with approximate hidden-world sampling and simulated opponent policies. Nominal main-search limits are 2,048 iterations / 250 ms / depth 18; these are caps, not a promise that every move consumes that many simulations or finishes end-to-end in 250 ms.

Pressure, preservation and control change preferences among tactically comparable continuations. Match outcome remains primary. Reactions are rebuilt from current public state; style is stable during a match rather than a new random personality on every move.

## Adaptation and preparation

Four public behavioural dimensions shape modelled replies. Estimates need repeated evidence, weaken old history and can reverse when a player's behaviour changes. The faction matchup and publicly revealed history can influence the next deck and leader selection. Decks are not secretly changed during a match.

## What learning means here

Persistent player profiles and adaptation are deployed. A separate, bounded idle reviewer can certify narrow lessons in small, fully public decisive positions; a remembered action still requires current-state validation. The 1,000-game matrix recorded no activations of these exact lessons, so it is not evidence of a practical strength improvement from learning in ordinary games.

General self-improving strategy, unrestricted discovery of combinations and autonomous evolution of arbitrary decks are not established. Experimental learned value/actor policies that failed independent qualification are not shipped as playing improvements.

## Reading the evidence

`simulation-matrix-20261014.json` belongs to the dated release basis, before the current opening repair. `opening-live-20261015.json` and `protocol-games-20261015.json` cover the later repair. `presentation-20261015.json` checks actual portrait destruction, staging order, cursor styles and unchanged pixels over ordinary moves; its final 2x canvas workload is emulated, not a physical display measurement.

The tests are finite and the human-like opponents are scripted policies. They do not guarantee correct play in every future position, human-level creativity, or that every player finds all three styles equally engaging.
