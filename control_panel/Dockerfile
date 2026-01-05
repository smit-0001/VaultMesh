FROM python:3.11-slim

WORKDIR /app

# Install system dependencies (needed for psycopg2 postgres driver)
RUN apt-get update && apt-get install -y libpq-dev gcc

# Copy requirements first (to cache dependencies)
# We assume you have a requirements.txt. If not, we create it in the next step.
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the code
COPY . .

# Expose the API Port
EXPOSE 8000

# Run the server
CMD ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000"]