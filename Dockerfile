FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    libenet-dev \
    curl \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY NetworkProtocol.h .
COPY Server.cpp .

# Компилируем C++ сервер
RUN g++ Server.cpp -o Server -lenet

# Скачиваем Playit
RUN curl -Lo playit https://github.com/playit-cloud/playit-agent/releases/latest/download/playit-linux-amd64 && chmod +x playit

# Запуск сервера, туннеля и веб-заглушки для Render
CMD ./Server & ./playit & python3 -m http.server $PORT
