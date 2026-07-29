FROM python:3.13.13-slim-bookworm AS base

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY netfree-ca-bundle.crt /usr/local/share/ca-certificates/netfree-ca-bundle.crt

RUN cat /usr/local/share/ca-certificates/netfree-ca-bundle.crt \
    >> /etc/ssl/certs/ca-certificates.crt

ENV SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
ENV REQUESTS_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt
ENV PIP_CERT=/etc/ssl/certs/ca-certificates.crt

WORKDIR /app

COPY requirements.txt requirements-dev.txt ./

RUN python -m pip install \
    --cert /etc/ssl/certs/ca-certificates.crt \
    --no-cache-dir \
    --requirement requirements.txt

COPY app ./app
COPY alembic.ini ./
COPY migrations ./migrations


FROM base AS test

RUN python -m pip install \
    --cert /etc/ssl/certs/ca-certificates.crt \
    --no-cache-dir \
    --requirement requirements-dev.txt

COPY tests ./tests

CMD ["pytest", "-q"]


FROM base AS runtime

EXPOSE 8000

CMD ["sh", "-c", "uvicorn app.main:app --host 0.0.0.0 --port ${API_GATEWAY_PORT:-8000}"]
