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
        Message *stashed_work;
        bool *work_is_stashed;
        short num_stashed_work;
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

        // Returns whether there is already work stashed from a sender, or
        // from anyone if sender is -1.
        bool have_stashed_work(short sender = -1);

        // Saves work to use or give away at a later time
        void stash_work(Message work);

        // Returns stashed work (work message) from a sender, or from anyone
        // if sender is -1.
        Message get_stashed_work(short sender = -1);

        // Frees up saved dead messages
        void clean_dead_messages(bool always_free = false);

        // Waits until all messages have been delivered, freeing dead message
        // queue along the way.
        void blocking_wait_for_message_delivery();

        // Frees the interconnect data structures
        void free_interconnect();
};

#endif