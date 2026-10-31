# Trigonometry Wars
## Player Manual

![Trigonometry Wars marquee](assets/images/marquee.png)

> Neon arena survival. Dodge, aim, fire, and survive the swarm.

---

## 1. Welcome

Trigonometry Wars is a fast-paced twin-stick arena shooter inspired by classic neon space survival games. You command a small fighter ship in a glowing geometric battlefield, fighting off waves of enemies while protecting yourself from incoming fire and colliding hazards.

Your mission is simple:

- move freely around the arena
- aim in any direction
- fire continuously at enemies
- avoid getting surrounded
- survive long enough to build a high score

---

## 2. Quick Start

### Build and run

```sh
make
make run
```

If needed, use a platform-specific build file:

```sh
make -f makefile.macos
make -f makefile.linux
make -f makefile.win
```

> Run the game from the project root so the assets and saved score file load correctly.

---

## 3. Controls

### Keyboard

| Action | Keys |
| --- | --- |
| Move ship | W, A, S, D |
| Aim | Arrow keys |
| Fire | Space |
| Back / cancel | Backspace |
| Pause | P |
| Options | O |
| Start player 1 | 1 |
| Start player 2 | 2 |
| Start player 3 | 3 |
| Start player 4 | 4 |

### Gamepad

| Action | Control |
| --- | --- |
| Move | Left stick or D-pad |
| Aim | Right stick |
| Fire | Trigger |
| Start / pause | Start |
| Back / cancel | Back |
| Options | Back |

### Control notes

- The D-pad works for both menu navigation and in-game movement.
- The analog sticks can be used in menus as well as in combat.
- The game supports up to four players and several connected controllers.

---

## 4. Core Gameplay

### Your ship

You control a glowing neon ship that moves on a 2D battlefield. It can:

- strafe around the arena
- aim at enemies in any direction
- shoot continuously while your aim is active
- survive enemy pressure and avoid collisions

### Objective

The goal is to survive as long as possible and maintain your score. Every enemy eliminated adds to your total and keeps the run alive.

### Enemy pressure

The arena is filled with multiple enemy classes, each with different movement patterns and threat levels. You must watch for:

- fast close-range attackers
- enemies that surround you
- gravity hazards and black holes
- swarm patterns that cut off escape routes

---

## 5. Enemy Types

The game includes many enemy varieties, such as:

- Grunts
- Wanderers
- Weavers
- Snakes
- Spinners
- Repulsors
- Mayflies
- Black holes and gravity wells

These enemies force you to stay mobile, keep aiming, and avoid panic movement.

---

## 6. Features and Modes

### Single-player

Classic survival mode where your focus is score, movement, and staying alive.

### Co-op mode

Multiple players can fight together in the same arena.

### Versus mode

Players can challenge each other in competitive play.

### Special mechanics

- Bombs
- Shields
- Score multipliers
- Gravity-well effects
- Dynamic enemy waves
- High-score tracking

---

## 7. Menus and Options

From the title screen, you can access:

- game mode selection
- top score screen
- graphics options

The graphics menu allows changes to:

- fullscreen
- window resolution
- glow / bloom
- smoothing
- vsync

These settings are saved to `settings.cfg`.

---

## 8. Best Practice Tips

### For beginners

- Keep moving; standing still is usually fatal.
- Learn the enemy patterns before going aggressive.
- Use the edges of the arena to reposition safely.
- Avoid getting boxed in by swarms.

### For better survival

- Aim slightly ahead of moving enemies.
- Keep your ship in motion while firing.
- Watch the danger zones created by black holes and repulsors.
- Use safe spacing to avoid accidental collisions.

---

## 9. Troubleshooting

### Game does not start

- Make sure SDL3 and OpenGL support are installed.
- Run from the project root.
- Rebuild using `make clean` and `make` if needed.

### Controls feel wrong

- Check whether you are using keyboard or controller input.
- Verify the controller is connected and detected.
- Use the menu controls consistently with Start and Back buttons.

### Graphics look blurry or off

- Open the graphics settings in the title menu.
- Adjust bloom, resolution, smoothing, and vsync.
- Restart the game after changing display settings.

---

## 10. Quick Reference

### Keyboard summary

- `W A S D` — move
- Arrow keys — aim
- `Space` — fire
- `P` — pause
- `Backspace` — back / cancel
- `O` — options
- `1` to `4` — start players

### Gamepad summary

- Left stick / D-pad — move
- Right stick / trigger — aim and fire
- Start — start / pause
- Back — menu back / options

---

## 11. Final Note

Trigonometry Wars is about movement, timing, and careful aim. The more you read the arena and adapt to enemy pressure, the longer you survive and the higher your score climbs.

Stay mobile, keep firing, and survive the neon storm.
