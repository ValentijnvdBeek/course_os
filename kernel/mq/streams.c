#include "streams.h"
#include <stdint.h>
#include "klibc.h"
#include "hashtable.h"
#include "drivers/uart.h"
#include "scheduler.h"

/*only call scheduler stuff(emit, broadcast, wait, sched_yeild), not kthr
 * do x = false whenever data comes in in test_kthread
 * use sched_yield in while loop instead of interupt which gives other threads chance to run
 * */
static int initialized;
struct ht *q_table;
struct queue *q_map[5000];
uint32_t next;

void q_create(char q_name[])
{
    struct queue *q = 0x0;
    //q = ht_alloc(sizeof(struct queue));
    q->qd = 0x0;
    q->q_name = q_name;
    print_uart0("initialized queue\n");
    if (initialized != 0){
        initialized = 0;
        os_printf_v2("******UNINITIALIZED*******\n");
        q_table = ht_alloc(5000);
        ht_add(q_table, q_name, q);
    }else{
        ht_add(q_table, q_name, q);
    }

    //implement the below if we increase # of subscribers
//  if (!os_strcmp(options,"rr")) {
//      q->options = options;
//  } else if(!os_strcmp(options,"broadcast")) {
//      q->options = options;
//  } else {
//  }
}

uint32_t q_open(char q_name[])
{
    if (initialized != 0) {
        return 0x0;
    }

    //change this hack. Why isn't it initialized to 0 already?
    if (next < 0 || next > 5000){
        next = 0;
    }
    os_printf_v2("q_name before ht_get is %s\n", q_name);
    struct queue *result = ht_get(q_table, q_name);
    os_printf_v2("ENTRY IS %d (in open)\n", *result);
    os_printf_v2("q_name after ht_get is %s\n", result->q_name);
    result->isStreaming = 0;
    if(result->qd != 0x0){
        print_uart0("has queue already\n");
        os_printf_v2("result->qd is %d\n", result->qd);
        return result->qd;
    }
    result->qd = next;
    q_map[next] = result;
    uint32_t qd = next;
    next++;//how should we deal with deleted streams? or should there just be a queue of x streams where we just delete the old ones?
    return qd;
}
// Adding message to queue
uint32_t q_publish(uint32_t qd, void *data, uint32_t datalen)
{
    // TODO: Copy the data? Don't copy the data? Copy-on-write the data?
    struct queue *q = q_map[qd];
    q->data = data;
    q->datalen = datalen;
    return &(q->data);
}

uint32_t q_subscribe(uint32_t qd, void (*receiver)(uint32_t src_tid, uint32_t event, char * data, int length), void *userdata)
{
    struct subscriber *user = 0x0;
    user->userdata = userdata;
    struct queue *q = q_map[qd];
    q->subscriber = user;
    q->receiver = receiver; 
    return 0;
}

void q_send(uint32_t qd, void *data, uint32_t datalength)
{
    os_printf_v2("in q_send\n");
    //get exact available space
    void *startingPoint = data;
    uint32_t spaceAvail = 512;
    struct queue *q = q_map[qd];
    while (datalength > spaceAvail) {
        print_uart0("in while loop\n");
        q->receiver(1, 2, startingPoint, spaceAvail);
        startingPoint = (uint32_t)&startingPoint + spaceAvail;
        datalength = datalength - spaceAvail;
    } 
    if(datalength > 0){
        q->receiver(1, 2, startingPoint, datalength);
    }
    //TODO: delete data after sending
    
}

void q_send_through_scheduler(uint32_t qd, void *data, uint32_t datalength) {
    //get exact available space
    struct queue *q = q_map[qd];
    if(! q->isStreaming){//checks if data is still being transmitted
        q->startingPoint = data;
    }
    q->isStreaming = 1;
    
    uint32_t spaceAvail = sched_get_message_space();
    
    while (datalength > spaceAvail) {
        q->receiver(1, 2, q->startingPoint, spaceAvail);
        q->startingPoint = (uint32_t)&(q->startingPoint) + spaceAvail;
        datalength = datalength - spaceAvail;
    } 
    if(datalength > 0){
        q->receiver(1, 2, q->startingPoint, datalength);
    }
    q->isStreaming = 0;
}

// block, waiting for a message from the queue
// read entire message or no data at all
uint32_t q_block_read(uint32_t qd, uint32_t *buf, uint32_t buflength)
{
    struct queue *current_queue = q_map[qd];
    // since NULL is undefined in the kernel, use 0x0 instead
    // need to use condition variables
    while (current_queue->data == 0x0)
        // BLOCK! FIX
    
    // check to see if data length is acceptable
    if (current_queue->datalen <= buflength) {

        uint32_t buf_index = *buf;
        // read to buffer
        for (uint32_t data_index = 0; data_index < current_queue->datalen; data_index++) {
            buf[buf_index] = ((uint32_t*) current_queue->data)[data_index];
            buf_index++;
        }

        return ((uint32_t) current_queue->data);
    }
    // data too big
    return 0;
}


// waits for reply, and when the reply comes, fills the buffer with it. Fails if buffer length too small
void q_send_reply(uint32_t reply_qd, uint32_t *data, uint32_t datalength)
{
    struct queue *reply_q = q_map[reply_qd];
    //get exact available space
    void *startingPoint = data;
    
    uint32_t spaceAvail = 512;
    while (datalength > spaceAvail) {
        print_uart0("in while loop\n");
        reply_q->receiver(1, 2, startingPoint, spaceAvail);
        startingPoint = (uint32_t)&startingPoint + spaceAvail;
        datalength = datalength - spaceAvail;
    } 
    if (datalength > 0){
        reply_q->receiver(1, 2, startingPoint, datalength);
    }
}


