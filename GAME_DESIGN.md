# Game Design

## Primary mode: Traffic Sprint

Traffic Sprint is a point-to-point highway race. Route progress is the primary ranking metric; finish time resolves completed races. The race may end on first finish, time expiry, or a ruleset-defined ending condition. The vertical slice uses a short solo route, countdown, finish trigger, results, and restart.

## Core loop

Select solo race → load route → countdown → weave through traffic → finish/results → restart or return. Online, lobbies and rematches are later phases.

## Initial controls

Keyboard and gamepad must both support accelerate, brake/reverse, steer, reset/recover, pause, and rear view. The exact mappings will use Enhanced Input and remain remappable later.

## Driving and collision intent

Automatic transmission, speed-sensitive steering, stability assistance, and recoverable low-speed errors favor responsive arcade handling. Light scrapes cost a little speed; severe impacts destabilize or trigger a safe delayed recovery. Permanent damage is not part of the MVP.

## Traffic behavior and fairness

Traffic follows authored lane splines with traffic profiles (desired speed, spacing, vehicle class, lane-change tendency). The prototype must never spawn an actor into the player’s immediate collision path; a configurable time-to-collision threshold and visible-lane availability check gate spawns. Lane changes must be gradual and signaled before crossing a player’s path.

## Near misses

A valid near miss is a close non-collision pass with sufficient relative speed and a completed overtake. A vehicle/encounter cooldown prevents duplicate awards. The vertical slice records count and shows feedback; score balancing and boost remain later work.

## HUD and UI flow

The prototype HUD displays speed, race phase/progress, remaining route distance, timer, and near-miss feedback. Menus, settings, results polish, and online UI follow the core game loop once driving is proven.
