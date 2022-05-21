#include "csapp.h"

typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

typedef struct _Stock{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
} Stock;

typedef struct _Tree{
    struct _Stock stock;
    struct _Tree *left;
    struct _Tree *right;
} Tree;

Tree *root;
int show_check = 0;
int byte_cnt = 0;

void printTree(Tree *root, char *msg) {
    char buf[MAXLINE];
    if(root == NULL) return;
    sprintf(buf, "%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    strcat(msg, buf);
    printTree(root->left, msg);
    printTree(root->right, msg);
}
Tree *Insert(Tree *root, int ID, int price, int left_stock) {
    if(root == NULL) {
        root = (Tree *)malloc(sizeof(Tree));
        root->right = root->left = NULL;
        root->stock.ID = ID;
        root->stock.left_stock = left_stock;
        root->stock.price = price;
    }
    else {
        if (ID < root->stock.ID)
            root->left = Insert(root->left, ID, price, left_stock);
        else   
            root->right = Insert(root->right, ID, price, left_stock);
    }
    return root;
}
Tree *FindStock(int ID) {
    Tree *curr_node = root;
    while(1) {
        if(curr_node->stock.ID == ID)
            return curr_node;
        else if(curr_node->stock.ID > ID)
            curr_node = curr_node->left;
        else   
            curr_node = curr_node->right;
    }
}
void saveStock(FILE *fp, Tree *root) {
    if(root == NULL) return;
    fprintf(fp, "%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    saveStock(fp, root->left);
    saveStock(fp, root->right);
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
    char *msg = (char *)malloc(sizeof(char)*MAXLINE);
    Tree *curr_node;
    //printf("argv : %s\n", argv[0]);
    //printf("here!\n");
    if(strcmp(argv[0], "show\n") == 0) {
        printTree(root, msg);
        strcpy(buf, msg);
    }
    else if(strcmp(argv[0], "sell") == 0) {
        ID = atoi(argv[1]);
        amount = atoi(argv[2]);
        curr_node = FindStock(ID);
        curr_node->stock.left_stock += amount;
        msg = "[sell] success\n";
        strcpy(buf, msg);
    }
    else if(strcmp(argv[0], "buy") == 0) {
        ID = atoi(argv[1]);
        amount = atoi(argv[2]);
        curr_node = FindStock(ID);
        if(curr_node->stock.left_stock < amount) {
            msg = "Not enough left stock\n";
            strcpy(buf, msg);
            return;
        }
        else {
            curr_node->stock.left_stock -= amount;
            msg = "[buy] success\n";
            strcpy(buf, msg);
        }
    }
    else if(strcmp(argv[0], "exit") == 0) {
        // Make exit function
        FILE *fp = fopen("stock.txt", "w");
        saveStock(fp, root);
        fclose(fp);
        Close(connfd);
    }
    return;
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
void stock_function(char buf[], int connfd) {
    char *argv[100];
    char cmdline[100];
    strcpy(cmdline, buf);
    parseline(cmdline, argv);
    eval(argv, buf, connfd);
    return;
}
void init_pool(int listenfd, pool *p) {
    int i;
    p->maxi = -1;
    for(i=0;i<FD_SETSIZE;i++)
        p->clientfd[i] = -1;
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}
void add_client(int connfd, pool *p) {
    int i;
    p->nready--;
    for(i=0;i<FD_SETSIZE;i++) {
        if(p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            FD_SET(connfd, &p->read_set);

            if(connfd > p->maxfd)
                p->maxfd = connfd;
            if(i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}
void check_clients(pool *p) {
    int i, connfd, n, j;
    char buf[MAXLINE];
    rio_t rio;

    for(i=0;(i<=p->maxi) && (p->nready > 0);i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n",
                        n, byte_cnt, connfd);
                printf("%s\n", buf);
                stock_function(buf, connfd);
                for(j=strlen(buf);j<MAXLINE;j++)
                    buf[j]='\0';
                Rio_writen(connfd, buf, MAXLINE);
                //printf("Waiting! %d\n", connfd);
            }
            else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}
int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    static pool pool;

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }
    init_stock();
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
        //if(FD_ISSET(STDIN_FILENO, &pool.ready_set))
            //command();
        if(FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage); 
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
        }
        check_clients(&pool);
    }
}
