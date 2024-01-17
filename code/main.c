#include <stdio.h>

#define BUFFER_SIZE 3

typedef struct {
  int data[BUFFER_SIZE];
  int head;
  int tail;
} CircularBuffer;

void initCircularBuffer(CircularBuffer *buffer) {
  buffer->head = 0;
  buffer->tail = 0;
}

void addDataToCircularBuffer(CircularBuffer *buffer, int data) {
  buffer->data[buffer->tail] = data;
  buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
}

int readDataFromCircularBuffer(CircularBuffer *buffer) {
  int data = buffer->data[buffer->head];
  buffer->head = (buffer->head + 1) % BUFFER_SIZE;
  return data;
}

int isCircularBufferFull(CircularBuffer *buffer) {
  return buffer->head == buffer->tail;
}

int isCircularBufferEmpty(CircularBuffer *buffer) {
  return buffer->head == (buffer->tail + 1) % BUFFER_SIZE;
}

int main(void)
{
  CircularBuffer buffer;

  // Initialize the circular buffer
  initCircularBuffer(&buffer);

  // Add some data to the buffer
  addDataToCircularBuffer(&buffer, 1);
  addDataToCircularBuffer(&buffer, 2);
  addDataToCircularBuffer(&buffer, 3);
  addDataToCircularBuffer(&buffer, 4);
  addDataToCircularBuffer(&buffer, 5);
  addDataToCircularBuffer(&buffer, 6);    

  // Read the data from the buffer
  printf("%d\n", readDataFromCircularBuffer(&buffer));
  printf("%d\n", readDataFromCircularBuffer(&buffer));
  printf("%d\n", readDataFromCircularBuffer(&buffer));

  return 0;
}
