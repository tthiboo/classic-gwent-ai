# Classic Gwent AI

A non-commercial fan prototype inspired by the classic Gwent rules in **The Witcher 3: Wild Hunt**. Built out of love for the game, with particular attention to an opponent worth playing more than once.

**Unofficial fan work. Not approved or endorsed by CD PROJEKT RED.** This is a public source snapshot for technical review. Card illustrations, game UI artwork, logos, audio, fonts, personal match histories and saved player profiles are not included. There is no hosted demo in this repository.

## What is implemented

- Five classic factions and 22 leader abilities; a catalogue of 162 card definitions, including non-collectible forms/tokens. Artwork variants are not separate definitions.
- A C++17 rules engine and a finite-budget information-set search opponent. It samples plausible hidden worlds rather than reading the opponent's real hidden hand.
- Three persistent styles: pressure, resource preservation and control. Style preferences operate inside tactical constraints rather than deliberately throwing games.
- Public-history adaptation: round investment, pressure after winning round one, flexible-row placement and the timing of useful counters.
- Between-game preparation: deck composition and leader choice respond to the known opposing faction and previously revealed behaviour. The player's current unrevealed leader is not an input.
- Local persistent profiles and narrowly verified endgame lessons. This is **not general self-learning**; rejected learned policies are not deployed.
- A browser interface, per-faction deck/leader persistence, card inspection, revival/mulligan galleries and weather/destruction effects. Original visual assets must be supplied separately with appropriate permission.

## Build and inspect the engine

Requirements: Python 3.10+ and a C++17 compiler (`c++`). The HTTP server uses the Python standard library. No paid inference service, API key or external model is required to play.

```sh
python3 tools/build.py
python3 tools/smoke.py
```

The smoke check starts a separate engine **without a memory path**, creates a game and checks the build identity and initial hand. It does not touch an existing game or player profile.

To run targeted regressions:

```sh
python3 tools/build.py --tests
python3 tools/smoke.py --tests
node test_presentation.cjs
node test_interface.cjs
node test_setup.cjs
```

## Browser client and assets

The web client and catalogue are included, but the graphical game is not a complete asset distribution. An authorised local asset bundle matching `web/assets/catalog.json` is required for its illustrated interface. The repository does not provide or download that bundle. Keep visual assets local and untracked.

With such assets available:

```sh
python3 server.py --port 8765
```

Open `http://127.0.0.1:8765`. The server binds to localhost and serves one shared match. It is a local prototype, not a multi-user production service. Profiles and lesson files are created locally and ignored by Git.

## Evidence, with dates and limits

The full 14 October 2026 simulation matrix covered **1,000 complete candidate games**, 25 faction pairings, both seats, three styles, ten simulated behaviour policies, eight deck themes, 778 distinct opponent decks and all 22 leaders. Another 500 reference games provided paired comparisons. These were simulations, not 1,000 human playtests.

The paired score was 418 versus 408.5 out of 500. This did **not** establish a statistically significant strength gain; it met the recorded strength-preservation criterion for that dated package. A later opening-only holdout failed its statistical threshold and is not described as a successful independent release test.

The current 15 October opening repair was checked separately: **210 targeted opening checks**, **60 full-budget opening decisions**, and **30 full protocol games with 1,138 legal actions and zero first-action passes**. The latter used 256 simulations per decision. The earlier 1,000 games are not relabelled as a new run of the current executable.

The normal search configuration caps its main search at 2,048 iterations, 250 ms and depth 18. Additional preparation, checking and presentation mean this is not a guaranteed end-to-end response time.

Aggregate evidence is in [`docs/evidence`](docs/evidence). See [`docs/ENGINE.md`](docs/ENGINE.md) for architecture and learning limitations. No universal optimality, invincibility or human-preference validation is claimed.

## Project layout

- `engine/`: rules, search, opponent model, preparation and regressions. Some historical filenames remain because production code composes these layers.
- `web/`: browser UI; metadata only under `web/assets/`.
- `server.py`: local game protocol and idle lesson-review scheduling.
- `tools/`: build, isolated smoke checks and bounded lesson review.
- `docs/`: aggregate evidence and technical notes, without personal gameplay data.

## Rights and contribution

The Witcher, GWENT, their characters and related trademarks belong to their respective rights holders. This project is not an official CD PROJEKT RED product. Public visibility is not permission to redistribute game artwork or to publish a derivative game. Any permitted public distribution of the illustrated prototype remains a question for the rights holder.

No general source-code licence has been selected for this snapshot. It is available for inspection on GitHub; do not assume an MIT/Apache licence or additional rights. This is a volunteer project, not a request for funding or royalties.
