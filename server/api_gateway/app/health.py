from collections.abc import Callable


def build_health_response(
    postgres_check: Callable[[], bool],
    redis_check: Callable[[], bool],
) -> tuple[dict[str, str], int]:
    postgres_available = postgres_check()
    redis_available = redis_check()
    gateway_available = (
        postgres_available and redis_available
    )

    body = {
        "api_gateway": (
            "healthy" if gateway_available else "unhealthy"
        ),
        "postgresql": (
            "healthy" if postgres_available else "unhealthy"
        ),
        "redis": (
            "healthy" if redis_available else "unhealthy"
        ),
    }
    return body, 200 if gateway_available else 503
