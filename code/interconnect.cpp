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
    Interconnect::work_requests = (bool *)calloc(nproc, sizeof(bool));
    Interconnect::num_work_requests = 0;
    Interconnect::stashed_work = (void **)malloc(sizeof(void *) * nproc);
    Interconnect::have_stashed_work = (bool *)calloc(nproc, sizeof(bool));
    Interconnect::num_stashed_work = 0;
    Interconnect::permanent_message = (void *)malloc(sizeof(char));
    *((char *)Interconnect::permanent_message) = (char)0;
    DeadMessageQueue dead_message_queue;
    Interconnect::dead_message_queue = dead_message_queue;
}
        
// Receives one async messages, returns false if nothing received
bool Interconnect::async_receive_message(Cnf &cnf) {
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
    switch (status.MPI_TAG) {
        case 0: {
            assert(buffer_size_needed == 1);
            cnf.free_cnf();
            free_interconnect();
            MPI_Finalize();
            exit(0);
        } case 1: {
            assert(buffer_size_needed == 1);
            assert(!(Interconnect::work_requests[sender]));
            Interconnect::work_requests[sender] = true;
            Interconnect::num_work_requests++;
            return true;
        } case 2: {
            assert(buffer_size_needed == 1);
            if (Interconnect::work_requests[sender]) {
                Interconnect::num_work_requests--;
            }
            Interconnect::work_requests[sender] = false;
            return true;
        } default: {
            assert(status.MPI_TAG == 3);
            assert(buffer_size_needed == Interconnect::work_bytes);
            assert(!(Interconnect::have_stashed_work[sender]));
            Interconnect::stashed_work[sender] = buffer;
            Interconnect::have_stashed_work[sender] = true;
            Interconnect::num_stashed_work++;
            return true;
        }
    }
}

// Sends message indicating algorithm success
void Interconnect::send_success_message(short recipient) {
    MPI_Request request;
    MPI_Isend(Interconnect::permanent_message, 1, MPI_CHAR, 
        recipient, 0, MPI_COMM_WORLD, &request);
}

// Sends message asking for work
void Interconnect::send_work_request(short recipient) {
    MPI_Request request;
    MPI_Isend(Interconnect::permanent_message, 1, MPI_CHAR, 
        recipient, 1, MPI_COMM_WORLD, &request);
}

// Sends message indicating no work is needed
void Interconnect::rescind_work_request(short recipient) {
    MPI_Request request;
    MPI_Isend(Interconnect::permanent_message, 1, MPI_CHAR, 
        recipient, 2, MPI_COMM_WORLD, &request);
}

// Sends work message
void Interconnect::send_work(void *work, short recipient) {
    MPI_Request request;
    MPI_Isend(work, Interconnect::work_bytes, MPI_CHAR, 
        recipient, 3, MPI_COMM_WORLD, &request);
    Interconnect::dead_message_queue.add_to_queue(work, request);
    return;
}

// Asks other processors for work to do
void Interconnect::ask_for_work() {
    // Implements a broadcast
    assert(Interconnect::num_stashed_work == 0);
    for (int recipient = 0; recipient < Interconnect::nproc; recipient++) {
        if (recipient == Interconnect::pid) {
            continue;
        }
        if (work_requests[recipient]) {
            continue;
        }
        send_work_request(recipient);
    }
}

// Sends work request to everyone
void Interconnect::broadcast_work_request() {
    for (int i = 0; i < Interconnect::nproc; i++) {
        if (i == Interconnect::pid) {
            continue;
        }
        send_work_request(i);
    }
}

// Rescinds work request sent to everyone
void Interconnect::broadcast_rescind_request() {
    for (int i = 0; i < Interconnect::nproc; i++) {
        if (i == Interconnect::pid) {
            continue;
        }
        rescind_work_request(i);
    }
}

// Gets work to do from another process
void *Interconnect::get_work(Cnf &cnf) {
    while (async_receive_message(cnf)) {
        continue;
    }
    if (Interconnect::num_stashed_work == 0) {
        broadcast_work_request();   
    }
    while (Interconnect::num_stashed_work == 0) {
        async_receive_message(cnf);
    }
    broadcast_rescind_request();
    assert(Interconnect::num_stashed_work > 0);
    for (int i = 0; i < Interconnect::nproc; i++) {
        if (i == Interconnect::pid) {
            continue;
        } else if (Interconnect::have_stashed_work[i]) {
            Interconnect::have_stashed_work[i] = false;
            Interconnect::num_stashed_work--;
            return Interconnect::stashed_work[i];
        }
    }
    assert(false);
}

// Picks from the processors asking for work
int Interconnect::pick_recipient() {
    // Picks arbitrary recipient
    assert(Interconnect::num_work_requests > 0);
    for (int i = 0; i < Interconnect::nproc; i++) {
        if (i == Interconnect::pid) {
            continue;
        } else if (Interconnect::work_requests[i]) {
            return i;
        }
    }
    assert(false);
}

// Gives a lazy process some work to do, returns true if the task was consumed
bool Interconnect::give_away_work(Cnf &cnf, Task task) {
    assert(Interconnect::num_work_requests > 0);
    int recipient = pick_recipient();
    if (Interconnect::num_stashed_work > 0) {
        for (int i = 0; i < Interconnect::nproc; i++) {
            if (i == Interconnect::pid) {
                continue;
            } else if (Interconnect::have_stashed_work[i]) {
                send_work(Interconnect::stashed_work[i], recipient);
                Interconnect::have_stashed_work[i] = false;
                Interconnect::num_stashed_work--;
                Interconnect::work_requests[recipient] = false;
                Interconnect::num_work_requests--;
                return false;
            }
        }
        assert(false);
    }
    unsigned int *compressed = cnf.to_int_rep();
    void *work = cnf.convert_to_work_message(compressed, task);
    send_work(work, recipient);
    Interconnect::work_requests[recipient] = false;
    Interconnect::num_work_requests--;
    return true;
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
    free(work_requests);
    free(stashed_work);
    free(have_stashed_work);
    return;
}
