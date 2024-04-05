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
        void *permanent_message;
        DeadMessageQueue dead_message_queue;

        Interconnect(int pid, int nproc, int work_bytes);
        
        // Receives one async messages, returns false if nothing received
        bool async_receive_message(Message &message);

        // Sends message asking for work
        void send_work_request(short recipient, bool urgent);
        
        // Sends work data to recipient
        void send_work(short recipient, void *work);

        // Sends an abort message
        void send_abort_message(short recipient);

        // Frees up saved dead messages
        void clean_dead_messages(bool always_free);

        // Frees the interconnect data structures
        void free_interconnect();
};

#endif