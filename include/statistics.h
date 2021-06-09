
#ifndef STATISTICS_H
#define STATISTICS_H

#define STATISTICS_TIMER 2

extern long global_total_connections;
extern int global_concurent_connections;

extern long total_bytes_sent;
extern long total_bytes_recieved;

typedef struct statistics
{
    long total_connections;
    int current_connections;
    long total_sent;
    long total_recieved;   
} statistics;


void initialize_statistics();
void add_connection();
void remove_conection();
void add_sent_bytes(int bytes);
void add_bytes_recieved(int bytes);
void force_update();
statistics * get_statistics(statistics * stats);



#endif

