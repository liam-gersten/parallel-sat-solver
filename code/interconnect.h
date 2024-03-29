#ifndef INTERCONNECT_H
#define INTERCONNECT_H

#include <cstdint>
#include <string>
#include "helpers.h"
#include "cnf.h"

class Interconnect {
    public:
        int pid;
        int nproc;
        int work_bytes;
        bool *work_requests;
        short num_work_requests;
        void **stashed_work;
        bool *have_stashed_work;
        short num_stashed_work;
        void *permanent_message;
        DeadMessageQueue dead_message_queue;

        Interconnect(int pid, int nproc, int work_bytes);
        
        // Receives one async messages, returns false if nothing received
        bool async_receive_message(Cnf &cnf);

        // Sends message indicating algorithm success
        void send_success_message(short recipient);

        // Sends message asking for work
        void send_work_request(short recipient);

        // Sends message indicating no work is needed
        void rescind_work_request(short recipient);

        // Sends work message
        void send_work(void *work, short recipient);
        
        // Asks other processors for work to do
        void ask_for_work();

        // Sends work request to everyone
        void broadcast_work_request();

        // Rescinds work request sent to everyone
        void broadcast_rescind_request();

        // Gets work to do from another process
        void *get_work(Cnf &cnf);
        
        // Picks from the processors asking for work
        int pick_recipient();
        
        // Gives a lazy process some work to do, returns true if the task was consumed
        bool give_away_work(Cnf &cnf, Task task);

        // Frees up saved dead messages
        void clean_dead_messages(bool always_free);

        // Frees the interconnect data structures
        void free_interconnect();
};

#endif