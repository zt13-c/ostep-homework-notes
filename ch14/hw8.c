#include<stdlib.h>
#include<stdio.h>

typedef struct {
    int *data;
    int size;
    int capacity;
} vector;

void vector_init(vector *v) {
    v->size = 0;
    v->capacity = 2; // 初始容量
    v->data = (int *) malloc(v->capacity * sizeof(int));
}

void vector_push_back(vector *v,int value) {
    if(v->size == v->capacity)
    {
        v->capacity *= 2; // 扩容
        v->data = (int *) realloc(v->data,v->capacity * sizeof(int));
        printf("触发扩容！新容量：%d\n", v->capacity);
    }

    v->data[v->size++] = value;
}

int main()
{
    vector v;
    vector_init(&v);
    for(int i = 0; i < 10; i++)
    {
        vector_push_back(&v,i);
    }
    free(v.data);
    return 0;
}