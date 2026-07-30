CREATE TABLE IF NOT EXISTS users (
    id UUID PRIMARY KEY,
    username VARCHAR(32) NOT NULL,
    username_normalized VARCHAR(32) NOT NULL,
    password_hash VARCHAR(512) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_users_username_normalized UNIQUE (username_normalized)
);

CREATE UNIQUE INDEX IF NOT EXISTS ix_users_username_normalized
    ON users (username_normalized);
