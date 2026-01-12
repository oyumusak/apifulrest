package main

import (
	"context"
	"os"
	"os/signal"
	"syscall"

	apifulrest "github.com/oyumusak/apifulrest/apifulRest"
)

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, os.Interrupt, syscall.SIGTERM)

	server := apifulrest.New(3256)

	// Simple GET endpoint
	server.AddGet("/hello", func(body string, headers map[string]string) interface{} {
		return map[string]interface{}{
			"message": "Hello, World!",
		}
	}, nil)

	// GET endpoint with authentication decorator
	server.AddGet("/protected", protectedHandler, authMiddleware)

	go func() {
		<-sigCh
		cancel()
	}()

	server.Run(ctx)
}

func protectedHandler(body string, headers map[string]string) interface{} {
	return map[string]interface{}{
		"user": headers["id"],
	}
}

func authMiddleware(headers map[string]string) bool {
	token, ok := headers["Authorization"]
	if !ok {
		return false
	}
	headers["id"] = token
	return true
}
