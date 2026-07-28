import unittest

from app.health import build_health_response


class HealthLogicTests(unittest.TestCase):
    def test_both_dependencies_are_available(self) -> None:
        body, status_code = build_health_response(
            postgres_check=lambda: True,
            redis_check=lambda: True,
        )

        self.assertEqual(status_code, 200)
        self.assertEqual(
            body,
            {
                "api_gateway": "healthy",
                "postgresql": "healthy",
                "redis": "healthy",
            },
        )

    def test_postgres_is_unavailable(self) -> None:
        body, status_code = build_health_response(
            postgres_check=lambda: False,
            redis_check=lambda: True,
        )

        self.assertEqual(status_code, 503)
        self.assertEqual(
            body,
            {
                "api_gateway": "unhealthy",
                "postgresql": "unhealthy",
                "redis": "healthy",
            },
        )

    def test_redis_is_unavailable(self) -> None:
        body, status_code = build_health_response(
            postgres_check=lambda: True,
            redis_check=lambda: False,
        )

        self.assertEqual(status_code, 503)
        self.assertEqual(
            body,
            {
                "api_gateway": "unhealthy",
                "postgresql": "healthy",
                "redis": "unhealthy",
            },
        )
