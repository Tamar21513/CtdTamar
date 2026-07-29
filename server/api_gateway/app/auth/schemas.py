import unicodedata
from datetime import datetime
from uuid import UUID

from pydantic import BaseModel, ConfigDict, Field, SecretStr, field_validator


def normalize_username(username: str) -> str:
    return unicodedata.normalize("NFKC", username.strip()).casefold()


def validate_username(username: str) -> str:
    display_name = unicodedata.normalize("NFKC", username.strip())
    if not 3 <= len(display_name) <= 32:
        raise ValueError("Username must be between 3 and 32 characters")
    if any(unicodedata.category(char).startswith("C") for char in display_name):
        raise ValueError("Username contains control characters")
    if not all(
        char.isalnum() or char in {"_", "-", "."}
        for char in display_name
    ):
        raise ValueError(
            "Username may contain letters, numbers, underscore, hyphen, and dot"
        )
    return display_name


class CredentialsRequest(BaseModel):
    username: str
    password: SecretStr

    @field_validator("username")
    @classmethod
    def username_is_valid(cls, value: str) -> str:
        return validate_username(value)

    @field_validator("password")
    @classmethod
    def password_is_valid(cls, value: SecretStr) -> SecretStr:
        password = value.get_secret_value()
        if len(password) < 10 or len(password) > 128:
            raise ValueError(
                "Password must be between 10 and 128 characters"
            )
        if not password.strip():
            raise ValueError("Password must not be blank")
        return value


class UserResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: UUID
    username: str
    created_at: datetime
