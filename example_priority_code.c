#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Define the node structure for our priority queue
typedef struct Node {
    void *data;           // The element data (void pointer)
    float priority;       // Priority value (lower value = higher priority)
    struct Node *next;    // Pointer to the next node
} Node;

// Define the priority queue structure
typedef struct PriorityQueue {
    Node *head;           // Pointer to the head of the queue
    int size;             // Number of elements in the queue
} PriorityQueue;

// Function to create a new priority queue
PriorityQueue* createPriorityQueue() {
    PriorityQueue *pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    if (pq == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    pq->head = NULL;
    pq->size = 0;
    return pq;
}

// Function to create a new node
Node* createNode(void *data, float priority) {
    Node *newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    newNode->data = data;
    newNode->priority = priority;
    newNode->next = NULL;
    return newNode;
}

// Function to check if the priority queue is empty
bool isEmpty(PriorityQueue *pq) {
    return pq->head == NULL;
}

// Function to get the size of the priority queue
int size(PriorityQueue *pq) {
    return pq->size;
}

// Function to enqueue an element with priority
void enqueue(PriorityQueue *pq, void *data, float priority) {
    Node *newNode = createNode(data, priority);
    
    // If the queue is empty or the new node has higher priority than the head
    if (isEmpty(pq) || priority < pq->head->priority) {
        newNode->next = pq->head;
        pq->head = newNode;
    } else {
        // Find the position to insert based on priority
        Node *current = pq->head;
        while (current->next != NULL && current->next->priority <= priority) {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
    
    pq->size++;
}

// Function to dequeue the highest priority element
void* dequeue(PriorityQueue *pq) {
    if (isEmpty(pq)) {
        fprintf(stderr, "Priority queue is empty\n");
        return NULL;
    }
    
    Node *temp = pq->head;
    void *data = temp->data;
    pq->head = pq->head->next;
    free(temp);
    pq->size--;
    
    return data;
}

// Function to peek at the highest priority element without removing it
void* peek(PriorityQueue *pq) {
    if (isEmpty(pq)) {
        fprintf(stderr, "Priority queue is empty\n");
        return NULL;
    }
    
    return pq->head->data;
}

// Function to get the priority of the highest priority element
float getTopPriority(PriorityQueue *pq) {
    if (isEmpty(pq)) {
        fprintf(stderr, "Priority queue is empty\n");
        return -1.0f;  // Assuming negative priority isn't valid
    }
    
    return pq->head->priority;
}

// Function to clear the priority queue
void clear(PriorityQueue *pq) {
    while (!isEmpty(pq)) {
        dequeue(pq);
    }
}

// Function to free the priority queue
void freePriorityQueue(PriorityQueue *pq) {
    clear(pq);
    free(pq);
}

// Example usage
int main() {
    PriorityQueue *pq = createPriorityQueue();
    
    // Sample data (integers stored as void pointers)
    int a = 10, b = 20, c = 30, d = 40;
    
    // Enqueue elements with priorities
    enqueue(pq, &a, 3.5);
    enqueue(pq, &b, 1.2);
    enqueue(pq, &c, 4.7);
    enqueue(pq, &d, 2.1);
    
    printf("Priority queue size: %d\n", size(pq));
    
    // Dequeue and print elements based on priority
    printf("Dequeuing elements in priority order:\n");
    while (!isEmpty(pq)) {
        int *value = (int*)dequeue(pq);
        printf("Value: %d\n", *value);
    }
    
    freePriorityQueue(pq);
    return 0;
}