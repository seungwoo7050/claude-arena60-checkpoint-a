# Arena60 WebSocket Protocol Specification

**Version**: 1.0.0
**Last Updated**: 2025-01-30
**Status**: Checkpoint A Complete

---

## Overview

Arena60 uses **text-based WebSocket frames** for client-server communication. All messages are UTF-8 encoded strings with space-separated fields.

---

## Client → Server Messages

### **Input Frame**

Sends player input state to the server.

**Format**:
```text
input <player_id> <sequence> <up> <down> <left> <right> <mouse_x> <mouse_y> [fire]
```

**Fields**:
| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `player_id` | string | Unique player identifier | `player1` |
| `sequence` | uint64 | Monotonic input sequence number | `42` |
| `up` | int | W key pressed (1) or released (0) | `1` |
| `down` | int | S key pressed (1) or released (0) | `0` |
| `left` | int | A key pressed (1) or released (0) | `0` |
| `right` | int | D key pressed (1) or released (0) | `1` |
| `mouse_x` | double | Mouse X position (game coordinates) | `150.5` |
| `mouse_y` | double | Mouse Y position (game coordinates) | `200.3` |
| `fire` | int | **Optional** - Mouse button pressed (1) or released (0) | `1` |

**Examples**:
```text
input player1 0 1 0 0 0 150.5 200.3
input player2 1 0 1 0 0 120.0 180.0 1
input attacker 5 1 0 0 1 200.0 150.0 1
```

**Notes**:
- `fire` field is **optional** for backward compatibility
- If `fire` is omitted, it defaults to `0` (not firing)
- Server applies input in the next tick

**Implementation**: `server/src/network/websocket_server.cpp:166-194`

---

## Server → Client Messages

### **State Frame**

Broadcasts current game state to all connected clients.

**Format**:
```text
state <player_id> <x> <y> <facing_radians> <tick> <delta> <health> <is_alive> <shots_fired> <hits_landed> <deaths>
```

**Fields**:
| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `player_id` | string | Player identifier | `player1` |
| `x` | double | X position (meters) | `100.5` |
| `y` | double | Y position (meters) | `200.3` |
| `facing_radians` | double | Facing angle in radians | `1.57` |
| `tick` | uint64 | Current server tick number | `60` |
| `delta` | double | Delta time since last tick (seconds) | `0.0167` |
| `health` | int | Current health points | `80` |
| `is_alive` | int | Alive (1) or dead (0) | `1` |
| `shots_fired` | uint32 | Cumulative shots fired | `10` |
| `hits_landed` | uint32 | Cumulative hits landed | `5` |
| `deaths` | uint32 | Cumulative deaths | `0` |

**Example**:
```text
state player1 105.0 200.0 1.57 61 0.0167 80 1 10 5 0
```

**Broadcast Frequency**: Every tick (~60 Hz)

**Implementation**: `server/src/network/websocket_server.cpp:63-70`

---

### **Death Event**

Notifies all clients when a player dies.

**Format**:
```text
death <player_id> <tick>
```

**Fields**:
| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `player_id` | string | Player who died | `defender` |
| `tick` | uint64 | Tick when death occurred | `150` |

**Example**:
```text
death defender 150
```

**Timing**: Sent immediately after death detection

**Implementation**: `server/src/network/websocket_server.cpp:72-76`

---

## Message Flow Example

### Combat Scenario

```text
# Client connects
[Client → Server]
input player1 0 0 0 0 0 100.0 100.0

# Server broadcasts initial state
[Server → Client]
state player1 100.0 100.0 0.0 1 0.0167 100 1 0 0 0

# Client moves and fires
[Client → Server]
input player1 1 1 0 0 0 150.5 200.3 1

# Server updates state (shot fired)
[Server → Client]
state player1 105.0 105.0 0.785 2 0.0167 100 1 1 0 0

# Hit detected (another player)
[Server → Client]
state player2 200.0 200.0 3.14 2 0.0167 80 1 0 0 0

# Player2 dies after 5 hits
[Server → Client]
death player2 10
state player2 200.0 200.0 3.14 10 0.0167 0 0 0 0 1
```

---

## Protocol Constants

| Constant | Value | Source |
|----------|-------|--------|
| Tick Rate | 60 TPS | `game_loop.cpp:10` |
| Damage per Hit | 20 HP | `game_session.cpp:16` |
| Max Health | 100 HP | `player_state.h:14` |
| Projectile Speed | 30 m/s | `projectile.h:39` |
| Projectile Lifetime | 1.5 s | `projectile.h:40` |
| Fire Cooldown | 0.1 s | `game_session.cpp:14` |

---

## Error Handling

**Invalid Input Frame**:
```cpp
// Server logs and continues
std::cerr << "invalid input frame: " << data << std::endl;
// Client connection remains open
```

**Client Disconnection**:
```cpp
// Server removes player from session
// Other clients notified via state updates (player missing)
```

---

## Future Extensions (Checkpoint B+)

- **Binary Protocol Buffers** for efficiency
- **UDP** for state sync (low latency)
- **Delta compression** (reduce bandwidth)
- **Skill activation messages**
- **Item pickup notifications**

---

## Testing

**Manual Test (wscat)**:
```bash
npm install -g wscat
wscat -c ws://localhost:8080

> input player1 0 1 0 0 0 150.5 200.0
< state player1 100.0 200.0 0.0 60 0.0167 100 1 0 0 0
```

**Automated Test**:
```bash
python tools/test_client.py --player tester --duration 10
```

---

## References

- **Implementation**: `server/src/network/websocket_server.cpp`
- **Client Example**: `tools/test_client.py`
- **Integration Tests**: `server/tests/integration/test_websocket_server.cpp`
