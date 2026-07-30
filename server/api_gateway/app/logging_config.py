import logging
import time
from logging.handlers import RotatingFileHandler
from pathlib import Path

from app.config import Settings

_LOG_FORMAT = "%(asctime)s %(levelname)s %(name)s %(message)s"
_MAX_BYTES = 5 * 1024 * 1024
_BACKUP_COUNT = 3

# Module-level handler instances, created once and re-attached (never
# re-created) by every configure_logging() call. Alembic's
# `command.upgrade` runs `logging.config.fileConfig(...)` with its
# default `disable_existing_loggers=True`, which detaches every
# handler on the root logger, including these. configure_logging() is
# called again from the FastAPI lifespan startup (every time a
# TestClient re-enters the app, and once for a real server process)
# specifically so these handlers get put back if something else
# removed them.
_console_handler: logging.Handler | None = None
_file_handler: logging.Handler | None = None


class _UtcFormatter(logging.Formatter):
    """Formats timestamps as ISO-8601 UTC, e.g. 2026-07-31T10:15:23.456Z."""

    converter = time.gmtime

    def formatTime(
        self, record: logging.LogRecord, datefmt: str | None = None
    ) -> str:
        formatted = time.strftime(
            "%Y-%m-%dT%H:%M:%S", self.converter(record.created)
        )
        return f"{formatted}.{int(record.msecs):03d}Z"


def configure_logging(settings: Settings) -> None:
    """Ensures a console handler and a rotating file handler are
    attached to the root logger. Safe to call repeatedly: existing
    handlers are reused (never re-created, so no file descriptors are
    leaked) and simply re-attached if something else detached them.

    Never pass request bodies, passwords, confirm_password, session
    tokens, cookies, or password hashes to this logger.
    """
    global _console_handler, _file_handler

    root_logger = logging.getLogger()
    root_logger.setLevel(logging.INFO)

    formatter = _UtcFormatter(_LOG_FORMAT)

    # Existing console output (relied on by `docker compose logs`) is
    # left untouched; this adds our own console handler for the
    # application's own log records plus a rotating file handler.
    if _console_handler is None:
        _console_handler = logging.StreamHandler()
        _console_handler.setFormatter(formatter)
    if _console_handler not in root_logger.handlers:
        root_logger.addHandler(_console_handler)

    if _file_handler is None:
        try:
            log_dir = Path(settings.LOG_DIR)
            log_dir.mkdir(parents=True, exist_ok=True)
            _file_handler = RotatingFileHandler(
                log_dir / "api_gateway.log",
                maxBytes=_MAX_BYTES,
                backupCount=_BACKUP_COUNT,
                encoding="utf-8",
            )
            _file_handler.setFormatter(formatter)
        except OSError as error:
            root_logger.warning(
                "Could not open log directory %s (%s); "
                "file logging is disabled for this session.",
                settings.LOG_DIR,
                error,
            )
    if (
        _file_handler is not None
        and _file_handler not in root_logger.handlers
    ):
        root_logger.addHandler(_file_handler)

    # Alembic's `command.upgrade` also calls `fileConfig` with
    # `disable_existing_loggers=True` (the default), which sets
    # `.disabled = True` on every already-created logger not
    # mentioned in alembic.ini - including every `app.*` module
    # logger, since they are created at import time, before the
    # migration runs. Handler re-attachment above is not enough on
    # its own: a disabled logger drops records before they ever
    # reach a handler. Re-enable them every time this runs.
    for name, existing_logger in logging.root.manager.loggerDict.items():
        if isinstance(
            existing_logger, logging.Logger
        ) and name.startswith("app."):
            existing_logger.disabled = False
