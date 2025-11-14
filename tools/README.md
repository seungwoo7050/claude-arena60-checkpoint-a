# Arena60 Test Tools

Testing utilities for Arena60 game server.

---

## Test Client (`test_client.py`)

Python WebSocket client for testing game server functionality.

### Features

- **Automated gameplay simulation** - Sends random movement and fire inputs
- **Multiple concurrent clients** - Stress test with multiple players
- **Real-time output** - Displays sent/received messages
- **Configurable** - Customize host, port, player ID, duration

### Prerequisites

```bash
# Python 3.7+
python3 --version

# Install websockets library
pip install websockets
```

### Basic Usage

**Single client (5 seconds)**:
```bash
python tools/test_client.py
```

Output:
```
============================================================
Arena60 Test Client
============================================================
Server: localhost:8080
Clients: 1
Duration: 5.0s
============================================================

[player1] Connected to ws://localhost:8080
[player1] -> input player1 0 1 0 0 0 150.5 200.3
[player1] <- state player1 100.0 200.0 0.0 60
[player1] -> input player1 1 0 0 1 0 145.2 195.8
[player1] <- state player1 105.0 200.0 0.0 61
...
[player1] Sent 312 inputs in 5.0s
[player1] Disconnected
```

### Command-Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--host` | `localhost` | Server hostname or IP |
| `--port` | `8080` | WebSocket port |
| `--player` | `player1` | Player ID |
| `--clients` | `1` | Number of concurrent clients |
| `--duration` | `5.0` | Test duration in seconds |

### Examples

**Custom player ID**:
```bash
python tools/test_client.py --player alice
```

**Different server**:
```bash
python tools/test_client.py --host 192.168.1.100 --port 9000
```

**Longer test**:
```bash
python tools/test_client.py --duration 30
```

**Stress test (10 concurrent clients)**:
```bash
python tools/test_client.py --clients 10
```

Output:
```
============================================================
Arena60 Test Client
============================================================
Server: localhost:8080
Clients: 10
Duration: 5.0s
============================================================

Starting 10 concurrent clients...
[player1] Connected to ws://localhost:8080
[player2] Connected to ws://localhost:8080
[player3] Connected to ws://localhost:8080
...
[player1] -> input player1 0 1 0 0 0 150.5 200.3
[player2] -> input player2 0 0 1 0 0 120.1 180.9
...
[player1] Sent 312 inputs in 5.0s
[player2] Sent 312 inputs in 5.0s
...

All 10 clients finished
```

**Combine options**:
```bash
python tools/test_client.py --host localhost --port 8080 --clients 20 --duration 10
```

### Input Simulation

The test client simulates realistic player behavior:

**Movement** (30% chance per key):
- `up` (W key): 1 if pressed, 0 if released
- `down` (S key): 1 if pressed, 0 if released
- `left` (A key): 1 if pressed, 0 if released
- `right` (D key): 1 if pressed, 0 if released

**Mouse Position**:
- Random movement within bounds (0-500, 0-500)
- Smooth changes (±10 units per input)

**Input Rate**:
- ~60 inputs per second (16ms interval)
- Matches typical game client behavior

### Protocol

**Client → Server**:
```
input <player_id> <seq> <up> <down> <left> <right> <mouse_x> <mouse_y>
```

Example:
```
input player1 42 1 0 0 0 150.5 200.3
```

**Server → Client**:
```
state <player_id> <x> <y> <angle> <tick>
death <player_id> <tick>
```

Example:
```
state player1 105.0 200.0 0.785 123
death player2 150
```

### Error Handling

**Connection refused**:
```
[player1] Connection refused. Is the server running?
```

→ Check if server is running: `docker ps` or `./arena60_server`

**Module not found**:
```
Error: websockets library not installed
Install with: pip install websockets
```

→ Install dependency: `pip install websockets`

**WebSocket error**:
```
[player1] WebSocket error: ...
```

→ Check server logs for details

### Use Cases

**1. Smoke Test**
```bash
# Quick test after building server
python tools/test_client.py --duration 5
```

**2. Movement Test**
```bash
# Verify player movement works
python tools/test_client.py --player alice --duration 10
```

**3. Combat Test**
```bash
# Run 2 players for combat scenario
python tools/test_client.py --player player1 --duration 20 &
python tools/test_client.py --player player2 --duration 20
```

**4. Load Test**
```bash
# Test server under load (10 concurrent clients)
python tools/test_client.py --clients 10 --duration 30
```

**5. Endurance Test**
```bash
# Long-running test (5 minutes)
python tools/test_client.py --duration 300
```

### Interpreting Output

**Normal operation**:
```
[player1] -> input player1 0 1 0 0 0 150.5 200.3
[player1] <- state player1 105.0 200.0 0.0 61
```
→ Server responding normally, player position updating

**No response**:
```
[player1] -> input player1 0 1 0 0 0 150.5 200.3
(no state received)
```
→ Check server logs, possible crash or deadlock

**Death event**:
```
[player1] <- death player1 150
```
→ Player died (combat system working)

**High latency**:
```
[player1] -> input player1 0 1 0 0 0 150.5 200.3
(2 second delay)
[player1] <- state player1 105.0 200.0 0.0 61
```
→ Server overloaded or network issue

### Limitations

- **No combat logic**: Client doesn't aim at enemies or dodge
- **No matchmaking**: Directly connects to WebSocket (no matchmaking API)
- **Text protocol only**: Uses text frames, not binary Protocol Buffers
- **No state validation**: Doesn't verify server responses

### Integration with CI/CD

**GitHub Actions example**:
```yaml
- name: Test game server
  run: |
    ./arena60_server &
    sleep 2
    python tools/test_client.py --duration 10
    killall arena60_server
```

**Docker Compose**:
```yaml
services:
  server:
    build: ./server
    ports:
      - "8080:8080"

  test:
    image: python:3.9
    command: >
      sh -c "pip install websockets &&
             python /tools/test_client.py --host server --clients 5"
    volumes:
      - ./tools:/tools
    depends_on:
      - server
```

---

## Alternative: wscat (Manual Testing)

For manual, interactive testing without Python:

```bash
# Install
npm install -g wscat

# Connect
wscat -c ws://localhost:8080

# Send input (manual)
> input player1 0 1 0 0 0 150.5 200.0

# Receive state (automatic)
< state player1 105.0 200.0 0.0 60
```

**Advantages**:
- No Python required
- Interactive (full control)
- Good for debugging

**Disadvantages**:
- Manual (not automated)
- One client at a time
- No stress testing

---

## Future Tools

Planned testing utilities:

- **`test_matchmaking.py`** - Test matchmaking API endpoints
- **`test_http.py`** - Test HTTP profile/leaderboard endpoints
- **`test_combat.py`** - Scripted combat scenarios
- **`benchmark.py`** - Automated performance benchmarking
- **`replay_viewer.py`** - View recorded game sessions (Checkpoint B)

---

## Contributing

To add new test tools:

1. Create script in `tools/` directory
2. Add Python shebang: `#!/usr/bin/env python3`
3. Make executable: `chmod +x tools/your_script.py`
4. Document in this README
5. Add usage example to main README

---

## License

Same as main project (portfolio demonstration).
