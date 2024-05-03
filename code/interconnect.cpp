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
  DeadMessageQueue dead_message_queue;
  Interconnect::stashed_work = (Message *)malloc(sizeof(Message) * nproc);
  Interconnect::work_is_stashed = (bool *)calloc(sizeof(bool), nproc);
  Interconnect::num_stashed_work = 0;
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
  message.size = buffer_size_needed;
  message.data = buffer;
  if (PRINT_INTERCONNECT) printf(" I(message type %d [%d -> %d] received)\n", message.type, sender, Interconnect::pid);
  return true;
}

// Sends message asking for work
void Interconnect::send_work_request(short recipient, short version) {
  MPI_Request request;
  void *data = (void *)malloc(sizeof(char));
  MPI_Isend(data, 1, MPI_CHAR, recipient, version, MPI_COMM_WORLD, &request);
  Interconnect::dead_message_queue.add_to_queue(data, request);
  if (PRINT_INTERCONNECT) printf(" I(message type %d [%d -> %d] sent)\n", version, Interconnect::pid, recipient);
}

// Sends work message
void Interconnect::send_work(short recipient, void *work) {
  MPI_Request request;
  MPI_Isend(work, Interconnect::work_bytes, MPI_CHAR, recipient, 3, 
    MPI_COMM_WORLD, &request);
  Interconnect::dead_message_queue.add_to_queue(work, request);
  if (PRINT_INTERCONNECT) printf(" I(message type 3 [%d -> %d] sent)\n", Interconnect::pid, recipient);
}

// Sends an abort message
void Interconnect::send_abort_message(short recipient) {
  MPI_Request request;
  void *data = (void *)malloc(sizeof(char));
  MPI_Isend(data, 1, MPI_CHAR, recipient, 4, MPI_COMM_WORLD, &request);
  Interconnect::dead_message_queue.add_to_queue(data, request);
  if (PRINT_INTERCONNECT) printf(" I(message type 4 [%d -> %d] sent)\n", Interconnect::pid, recipient);
}

// Sends an invalidation message
void Interconnect::send_invalidation(short recipient) {
  MPI_Request request;
  void *data = (void *)malloc(sizeof(char));
  MPI_Isend(data, 1, MPI_CHAR, recipient, 5, MPI_COMM_WORLD, &request);
  Interconnect::dead_message_queue.add_to_queue(data, request);
  if (PRINT_INTERCONNECT) printf(" I(message type 5 [%d -> %d] sent)\n", Interconnect::pid, recipient);
}

// Sends a conflict clause to a recipient
void Interconnect::send_conflict_clause(
    short recipient, 
    Clause conflict_clause,
    bool broadcast) {
  if (broadcast) {
    for (short i = 0; i < Interconnect::nproc; i++) {
      if (i == Interconnect::pid) {
        continue;
      }
      send_conflict_clause(i, conflict_clause);
    }
    return;
  }
  size_t buffer_size = (sizeof(bool) + sizeof(int)) * conflict_clause.num_literals;
  void *data = (void *)malloc(buffer_size);
  bool *sign_ptr = (bool *)data;
  int *var_ptr = (int *)(sign_ptr + conflict_clause.num_literals);
  for (int i = 0; i < conflict_clause.num_literals; i++) {
    sign_ptr[i] = conflict_clause.literal_signs[i];
    var_ptr[i] = conflict_clause.literal_variable_ids[i];
  }
  MPI_Request request;
  MPI_Isend(data, buffer_size, MPI_CHAR, recipient, 6, 
    MPI_COMM_WORLD, &request);
  Interconnect::dead_message_queue.add_to_queue(data, request);
  if (PRINT_INTERCONNECT) printf(" I(message type 6 [%d -> %d] sent)\n", Interconnect::pid, recipient);
}

// Returns whether there is already work stashed from a sender, or
// from anyone if sender is -1.
bool Interconnect::have_stashed_work(short sender) {
  assert(-1 <= sender && sender < Interconnect::nproc);
  if (sender == -1) {
    return (Interconnect::num_stashed_work > 0);
  }
  return (Interconnect::work_is_stashed[sender]);
}

// Saves work to use or give away at a later time
void Interconnect::stash_work(Message work) {
  assert(0 <= work.sender && work.sender < Interconnect::nproc);
  assert(!have_stashed_work(work.sender));
  assert(work.type == 3);
  Interconnect::stashed_work[work.sender] = work;
  Interconnect::work_is_stashed[work.sender] = true;
  Interconnect::num_stashed_work++;
}

// Returns stashed work (work message) from a sender, or from anyone
// if sender is -1.
Message Interconnect::get_stashed_work(short sender) {
  assert(have_stashed_work(sender));
  assert(-1 <= sender && sender < Interconnect::nproc);
  if (sender == -1) {
    for (short i = 0; i < Interconnect::nproc; i++) {
      if (Interconnect::work_is_stashed[i]) {
        Message work = Interconnect::stashed_work[i];
        Interconnect::work_is_stashed[i] = false;
        Interconnect::num_stashed_work--;
        return work;
      }
    }
  }
  Message work = Interconnect::stashed_work[sender];
  Interconnect::work_is_stashed[sender] = false;
  Interconnect::num_stashed_work--;
  return work;
}

// Frees up saved dead messages
void Interconnect::clean_dead_messages(bool always_free) {
  if (PRINT_LEVEL >= 2) printf("\tPID %d: cleaning_dead_message\n", Interconnect::pid);
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
  if (PRINT_LEVEL >= 2) printf("\tPID %d: cleaning_dead_message cleaned = %d\n", Interconnect::pid, num_cleaned);
}

// Waits until all messages have been delivered, freeing dead message
// queue along the way.
void Interconnect::blocking_wait_for_message_delivery() {
  // TODO: Does the wait check for message send or message receive?
  while (Interconnect::dead_message_queue.count > 0) {
    clean_dead_messages();
  }
}

// Frees the interconnect data structures
void Interconnect::free_interconnect() {
  for (short i = 0; i < Interconnect::nproc; i++) {
    if (Interconnect::work_is_stashed[i]) {
      free((Interconnect::stashed_work[i]).data);
      Interconnect::num_stashed_work--;
    }
  }
  assert(Interconnect::num_stashed_work == 0);
  free(Interconnect::stashed_work);
  free(Interconnect::work_is_stashed);
  clean_dead_messages(true);
}