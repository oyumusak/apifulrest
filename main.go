package main

import (
	"context"
	"fmt"
	"os"
	"os/signal"
	"syscall"

	apifulrest "github.com/oyumusak/apifulrest/apifulRest"
)

var requestCount int

func yusufFunc(body string, headers map[string]string) interface{} {

	return map[string]interface{}{
		"name": "omer",
	}
}

func aliFunc(body string, headers map[string]string) interface{} {
	if _, ok := headers["id"]; !ok {
		return map[string]interface{}{
			"error": "need jwt auth",
		}
	}

	header := "hello"
	if val, ok := headers["ip"]; ok {
		header = val
	}
	return map[string]interface{}{
		"name":   "ali",
		"header": header,
	}
}

func tokenValidator(headers map[string]string) bool {
	val, ok := headers["authorizon"]
	if !ok {
		return false
	}
	headers["id"] = val
	return true
}

func main() {

	requestCount = 0
	ctx, cancel := context.WithCancel(context.Background())
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, os.Interrupt, syscall.SIGTERM)

	server := apifulrest.New(3256)

	server.AddGet("/yusuf", yusufFunc, nil)
	server.AddGet("/ali", aliFunc, tokenValidator)

	go func() {
		<-sigCh
		fmt.Println("Kapatılıyor...")
		cancel()
	}()

	server.Run(ctx)
}
