# 1. Build Stage
FROM gcc:latest as builder

# Install CMake
RUN apt-get update && apt-get install -y cmake

WORKDIR /app

# Copy source code
COPY . .

# CRITICAL FIX: Remove any existing build folder copied from your machine
# and create a fresh one for the container.
RUN rm -rf build && mkdir build

WORKDIR /app/build

# Generate a fresh Makefile for the container environment
RUN cmake ..

# Compile
RUN make

# 2. Runtime Stage (This part remains the same)
FROM ubuntu:latest

WORKDIR /app

# We need standard libraries
RUN apt-get update && apt-get install -y libstdc++6

# Copy the binary from the builder stage
COPY --from=builder /app/build/vault_storage .

# Create the data folder
RUN mkdir -p data

# Expose the Storage Port
EXPOSE 9000

# Run the server
CMD ["./vault_storage"]