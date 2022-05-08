#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void myexit(char *out_val[]){
    for (int i = 0; i < 5; i++){
        printf("%s\n", out_val[i]);
        free(out_val[i]);
    }
    exit(EXIT_SUCCESS);
}

void words(char *outval[]){

    char *arr[5];

    for (int k = 0; k < 5; k++){
        char *num;
        outval[k] = malloc(sizeof(char) * 5);
        sprintf(outval[k], "str%d", k);
    }

}
int main(){

    char *out_val[5];
    words(out_val);
    myexit(out_val);
    for (int i = 0; i < 5; i++){
        printf("%s\n", out_val[i]);
        free(out_val[i]);
    }
}