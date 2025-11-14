#!/usr/bin/env python3
"""
Arena60 Test Client

Simple WebSocket client for testing the Arena60 game server.
Connects to the server, sends input, and displays server responses.
"""

import asyncio
import argparse
import sys
from typing import Optional
import random

try:
    import websockets
except ImportError:
    print("Error: websockets library not installed")
    print("Install with: pip install websockets")
    sys.exit(1)


class Arena60Client:
    """WebSocket client for Arena60 game server."""

    def __init__(self, player_id: str, host: str = "localhost", port: int = 8080):
        self.player_id = player_id
        self.uri = f"ws://{host}:{port}"
        self.seq = 0

    async def connect_and_play(self, duration: float = 5.0):
        """Connect to server and simulate gameplay."""
        try:
            async with websockets.connect(self.uri) as websocket:
                print(f"[{self.player_id}] Connected to {self.uri}")

                # Start receiving task
                receive_task = asyncio.create_task(self._receive_loop(websocket))

                # Simulate gameplay
                await self._simulate_gameplay(websocket, duration)

                # Cancel receive task
                receive_task.cancel()
                try:
                    await receive_task
                except asyncio.CancelledError:
                    pass

                print(f"[{self.player_id}] Disconnected")

        except websockets.exceptions.WebSocketException as e:
            print(f"[{self.player_id}] WebSocket error: {e}")
        except ConnectionRefusedError:
            print(f"[{self.player_id}] Connection refused. Is the server running?")
        except Exception as e:
            print(f"[{self.player_id}] Unexpected error: {e}")

    async def _receive_loop(self, websocket):
        """Continuously receive and display server messages."""
        try:
            async for message in websocket:
                print(f"[{self.player_id}] <- {message}")
        except asyncio.CancelledError:
            pass

    async def _simulate_gameplay(self, websocket, duration: float):
        """Simulate player actions."""
        start_time = asyncio.get_event_loop().time()

        # Starting position and mouse
        mouse_x = random.uniform(100, 200)
        mouse_y = random.uniform(100, 200)

        action_count = 0

        while asyncio.get_event_loop().time() - start_time < duration:
            # Random movement (WASD)
            up = random.randint(0, 1) if random.random() < 0.3 else 0
            down = random.randint(0, 1) if random.random() < 0.3 else 0
            left = random.randint(0, 1) if random.random() < 0.3 else 0
            right = random.randint(0, 1) if random.random() < 0.3 else 0

            # Random mouse movement
            mouse_x += random.uniform(-10, 10)
            mouse_y += random.uniform(-10, 10)
            mouse_x = max(0, min(500, mouse_x))
            mouse_y = max(0, min(500, mouse_y))

            # Send input
            input_msg = f"input {self.player_id} {self.seq} {up} {down} {left} {right} {mouse_x:.1f} {mouse_y:.1f}"
            await websocket.send(input_msg)
            print(f"[{self.player_id}] -> {input_msg}")

            self.seq += 1
            action_count += 1

            # Wait before next input (60 inputs per second max)
            await asyncio.sleep(0.016)  # ~60 FPS

        print(f"[{self.player_id}] Sent {action_count} inputs in {duration:.1f}s")


async def run_single_client(player_id: str, host: str, port: int, duration: float):
    """Run a single client."""
    client = Arena60Client(player_id, host, port)
    await client.connect_and_play(duration)


async def run_multiple_clients(num_clients: int, host: str, port: int, duration: float):
    """Run multiple clients concurrently (stress test)."""
    print(f"Starting {num_clients} concurrent clients...")

    tasks = []
    for i in range(num_clients):
        player_id = f"player{i+1}"
        task = asyncio.create_task(run_single_client(player_id, host, port, duration))
        tasks.append(task)

    await asyncio.gather(*tasks, return_exceptions=True)

    print(f"\nAll {num_clients} clients finished")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Arena60 Test Client - WebSocket client for game server testing"
    )
    parser.add_argument(
        "--host",
        default="localhost",
        help="Server host (default: localhost)"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Server port (default: 8080)"
    )
    parser.add_argument(
        "--player",
        default="player1",
        help="Player ID (default: player1)"
    )
    parser.add_argument(
        "--clients",
        type=int,
        default=1,
        help="Number of concurrent clients (default: 1)"
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=5.0,
        help="Test duration in seconds (default: 5.0)"
    )

    args = parser.parse_args()

    print("=" * 60)
    print("Arena60 Test Client")
    print("=" * 60)
    print(f"Server: {args.host}:{args.port}")
    print(f"Clients: {args.clients}")
    print(f"Duration: {args.duration}s")
    print("=" * 60)
    print()

    try:
        if args.clients == 1:
            asyncio.run(run_single_client(args.player, args.host, args.port, args.duration))
        else:
            asyncio.run(run_multiple_clients(args.clients, args.host, args.port, args.duration))
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(0)


if __name__ == "__main__":
    main()
