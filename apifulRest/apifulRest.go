package apifulrest

/*
#include "apifulRest.h"
#include <string.h>
*/
import "C"
import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"syscall"
	"unsafe"
)

// first body second headers
type HandlerFunc func(string, map[string]string) interface{}
type Decorator func(map[string]string) bool

type ApifulRest interface {
	AddGet(endpoint string, handler HandlerFunc, dec Decorator)
	AddPost(endpoint string, handler HandlerFunc, dec Decorator)
	Run(cancelCtx context.Context)
}

type functionPackage struct {
	handler   HandlerFunc
	decorator *Decorator
}

type apifulRest struct {
	port       int
	serverFd   int
	running    int
	middleware map[string]HandlerFunc
	postFuncs  map[string]functionPackage
	getFuncs   map[string]functionPackage
	reqChan    chan reqParams
}

type reqParams struct {
	respData   *C.char
	respLength *C.int
	cli_fd     C.int
}

var globalRest *apifulRest

func New(port int) ApifulRest {
	serverFd := C.createSocket(C.int(port))

	globalRest = &apifulRest{
		port:       port,
		serverFd:   int(serverFd),
		middleware: make(map[string]HandlerFunc),
		postFuncs:  make(map[string]functionPackage),
		getFuncs:   make(map[string]functionPackage),
		running:    0,
		reqChan:    make(chan reqParams, 1024),
	}
	return globalRest
}

func (a apifulRest) AddGet(endpoint string, handler HandlerFunc, dec Decorator) {
	var decPtr *Decorator
	if dec != nil {
		decPtr = &dec
	}
	a.getFuncs[endpoint] = functionPackage{
		handler:   handler,
		decorator: decPtr,
	}
}

func (a apifulRest) AddPost(endpoint string, handler HandlerFunc, dec Decorator) {
	var decPtr *Decorator
	if dec != nil {
		decPtr = &dec
	}
	a.postFuncs[endpoint] = functionPackage{
		handler:   handler,
		decorator: decPtr,
	}
}

func (a apifulRest) PoolFunc(cancelContext context.Context) {
	buf := make([]byte, 4096)
	for {

		select {
		case <-cancelContext.Done():
			return
		case reqParams := <-a.reqChan:

			fd := int(reqParams.cli_fd)

			n, err := syscall.Read(fd, buf)
			if err != nil || n == 0 {
				C.setDel(reqParams.cli_fd)
				if n != 0 {
					fmt.Printf("fd:%d\n", fd)
					fmt.Println("Read error:", err)
				}
				continue
			}

			data := string(buf[:n])

			method, path, headers, body := parseHTTPRequest(data)

			var funcs map[string]functionPackage
			switch method {
			case "GET":
				funcs = globalRest.getFuncs
			case "POST":
				funcs = globalRest.postFuncs
			}

			result := handleEndpoint(funcs, path, body, headers)

			respJSON, _ := json.Marshal(result)
			response := fmt.Sprintf("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", len(respJSON), respJSON)

			respSlice := unsafe.Slice((*byte)(unsafe.Pointer(reqParams.respData)), 4096)
			copy(respSlice, response)
			*reqParams.respLength = C.int(len(response))
			C.setWritable(reqParams.cli_fd)

		}

	}
}

func (a apifulRest) Run(cancelCtx context.Context) {
	a.running = 1

	for i := 0; i < 1024; i++ {
		go a.PoolFunc(cancelCtx)
	}

	go func() {
		<-cancelCtx.Done()
		a.running = 0
		C.triggerShutdown()
	}()

	C.processLoop(C.int(a.serverFd), ((*C.int)(unsafe.Pointer(&a.running))))
}

func parseHTTPRequest(raw string) (method, path string, headers map[string]string, body string) {
	headers = make(map[string]string)
	lines := strings.Split(raw, "\r\n")
	if len(lines) > 0 {
		parts := strings.Split(lines[0], " ")
		if len(parts) >= 2 {
			method = parts[0]
			path = parts[1]
		}
	}
	i := 1
	for ; i < len(lines); i++ {
		line := lines[i]
		if line == "" {
			break
		}
		if idx := strings.Index(line, ":"); idx != -1 {
			headers[strings.TrimSpace(line[:idx])] = strings.TrimSpace(line[idx+1:])
		}
	}
	body = strings.Join(lines[i+1:], "\r\n")
	return
}

func handleEndpoint(funcs map[string]functionPackage, path, body string, headers map[string]string) interface{} {
	handler, ok := funcs[path]
	if !ok {
		return map[string]interface{}{"error": "Not Found"}
	}

	if handler.decorator != nil {
		if !(*handler.decorator)(headers) {
			return map[string]interface{}{"error": "Invalid Request"}
		}
	}

	return handler.handler(body, headers)
}

//export HandleRequest
func HandleRequest(reqData *C.char, respData *C.char, respLength *C.int, cli_fd C.int) {

	globalRest.reqChan <- reqParams{
		respData:   respData,
		respLength: respLength,
		cli_fd:     cli_fd,
	}
}
