FROM python:3.13-slim@sha256:e48f59cdba69a3cbd9ed2cf0f348d90d45a8d350d98134cf1070bc106ee9ed90
WORKDIR /app
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
COPY tools/launcher/web/server.py tools/launcher/web/state.py /app/
COPY tools/engine/bundle.py tools/engine/pe.py /app/
COPY tools/launcher/web/static /app/static
USER 10001:10001
EXPOSE 8765
CMD ["python", "/app/server.py"]
