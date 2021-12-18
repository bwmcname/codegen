#pragma once
#include <stdlib.h>

template <typename T>
struct token_stack
{
    static const int Increment = 1024; // This seems fine.
    static const int InitialSize = 4096; // Pretty big, doesn't hurt.
    int Top;
    int Populated;
    int Capacity;
    T *Tokens;
};

template <typename T>
void Resize(token_stack<T> *Stack)
{
    int NewCapacity = Stack->Capacity + token_stack<T>::Increment;
    size_t BufferSize = sizeof(T) * NewCapacity;
    T *Buffer = (T *)malloc(BufferSize);
    
    memcpy(Buffer, Stack->Tokens, sizeof(T) * Stack->Capacity);
    free(Stack->Tokens);
    Stack->Tokens = Buffer;
}

template <typename T>
void CreateTokenStack(token_stack<T> *Stack)
{
    Stack->Top = -1;
    Stack->Populated = 0;
    
    Stack->Tokens = (T *)malloc(sizeof(T) * token_stack<T>::InitialSize);
    Stack->Capacity = token_stack<T>::InitialSize;
}

template <typename T>
void FreeTokenStack(token_stack<T> *Stack)
{
    free(Stack->Tokens);
}