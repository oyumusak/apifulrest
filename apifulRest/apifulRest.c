#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "apifulRest.h"

#define MAX_EVENTS 10000
#define MAX_CLIENTS 10000

// Go'dan export edilen fonksiyon
extern void HandleRequest(char *reqData, char *respData, int *respLength, int cli_fd);

// Client context havuzu
typedef struct {
    int fd;
    int in_use;
    char reqData[2048];
    char respData[4096];
    int respLength;
    int ready;
} ClientContext;

static ClientContext client_pool[MAX_CLIENTS + 10];
struct epoll_event event, events[MAX_CLIENTS + 10];
static int shutdown_fd = -1;


// Context'i serbest bırak
void release_context(ClientContext *ctx) {
    if (ctx) {
        ctx->in_use = 0;
        ctx->fd = -1;
    }
}


int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return -1;
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

int	createSocket(int port){
	int sockFd;
	struct sockaddr_in addr;
	int opt = 1;
	
	sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFd == -1) {
		exit(0);
	}
	

    // SO_REUSEADDR ayarı
    setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	set_nonblocking(sockFd);

	memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);


    if (bind(sockFd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockFd);
        exit(1);
    }

	listen(sockFd, MAX_EVENTS);

	return (sockFd);
}

int epoll_fd;
void processLoop(int sockFd, int *running)
{
	if ((epoll_fd = epoll_create1(0)) == -1) {
        perror("Epoll create hatası");
        exit(EXIT_FAILURE);
    }

    // Shutdown için eventfd oluştur
    shutdown_fd = eventfd(0, EFD_NONBLOCK);
    if (shutdown_fd == -1) {
        perror("eventfd create hatası");
        exit(EXIT_FAILURE);
    }

    memset(client_pool, 0, sizeof(client_pool));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_pool[i].fd = -1;
        client_pool[i].in_use = 0;
    }

    static ClientContext server_ctx;
    server_ctx.fd = sockFd;
    server_ctx.in_use = 1;

    event.events = EPOLLIN; 
    event.data.ptr = &server_ctx;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockFd, &event) == -1) {
        perror("Epoll ctl hatası");
        exit(EXIT_FAILURE);
    }

    // Shutdown eventfd'yi epoll'a ekle
    static ClientContext shutdown_ctx;
    shutdown_ctx.fd = shutdown_fd;
    shutdown_ctx.in_use = 1;
    event.events = EPOLLIN;
    event.data.ptr = &shutdown_ctx;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, shutdown_fd, &event) == -1) {
        perror("Epoll ctl shutdown hatası");
        exit(EXIT_FAILURE);
    }
	
	int event_count;
	while (*running) {
		event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if (event_count == -1) {
            if (errno == EINTR) {
                // Sinyal geldi, devam et
                continue;
            }
            perror("Epoll wait");
            break;
        } else if (event_count == 0) {
			continue;
		}
        // Olayları işle
        for (int i = 0; i < event_count; i++) {
            ClientContext *ctx = (ClientContext*)events[i].data.ptr;
            
            if (ctx == NULL) continue;

            // Shutdown sinyali kontrol et
            if (ctx->fd == shutdown_fd) {
                *running = 0;
                break;
            }
            
            if (ctx->fd == sockFd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int new_socket = accept(sockFd, (struct sockaddr *)&client_addr, &client_len);
                
                if (new_socket == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("Accept");
                    }
                    continue;
                }
                set_nonblocking(new_socket);


				ClientContext *client_ctx = &client_pool[new_socket];
				client_ctx->in_use = 1;
				client_ctx->ready = 0;
				client_ctx->respLength = 0;
				memset(client_ctx->reqData, 0, sizeof(client_ctx->reqData));
				memset(client_ctx->respData, 0, sizeof(client_ctx->respData));


                client_ctx->fd = new_socket;

                // Yeni müşteriyi Epoll listesine ekle (ptr kullanarak)
                event.events = EPOLLIN | EPOLLET;
                event.data.ptr = client_ctx;
                
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) {
                    perror("Epoll ctl add client");
                    release_context(client_ctx);
                    close(new_socket);
                }
            } else {
                int client_fd = ctx->fd;
				
				if (events[i].events & EPOLLIN) {

						HandleRequest(
							ctx->reqData, 
							ctx->respData, 
							&ctx->respLength,
							client_fd
						);	
				} else if ((events[i].events & EPOLLOUT)){
					if (client_fd < 0)
						printf("cevap verildi %d\n", client_fd);
                    send(client_fd, ctx->respData, ctx->respLength, 0);
                    
                    // Bağlantıyı kapat ve context'i serbest bırak
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    release_context(ctx);
				}
            }
        }
	}

	close(sockFd);
	if (shutdown_fd != -1) {
		close(shutdown_fd);
		shutdown_fd = -1;
	}
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_pool[i].fd != -1)
			close(client_pool[i].fd);
	}
}


void setDel(int cliFd) {
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cliFd, NULL);
	close(cliFd);
	release_context(&client_pool[cliFd]);
}

void setWritable(int cliFd){
	struct epoll_event new;
	new.events = EPOLLOUT | EPOLLET;
	new.data.ptr = &client_pool[cliFd];
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, cliFd, &new);
}

void triggerShutdown() {
	if (shutdown_fd != -1) {
		uint64_t val = 1;
		write(shutdown_fd, &val, sizeof(val));
	}
}