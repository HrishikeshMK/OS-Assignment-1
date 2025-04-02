#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/msg.h>

//STRUCT DEFINITIONS
typedef struct messageBuff{
    long mtype;
    int count; 
}message;

typedef struct TrieNode{
    struct TrieNode *letters[26];
    int count;
}TrieNode;

//TRIE FUNCTIONS
TrieNode *create(){
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    for(int i =0; i< 26; i++) node -> letters[i]= NULL;
    node -> count =0;
    return node;
}

void insert(TrieNode *root, char *str){
    TrieNode *temp = root;
    for(int i =0; str[i] != '\0'; i++){
        int j = str[i] - 'a';
        if(temp -> letters[j] == NULL) temp -> letters[j] = create();
        temp = temp -> letters[j];  
    }
    temp -> count ++;
}

int count(TrieNode *root, char *str){
    TrieNode *temp = root;
    for(int i = 0; str[i] != '\0'; i++){
        int j = str[i] - 'a';
        if(temp -> letters[j] == NULL) return 0;
        temp = temp-> letters[j];
    }
    return temp->count;
}

void deleteTrie(TrieNode *root){
    if(root == NULL) return;
    for(int i = 0; i < 26; i++)
        free(root -> letters[i]);
    free(root);
}

//OTHER FUNCTIONS
void caeserShift(char *str, int key) {
    if (key <= 0) return;
    for (int i = 0; str[i] != '\0'; i++) 
        str[i] = 'a' + (str[i] - 'a' + key) % 26;
}

int main(int argc, char *argv[]) {
    if (argc != 2) return 1;

    char inputN[12], wordsN[12]; // N - Name
    snprintf(inputN, sizeof(inputN), "input%s.txt", argv[1]);
    snprintf(wordsN, sizeof(wordsN), "words%s.txt", argv[1]);

    //INPUT FILE OPEN
    FILE *inputF = fopen(inputN, "r"); // F - File Pointer
    if (inputF == NULL) {
        printf("Error opening file %s\n", inputN);
        return 1;
    }

    int N, wlen, matrix_key, mq_key;
    fscanf(inputF, "%d", &N);
    fscanf(inputF, "%d", &wlen);
    fscanf(inputF, "%d", &matrix_key);
    fscanf(inputF, "%d", &mq_key);
    fclose(inputF);

    //SHARED MEMORY SEGMENT
    int shmid = shmget(matrix_key, sizeof(char[N][N][wlen]), 0666);
    char (*matrix)[N][wlen] = shmat(shmid, NULL, 0);
    if (matrix == (void *) -1) {
        perror("shmat failed");
        return 1;
    }

    //OPEN WORDS FILE
    FILE *wordsF = fopen(wordsN, "r");
    if (wordsF == NULL) {
        printf("Error opening file %s\n", wordsN);
        return 1;
    }

    TrieNode *root = create();
    char str[wlen+1];

    while (fscanf(wordsF, "%s", str) != EOF) insert(root, str);
    fclose(wordsF);

    //MESSAGE QUEUE
    int msqid = msgget(mq_key, 0666);
    if (msqid == -1) {
        perror("msgget failed");
        return 1;
    }   

    //MAIN LOGIC
    int key = 0;
    for (int i = 0; i < 2 * N - 1; i++) {
        int currCount = 0;  
        int j = (i < N) ? i : N - 1;
        int k = (i < N) ? 0 : i - N + 1;
        
        while (j >= 0 && k < N) { 
            caeserShift(matrix[j][k], key);
            currCount += count(root, matrix[j][k]); 
            j--; k++;
        }

        message msg;
        msg.mtype = 1;
        msg.count = currCount;

        if (msgsnd(msqid, &msg, sizeof(msg)-sizeof(msg.mtype), 0) == -1) {
            perror("msgsnd failed");
            return 1;
        }

        if (msgrcv(msqid, &msg, sizeof(msg)-sizeof(msg.mtype), 2, 0) == -1) {
            perror("msgrcv failed");
            return 1;
        }
        key = msg.count;
    }

    //CLEANING
    shmdt(matrix);
    deleteTrie(root);
    return 0;
}
