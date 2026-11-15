FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ librabbitmq-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY main.cpp .
RUN g++ -std=c++17 -O2 -Wall -Wextra main.cpp -lrabbitmq -o sensor

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends librabbitmq4 && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/sensor /usr/local/bin/sensor
CMD ["sensor"]
