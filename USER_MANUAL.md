# Trigonometry Wars
## User Manual

![Trigonometry Wars marquee](assets/images/marquee.png)

Trigonometry Wars is a fast, neon-lit arena shooter inspired by classic twin-stick action games. You pilot a glowing ship across a vector-style grid, survive enemy waves, collect score, and chase a high score.

---

## 1. Game overview

You are a small spacecraft fighting through waves of enemies in a brightly lit arena. Your goals are simple:

- Move freely around the arena
- Aim and fire in any direction
- Dodge incoming threats
- Survive as long as possible
- Earn points by eliminating enemies and chaining survival

The game supports:

- Single-player arcade mode
- Multiplayer co-op play
- Multiplayer versus matches
- Keyboard and gamepad controls
- Persistent high scores and attract mode

---

## 2. How to start the game

From the project root, build and run the game:

```sh
make
make run
```

You can also use the platform-specific build files if needed:

```sh
make -f makefile.macos
make -f makefile.linux
make -f makefile.win
```

Important:

- Run the game from the repository root so the game can load assets such as images, sounds, and the saved score file correctly.
- The executable is created as `./trigwars`.

---

## 3. Controls

### Keyboard controls

| Action | Key |
| --- | --- |
| Move ship | W, A, S, D |
| Aim / fire | Arrow keys + Space |
| Start player 1 | 1 |
| Start player 2 | 2 |
| Start player 3 | 3 |
| Start player 4 | 4 |
| Back / cancel | Backspace |
| Pause | P |
| Options | O |

### Gamepad controls

| Action | Control |
| --- | --- |
| Move | Left stick or D-pad |
| Aim / fire | Right stick or trigger |
| Start player | Start button |
| Back / cancel | Back button |
| Pause | Pause / Start |
| Options | Back button |

Notes:

- The D-pad works for both menu navigation and in-game movement.
- The analog sticks can also be used for menu selection.
- The game supports up to four connected controllers.

---

## 4. Main gameplay

### Objective

Survive enemy attacks, avoid collisions, and clear waves as long as possible. Every enemy you eliminate contributes to your score and pushes your survival run higher.

### Core actions

- Move the ship with the left stick or `WASD`
- Aim with the right stick or arrow keys
- Fire with the trigger or `Space`
- Use bomb or special defensive actions when available
- Keep moving to avoid getting trapped by swarms

### Enemy types

The game includes many enemies with different movement and threat patterns, including:

- Grunts
- Wanderers
- Weavers
- Snakes
- Spinners
- Repulsors
- Mayflies
- Black holes and gravity wells

These enemies create pressure, forcing you to keep moving and aim accurately.

---

## 5. Game modes and features

### Single-player

A classic survival run where you focus on staying alive and maximizing your score.

### Co-op

Up to multiple players can work together in the same arena against the incoming waves.

### Versus

Players can battle against each other in a competitive mode.

### Special abilities and hazards

The game includes several interactive mechanics:

- Bombs
- Shields
- Score multipliers
- Gravity wells
- Hazardous black holes
- Enemy waves with varied patterns

These systems increase difficulty and reward skillful play.

---

## 6. Menu and options

From the title screen, you can access options and high-score screens. You can also change graphics settings such as:

- Fullscreen
- Window size / resolution
- Glow or bloom effect
- Smoothing
- Vsync

These settings are saved to `settings.cfg`.

The title screen also provides access to:

- Game modes
- Top scores
- Graphics options

---

## 7. Tips for new players

- Keep moving at all times; staying still usually leads to a quick defeat.
- Learn the enemy patterns before committing to aggressive attacks.
- Use the edges of the arena to reposition when overwhelmed.
- Don’t overstay in a dangerous lane; keep your ship mobile.
- Aim slightly ahead of moving enemies when possible.
- In co-op or versus modes, coordinate with teammates and avoid cross-fire.

---

## 8. Troubleshooting

### The game will not start

- Make sure SDL3 and OpenGL support are installed.
- Run the game from the repository root.
- Rebuild with `make clean` and `make` if the executable is stale or broken.

### Controls feel wrong

- Check whether you are using keyboard or gamepad input.
- Make sure a controller is connected and recognized by SDL.
- On the title screen, use the correct start/back buttons for menu actions.

### Graphics look odd or blurry

- Open the graphics options from the title screen.
- Adjust bloom, smoothing, resolution, or vsync.
- Restart the game after changing display settings.

---

## 9. Quick reference

### Keyboard summary

- `W A S D` — move
- Arrow keys — aim
- `Space` — fire
- `P` — pause
- `Backspace` — back/cancel
- `O` — options
- `1` to `4` — start players

### Gamepad summary

- Left stick / D-pad — move
- Right stick / trigger — aim and fire
- Start — start/pause
- Back — menu back/options

---

## 10. Closing note

Trigonometry Wars rewards quick reflexes, smart movement, and controlled aiming. Whether you are playing solo or with friends, the key to success is understanding enemy patterns and maintaining constant motion.

Enjoy the run, chase the high score, and survive the neon storm.
