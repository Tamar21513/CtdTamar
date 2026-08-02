import asyncio
from functools import wraps

import pytest

from app.matches.allocator import (
    GameServerShard,
    MatchUnavailableError,
    ShardPoolAllocator,
)


def async_test(function):
    @wraps(function)
    def run():
        return asyncio.run(function())

    return run


@async_test
async def test_acquiring_two_shards_returns_distinct_shards() -> None:
    pool = ShardPoolAllocator(
        [GameServerShard("a", 1), GameServerShard("b", 2)]
    )
    first = await pool.acquire()
    second = await pool.acquire()
    assert {first, second} == {
        GameServerShard("a", 1), GameServerShard("b", 2)
    }


@async_test
async def test_third_acquire_on_exhausted_pool_raises() -> None:
    pool = ShardPoolAllocator(
        [GameServerShard("a", 1), GameServerShard("b", 2)]
    )
    await pool.acquire()
    await pool.acquire()
    with pytest.raises(MatchUnavailableError):
        await pool.acquire()


@async_test
async def test_releasing_a_shard_makes_it_acquirable_again() -> None:
    pool = ShardPoolAllocator(
        [GameServerShard("a", 1), GameServerShard("b", 2)]
    )
    first = await pool.acquire()
    await pool.acquire()
    await pool.release(first)
    reacquired = await pool.acquire()
    assert reacquired == first


@async_test
async def test_single_shard_pool_behaves_like_old_single_lock() -> None:
    pool = ShardPoolAllocator([GameServerShard("only", 1)])
    await pool.acquire()
    with pytest.raises(MatchUnavailableError):
        await pool.acquire()


def test_empty_shard_pool_is_rejected() -> None:
    with pytest.raises(ValueError):
        ShardPoolAllocator([])


class FakeLauncher:
    def __init__(self) -> None:
        self.next_port = 9000
        self.spawn_calls = 0
        self.released: list[GameServerShard] = []

    async def spawn(self) -> GameServerShard:
        self.spawn_calls += 1
        shard = GameServerShard("dynamic", self.next_port)
        self.next_port += 1
        return shard

    async def release(self, shard: GameServerShard) -> None:
        self.released.append(shard)


@async_test
async def test_exhausted_static_pool_falls_back_to_launcher() -> None:
    launcher = FakeLauncher()
    pool = ShardPoolAllocator(
        [GameServerShard("static", 1)], launcher=launcher
    )
    await pool.acquire()
    dynamic = await pool.acquire()
    assert launcher.spawn_calls == 1
    assert dynamic == GameServerShard("dynamic", 9000)


@async_test
async def test_max_dynamic_shards_cap_is_enforced() -> None:
    launcher = FakeLauncher()
    pool = ShardPoolAllocator(
        [GameServerShard("static", 1)],
        launcher=launcher,
        max_dynamic_shards=1,
    )
    await pool.acquire()
    await pool.acquire()
    with pytest.raises(MatchUnavailableError):
        await pool.acquire()
    assert launcher.spawn_calls == 1


@async_test
async def test_releasing_dynamic_shard_does_not_return_to_static_pool() -> None:
    launcher = FakeLauncher()
    pool = ShardPoolAllocator(
        [GameServerShard("static", 1)],
        launcher=launcher,
        max_dynamic_shards=1,
    )
    await pool.acquire()
    dynamic = await pool.acquire()
    await pool.release(dynamic)
    assert launcher.released == [dynamic]

    # The static shard is still checked out (never released) and the
    # dynamic shard must not have leaked into the static list - the
    # next acquire has to go through the launcher again (a fresh
    # spawn_calls increment and a new port), not return instantly from
    # a corrupted static pool.
    second_dynamic = await pool.acquire()
    assert launcher.spawn_calls == 2
    assert second_dynamic == GameServerShard("dynamic", 9001)


@async_test
async def test_releasing_static_shard_returns_to_static_pool_without_launcher_call() -> None:
    launcher = FakeLauncher()
    pool = ShardPoolAllocator(
        [GameServerShard("static", 1)], launcher=launcher
    )
    shard = await pool.acquire()
    await pool.release(shard)
    assert launcher.released == []
    reacquired = await pool.acquire()
    assert reacquired == shard
    assert launcher.spawn_calls == 0
