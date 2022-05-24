/* 
 * echoservert_pre.c - A prethreaded concurrent echo server
 */
/* $begin echoservertpremain */
#include "csapp.h"
#define NTHREADS  20
#define SBUFSIZE  20

// Struct Area
typedef struct {
    int *buf;          /* Buffer array */         
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} sbuf_t;
typedef struct _Stock{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    sem_t cnt_mutex;
} Stock;
typedef struct _Tree{
    struct _Stock stock;
    struct _Tree *left;
    struct _Tree *right;
} Tree;

// Declaration Area
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
void stock_serv(int connfd);
void *thread(void *vargp);
static void init_stock_serv(void);
void init_stock(void);
void stock_function(char buf[], int connfd);
void parseline(char *cmdline, char *argv[]);
void eval(char *argv[], char buf[], int connfd);
void printTree(Tree *root, char *msg);
Tree *FindStock(int ID);
Tree *Insert(Tree *root, int ID, int price, int left_stock);
void sigint_handler(int sig);
void saveStock(FILE *fp, Tree *root);
// global variable Area
sbuf_t sbuf; /* Shared buffer of connected descriptors */
Tree *root;
static sem_t mutex;
static int byte_cnt = 0;

int main(int argc, char **argv) 
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid; 

    signal(SIGINT, sigint_handler);

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    init_stock();
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE); //line:conc:pre:initsbuf

    for (i = 0; i < NTHREADS; i++)  /* Create worker threads */ //line:conc:pre:begincreate
	    Pthread_create(&tid, NULL, thread, NULL);               //line:conc:pre:endcreate

    while (1) { 
        clientlen = sizeof(struct sockaddr_storage);
	    connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
	    sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
    }
}