// waits for reply, and when the reply comes, fills the buffer with it. Fails if buffer length too small
void q_send_reply_through_scheduler(uint32_t reply_qd, uint32_t *data, uint32_t datalength)
{
    struct queue *reply_q = q_map[reply_qd];
    if(! reply_q->isStreaming){//checks if data is still being transmitted
        reply_q->startingPoint = data;
    }
    reply_q->isStreaming = 1;
    
    uint32_t spaceAvail = sched_get_message_space();
    while (datalength > spaceAvail) {
        print_uart0("in while loop\n");
        reply_q->receiver(1, 2, reply_q->startingPoint, spaceAvail);
        reply_q->startingPoint = (uint32_t)&(reply_q->startingPoint) + spaceAvail;
        datalength = datalength - spaceAvail;
    } 
    if (datalength > 0){
        reply_q->receiver(1, 2, reply_q->startingPoint, datalength);
    }

    reply_q->isStreaming = 0;
}


// attaches an asynchronous receiver to the reply
void q_subscribe_to_reply(uint32_t reply_qd, void (*receiver)(uint32_t src_tid, uint32_t event, char * data, int length))
{
    print_uart0("entering subscr to reply\n");
   struct queue *reply_q = q_map[reply_qd];
   reply_q->receiver = receiver;
   print_uart0("exiting subscr to reply\n");
}


void sample_receiver(uint32_t src_tid, uint32_t event, char * data, int length)
{
    //print_uart0("In Sample receiver. \n");
    int i;
    char *s = (char*)data;
    for (i=0; i<length; i++) {
        print_char_uart0(s[i]);
    }
}



void child_receiver(uint32_t parent_tid, uint32_t tid)
{
    char * msg = "-------------sent from child-------------\n";
    uint32_t msg_len = os_strlen(msg);
    uint32_t event_id = 1;
    os_printf("In child_receiver. parent_tid is %d tid is %d\n", parent_tid, tid);

    if(sched_get_message_space() >= msg_len){
        if(sched_post_message(parent_tid, event_id, msg, msg_len)){
            ERROR("Message fail\n");
        }else{
            os_printf("child_receiver. MESSAGE PASSED\n");
        }
    }
}

void parent_reciever(uint32_t parent_tid, uint32_t tid)
{
    os_printf("in parent_reciever. parent_tid is %d tid is %d\n", parent_tid, tid);
    sched_register_callback_handler(&sample_receiver);
    os_printf("in parent receiver. done scheduling\n");
    kthr_start(kthr_create(&child_receiver));
    os_printf("in parent_reciever. started child thread\n");
    kthr_yield();
    os_printf("in parent_reciever. done yielding\n");
}

void q_test()
{
    // test client
    os_printf("this is a test\n");
    print_uart0("in q_test\n");
    q_create("printf");
	int qd = q_open("printf");
    os_printf_v2("qd for print is %d\n", qd);
	q_subscribe(qd, sample_receiver, 0x0);
    os_printf("msg is %d\n", q_publish(qd, "first message", 13));
    // struct queue *test_q = q_map[qd];
    // // check client message
    // os_printf("%s\n", (char*) test_q->data);
    // uint32_t number = 0;
    // uint32_t *sample_buffer = &number;
    // q_create("printf_2");
    //int reply_qd = q_open("printf_2");
    //q_block_read(qd, *sample_buffer, 100);
    // test buffer
    //os_printf("%s\n", (char*) sample_buffer);



    char* mydata = "yo, dawg\n";
    q_create("message");
    qd = q_open("message");
    os_printf_v2("qd for message is %d\n", qd);
    qd = q_open("printf");
    os_printf_v2("qd for printf is %d\n", qd);
    qd = q_open("message");
    os_printf_v2("qd for message is %d\n", qd);
    q_subscribe(qd, sample_receiver, 0x0);
    sched_init();
    kthr_start(kthr_create(&parent_reciever));
    kthr_yield();
    q_send(qd, mydata, os_strlen(mydata));
    mydata = "sup g\n";
    q_send(qd, mydata, os_strlen(mydata));


    q_subscribe(qd, printf_receiver, 0x0);
    os_printf("about to create reply\n");
    q_create("reply");
    os_printf("successfully created reply\n");
    int reply_qd = q_open("reply");
    os_printf("successfully opened reply\n");
    q_subscribe_to_reply(reply_qd, &child_receiver);
    print_uart0("successfully subscribed to reply\n");
    q_subscribe(qd, printf_receiver, 0x0);

    // kthr_yield();
    //char* word = "hello dog";
   // sched_post_message(1, 2, word, sizeof(word));
    return;

    // os_printf_v2("this is a test\n");
    // q_create("my_q");
    // os_printf_v2("Calling q_open...\n");
    // qd = q_open("my_q");
    // os_printf_v2("qd is %d\n", qd);
    // char* data = "This is a message";
    // uint32_t *castData = (uint32_t*) data;
    // q_publish(qd, castData, sizeof(data));
    // char* userdata = "Some userdata";
    // uint32_t *castUserdata = (void*) userdata;// what should this really be?
    // void *fp = &sample_receiver;
    // //fp += 0x10000;
    // //os_printf("Function pointer is: %X\n", fp);
    // os_printf_v2("First 4 bytes at %X are: %X\n", fp, *(uint32_t*)fp);
    // q_subscribe(qd, (void (*)(uint32_t*, uint32_t*, uint32_t))fp, castUserdata);
    // os_printf_v2("asdf\n");
    // q_send(qd, castData, sizeof(data));
}
