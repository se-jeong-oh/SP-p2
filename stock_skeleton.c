#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>

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

// function Area
void unix_error(char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
void swap(int *a, int *b) {
    int tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
    return;
}
void printTree(Tree *root) {
    if(root == NULL) return;
    printf("%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    printTree(root->left);
    printTree(root->right);
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
void eval(char *argv[]) {
    int ID, amount;
    Tree *curr_node;
    if(strcmp(argv[0], "show") == 0)
        printTree(root);
    else if(strcmp(argv[0], "sell") == 0) {
        ID = atoi(argv[1]);
        amount = atoi(argv[2]);
        curr_node = FindStock(ID);
        curr_node->stock.left_stock += amount;
    }
    else if(strcmp(argv[0], "buy") == 0) {
        ID = atoi(argv[1]);
        amount = atoi(argv[2]);
        curr_node = FindStock(ID);
        if(curr_node->stock.left_stock < amount) {
            printf("Not enough left stock\n");
            return;
        }
        else {
            curr_node->stock.left_stock -= amount;
        }
    }
    else if(strcmp(argv[0], "exit") == 0) {
        // Make exit function
        FILE *fp = fopen("stock.txt", "w");
        saveStock(fp, root);
        fclose(fp);
        exit(0);
    }
    return;
}
char *Fgets(char *ptr, int n, FILE *stream) {
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
	    unix_error("Fgets error");

    return rptr;
}

// main Area
int main() {
    FILE *fp;
    char stockInfo[100];
    char cmdline[100];
    char *argv[100];
    Tree *stockNode;
    int stockID, stockPrice, stockLeftstock, pos = 0;
    char *ptr;
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
    // Data Load From stock.txt Finished.
    while(1) {
        Fgets(cmdline, 100, stdin);
        if(strcmp(cmdline,"\n")==0) continue;
        cmdline[strlen(cmdline)-1] = '\0';
        parseline(cmdline, argv);
        eval(argv);
    }
    //stockNode = FindStock(1);

    return 0;
}