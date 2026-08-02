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
