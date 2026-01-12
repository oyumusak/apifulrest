#ifndef APIFULREST_H
#define APIFULREST_H


typedef struct s_cliList {
    int fd;
    char    req[2048];
    char    resp[4096];
    int     respLength;
    int     ready;
    struct s_cliList *next;
    struct s_cliList *prev;
} t_cliList;

int	createSocket(int port);
void processLoop(int sockFd, int *running);
void setWritable(int cliFd);
void setDel(int cliFd);
void triggerShutdown();
void sendResponse(int clientFd, char *response, int length);

// Go'dan export edilen fonksiyon (Go tarafında tanımlı)
// reqData: C'den Go'ya gönderilen request
// reqLength: request uzunluğu
// respData: Go'dan C'ye dönen response (Go dolduracak)
// respLength: response uzunluğu (Go dolduracak)
// ready: Go işlemi bittiğinde 1 olacak
extern void HandleRequest(char *reqData, char *respData, int *respLength, int cli_fd);

#endif