// function Area
void sigint_handler(int sig) {
    FILE *fp = fopen("stock.txt", "w");
    saveStock(fp, root);
    fclose(fp);
    exit(0);
}
void saveStock(FILE *fp, Tree *root) {
    if(root == NULL) return;
    fprintf(fp, "%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    saveStock(fp, root->left);
    saveStock(fp, root->right);
    return;
}
void *thread(void *vargp) 
{  
    Pthread_detach(pthread_self()); 
    while (1) { 
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ //line:conc:pre:removeconnfd
        stock_serv(connfd);                /* Service client */
        Close(connfd);
    }
}
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                          /* Wait for available item */
    P(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->slots);                          /* Announce available slot */
    return item;
}
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int)); 
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}
static void init_stock_serv(void) 
{
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}
void stock_serv(int connfd) 
{
    int n, j;
    char buf[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    Pthread_once(&once, init_stock_serv); // main thread
    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        P(&mutex);
        byte_cnt += n;
        printf("thread %d received %d (%d total) bytes on fd %d\n",
                (int) pthread_self(), n, byte_cnt, connfd);
        V(&mutex);
        stock_function(buf, connfd);
        for(j=strlen(buf);j<MAXLINE;j++)
            buf[j]='\0';
        Rio_writen(connfd, buf, MAXLINE);
    }
}
void init_stock(void) {
    FILE *fp;
    char stockInfo[100];
    char *ptr;
    int stockID = 0, stockPrice = 0, stockLeftstock = 0, pos = 0;
    
    fp = fopen("stock.txt", "r");
    while(fgets(stockInfo, 100, fp) != NULL) {
        ptr = strtok(stockInfo, " ");
        while (ptr != NULL) {
            if(pos == 0) stockID = atoi(ptr);
            else if(pos == 1) stockLeftstock = atoi(ptr);
            else stockPrice = atoi(ptr);
            pos++;
            ptr = strtok(NULL, " ");
        }
        root = Insert(root, stockID, stockPrice, stockLeftstock);
        pos = 0;
    }
    fclose(fp);
}
Tree *Insert(Tree *root, int ID, int price, int left_stock) {
    if(root == NULL) {
        root = (Tree *)malloc(sizeof(Tree));
        root->right = root->left = NULL;
        root->stock.ID = ID;
        root->stock.left_stock = left_stock;
        root->stock.price = price;
        root->stock.readcnt = 0;
        Sem_init(&root->stock.mutex, 0, 1);
        Sem_init(&root->stock.cnt_mutex, 0, 1);
    }
    else {
        if (ID < root->stock.ID)
            root->left = Insert(root->left, ID, price, left_stock);
        else   
            root->right = Insert(root->right, ID, price, left_stock);
    }
    return root;
}
void stock_function(char buf[], int connfd) {
    char *argv[100];
    char cmdline[100];
    strcpy(cmdline, buf);
    parseline(cmdline, argv);
    eval(argv, buf, connfd);
    return;
}
void parseline(char *cmdline, char *argv[]) {
    int i = 0;
    char *temp = strtok(cmdline," ");
    while(temp != NULL) {
        argv[i] = temp;
        i++;
        temp = strtok(NULL, " ");
    }
    return;
}
void eval(char *argv[], char buf[], int connfd) {
    int ID, amount;
    char *msg = (char *)calloc(MAXLINE,sizeof(char));
    Tree *curr_node;

    if(strcmp(argv[0], "show\n") == 0) {
        printTree(root, msg);
        strcpy(buf, msg);
    }
    else if(strcmp(argv[0], "sell") == 0) {
        ID = atoi(argv[1]);
        amount = atoi(argv[2]);
        curr_node = FindStock(ID);
        P(&curr_node->stock.mutex);
        curr_node->stock.left_stock += amount;
        V(&curr_node->stock.mutex);
        msg = "[sell] success\n";
        strcpy(buf, msg);
    }
    else if(strcmp(argv[0], "buy") == 0) {
        ID = atoi(argv[1]);
        amount = atoi(argv[2]);
        curr_node = FindStock(ID);
        P(&curr_node->stock.mutex);
        if(curr_node->stock.left_stock < amount) {
            msg = "Not enough left stock\n";
            strcpy(buf, msg);
            V(&curr_node->stock.mutex);
            return;
        }
        else {            
            curr_node->stock.left_stock -= amount;
            V(&curr_node->stock.mutex);
            msg = "[buy] success\n";
            strcpy(buf, msg);
        }
    }
    else if(strcmp(argv[0], "exit") == 0) {
        // Make exit function
        Close(connfd);
        //FD_CLR(connfd, &p->read_set);
        // connfd = p->clientfd[i];
        /*for(i=0;i<p->maxi;i++)
            if(p->clientfd[i] == connfd)
                break;
        p->clientfd[i] = -1;
        */
    }
    return;
}
void printTree(Tree *root, char *msg) {
    char buf[MAXLINE];
    if(root == NULL) return;
    P(&mutex);
    root->stock.readcnt++;
    if(root->stock.readcnt == 1)
        P(&root->stock.mutex);
    V(&mutex);        
    sprintf(buf, "%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    P(&mutex);
    root->stock.readcnt--;
    if(root->stock.readcnt == 0)
        V(&root->stock.mutex);
    V(&mutex);
    strcat(msg, buf);
    printTree(root->left, msg);
    printTree(root->right, msg);
}
Tree *FindStock(int ID) {
    Tree *curr_node = root;
    while(1) {
        P(&mutex);
        //P(&curr_node->stock.cnt_mutex);
        curr_node->stock.readcnt++;
        //V(&curr_node->stock.cnt_mutex);
        if(curr_node->stock.readcnt == 1)
            P(&curr_node->stock.mutex);
        V(&mutex);

        if(curr_node->stock.ID == ID) {
            P(&mutex);
            curr_node->stock.readcnt--;
            if(curr_node->stock.readcnt == 0)
                V(&curr_node->stock.mutex);
            V(&mutex);
            return curr_node;
        }
        else if(curr_node->stock.ID > ID) {
            P(&mutex);
            curr_node->stock.readcnt--;
            if(curr_node->stock.readcnt == 0)
                V(&curr_node->stock.mutex);
            V(&mutex);
            curr_node = curr_node->left;
        }
        else {
            P(&mutex);
            curr_node->stock.readcnt--;
            if(curr_node->stock.readcnt == 0)
                V(&curr_node->stock.mutex);
            V(&mutex);
            curr_node = curr_node->right;
        }
    }
}
