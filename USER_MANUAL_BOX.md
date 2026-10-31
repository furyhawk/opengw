# Trigonometry Wars
## Game Box Manual

![Trigonometry Wars marquee](assets/images/marquee.png)

> The neon arena shooter for fast reflexes, smart movement, and high-score chasing.

---

## Welcome to the Box

Trigonometry Wars is a fast, colorful arena shooter where you pilot a neon fighter ship across a geometric battlefield. Fight off swarms of enemies, survive deadly waves, and climb the leaderboard.

This manual is designed to feel like a retail game box insert: quick to scan, easy to understand, and ready for new players to jump in.

---

## What's in the Game

- 1 to 4 player support
- Keyboard and gamepad controls
- Single-player survival mode
- Co-op play
- Versus play
- Persistent high-score tracking
- Dynamic enemy waves and hazards
- Neon vector graphics and bloom effects

---

## Getting Started

### System requirements

- A modern PC or Mac
- SDL3 support
- OpenGL 3.3 core profile support
- A controller is optional but recommended

### Install and run

```sh
make
make run
```

If needed:

```sh
make -f makefile.macos
make -f makefile.linux
make -f makefile.win
```

> Run from the project root so the game can find the asset folders and score file.

---

## Controls

### Keyboard

| Function | Controls |
| --- | --- |
| Move | W, A, S, D |
| Aim | Arrow keys |
| Fire | Space |
| Pause | P |
| Back / cancel | Backspace |
| Options | O |
| Start player 1 | 1 |
| Start player 2 | 2 |
| Start player 3 | 3 |
| Start player 4 | 4 |

### Gamepad

| Function | Controls |
| --- | --- |
| Move | Left stick or D-pad |
| Aim | Right stick |
| Fire | Trigger |
| Pause / start | Start |
| Back / cancel | Back |
| Options | Back |

---

## How to Play

### Objective

Survive enemy waves, destroy incoming threats, and keep your ship alive as long as possible. The longer you last, the higher your score climbs.

### Basic flow

1. Start a game from the title screen.
2. Move with the left stick or WASD.
3. Aim with the right stick or arrow keys.
4. Fire continuously at enemies.
5. Watch your surroundings and avoid being boxed in.
6. Stay alive and build your score.

### Combat tips

- Keep moving at all times.
- Learn enemy patterns before chasing risky attacks.
- Use your edges to break dangerous lanes.
- Stay aware of hazards like black holes and repulsors.

---

## Game Modes

### Arcade / Survival

The classic mode. Fight off endless waves and chase a bigger score.

### Co-op

Multiple ships work together against the enemy swarm.

### Versus

Compete head-to-head with other players in a faster, more direct match.

---

## Features

- Neon vector graphics with bloom glow
- Varying enemy types and behavior
- Multiple hazard patterns
- Bombs, shields, and score multipliers
- Persistent high scores
- Adjustable graphics settings

---

## Options & Graphics

From the title screen, open the options menu to modify:

- fullscreen mode
- window resolution
- glow / bloom strength
- smoothing
- vsync

These choices are saved to `settings.cfg`.

---

## Troubleshooting

### The game won’t launch

- Make sure the project is built successfully with `make`.
- Run it from the repository root.
- Rebuild if needed using `make clean` followed by `make`.

### Controls not responding

- Confirm your keyboard or controller is connected.
- Check the menu inputs for start/back.
- Make sure the game is seeing the selected input device.

### Display looks wrong

- Open graphics settings from the title menu.
- Adjust bloom, smoothing, resolution, and vsync.
- Restart after changing the display mode.

---

## Quick Start Card

### Keyboard

- Move: `W A S D`
- Aim: Arrow keys
- Fire: `Space`
- Pause: `P`
- Back: `Backspace`
- Options: `O`

### Gamepad

- Move: Left stick / D-pad
- Aim and fire: Right stick / trigger
- Start / pause: Start
- Back: Back

---

## Final Warning

The arena is unforgiving. Survive the swarm, stay sharp, and keep the ship in motion. The best runs are built from calm decisions, fast reactions, and disciplined movement.

Enjoy the game.
