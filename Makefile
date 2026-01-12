CC = gcc
CFLAGS = -fPIC
CFILES_DIR = cFiles

# Go projesini çalıştır (cache temizle + derle + çalıştır)
run:
	go clean -cache
	go build -o app . && ./app

# Go projesini derle (cache temizle)
build:
	go clean -cache
	go build -o app .

# Temizlik
clean:
	go clean -cache
	rm -f $(CFILES_DIR)/*.o $(CFILES_DIR)/*.so app

.PHONY: run build clean