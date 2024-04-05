#include "interconnect.h"
#include <cstdint>
#include <string>
#include "helpers.h"
#include "cnf.h"
#include <cassert>
#include "mpi.h"

Interconnect::Interconnect(int pid, int nproc, int work_bytes) {
    Interconnect::pid = pid;
    Interconnect::nproc = nproc;
    Interconnect::work_bytes = work_bytes;
    Interconnect::permanent_message = (void *)malloc(sizeof(char));
    *((char *)Interconnect::permanent_message) = (char)0;
    DeadMessageQueue dead_message_queue;
    Interconnect::dead_message_queue = dead_message_queue;
}
        
// Receives one async messages, returns false if nothing received
bool Interconnect::async_receive_message(Message &message) {
    int buffer_size_needed = 0;
    MPI_Status status;
    int flag;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
    if (!flag) {
        return false;
    }
    MPI_Get_count(&status, MPI_CHAR, &buffer_size_needed);
    int sender = status.MPI_SOURCE;
    void *buffer = (void *)malloc(buffer_size_needed * sizeof(char));
    MPI_Recv(buffer, buffer_size_needed, MPI_CHAR, sender, 
        MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    message.sender = sender;
    message.type = status.MPI_TAG;
    message.data = buffer;
    return true;
}

// Sends message asking for work
void Interconnect::send_work_request(short recipient, bool urgent) {
    MPI_Request request;
    int tag = (int)(urgent);
    MPI_Isend(Interconnect::permanent_message, 1, MPI_CHAR, 
        recipient, tag, MPI_COMM_WORLD, &request);
    return;
}

// Sends work message
void Interconnect::send_work(short recipient, void *work) {
    MPI_Request request;
    MPI_Isend(work, Interconnect::work_bytes, MPI_CHAR, 
        recipient, 2, MPI_COMM_WORLD, &request);
    Interconnect::dead_message_queue.add_to_queue(work, request);
    return;
}

// Sends an abort message
void Interconnect::send_abort_message(short recipient) {
    MPI_Request request;
    MPI_Isend(Interconnect::permanent_message, 1, MPI_CHAR, 
        recipient, 3, MPI_COMM_WORLD, &request);
    return;
}

// Frees up saved dead messages
void Interconnect::clean_dead_messages(bool always_free) {
  if (PRINT_LEVEL >= 2) printf("\tPID %d cleaning_dead_message\n", Interconnect::pid);
  int num_to_check = Interconnect::dead_message_queue.count;
  int num_checked = 0;
  int num_cleaned = 0;
  void *message;
  MPI_Status status;
  int flag = 0;
  MPI_Request request;
  while (num_checked < num_to_check) {
    request = Interconnect::dead_message_queue.peak_front();
    message = Interconnect::dead_message_queue.pop_from_front();
    if (!always_free) {
      MPI_Test(&request, &flag, &status);
    } else {
      flag = 1;
    }
    if (!flag) {
      Interconnect::dead_message_queue.add_to_queue(message, request);
    } else {
      num_cleaned++;
    }
    num_checked++;
  }
  if (PRINT_LEVEL >= 2) printf("\tPID %d cleaning_dead_message cleaned = %d\n", Interconnect::pid, num_cleaned);
}

// Frees the interconnect data structures
void Interconnect::free_interconnect() {
    // TODO: implement this
    return;
}
