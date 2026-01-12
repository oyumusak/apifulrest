# ApifulRest

A lightweight, high-performance REST API framework for Go with C-based socket handling.

## Features

- 🚀 High-performance C socket layer with epoll
- 🔄 Goroutine pool for concurrent request handling
- 🛡️ Decorator pattern for middleware/authentication
- 📦 Simple and intuitive API

## Installation

```bash
go get github.com/oyumusak/apifulrest
```

## Quick Start

```go
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
```

## API

### Creating a Server

```go
server := apifulrest.New(port int)
```

### Adding Endpoints

```go
// GET endpoint
server.AddGet(endpoint string, handler HandlerFunc, decorator Decorator)

// POST endpoint
server.AddPost(endpoint string, handler HandlerFunc, decorator Decorator)
```

### Handler Function

```go
type HandlerFunc func(body string, headers map[string]string) interface{}
```

### Decorator (Middleware)

```go
type Decorator func(headers map[string]string) bool
```

Decorators run before the handler. Return `true` to continue, `false` to reject the request.

## Building

```bash
make
```

## Example Requests

```bash
# Simple request
curl localhost:3256/hello

# Request with header
curl localhost:3256/protected -H "Authorization: your-token"
```

## License

MIT
