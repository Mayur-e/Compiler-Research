#pragma once
void* __builtin_malloc(unsigned long long);
struct Node { struct Node* next; struct Node* prev; int data; };
