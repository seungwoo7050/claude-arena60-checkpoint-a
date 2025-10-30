# Arena60 - Production Battle Arena Games

**Phase 2** of Arena60 project

## Status

- [ ] Checkpoint A: 1v1 Duel Game
- [ ] Checkpoint B: 60-player Battle Royale
- [ ] Checkpoint C: Esports Platform

## Quick Start

```bash
# Start infrastructure
cd deployments/docker
docker-compose up -d

# Build server
cd server
mkdir build && cd build
cmake ..
make

# Run tests
ctest

# Run server
./arena60_server
```

## Documentation

See `docs/` for detailed specifications.
