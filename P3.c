#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_MINUTE 60


//////////////////////
//      STRUCTS     //
//////////////////////

typedef struct _seller_id {
    char seller_type;
    int seller_num;
} seller_id;

typedef struct _customer {
    seller_id server;
    int customer_num;
    int arrival_time;
    int service_time;
    int served;
} customer;


//////////////////////////////////
//      GLOBAL VARIABLES        //
//////////////////////////////////

int N = 0;                                          // number of customers per seller
int curr_minute = 0;                                // current time in simulation (1 sec ~ 1 min)
int threads_operating = 10;                         // number of threads still in operation
customer * customer_array[10];                      // array of customer queues
customer seating_chart[10][10];                     // seating chart [row][col] 
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;     // condition variable for synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex for serialization


//////////////////////////////////
//      FUNCTION PROTOTYPES     //
//////////////////////////////////

// Initializes the global seating chart to have no seats filled
void initialize_seating_chart();
// Utility function to compare two customers by their arrival time, used for sorting
int compare_customers(const void * a, const void * b);
// Generates the customer queue for a seller and sorts the generated customers by arrival time
// Returns the arrival time of the first customer in the queue
int generate_and_sort_customers(customer * customers, seller_id * server);
// Attempts to find a seat for a given customer and if so assigns them that seat
// Returns 1 if a seat was assigned and 0 otherwise
int serve_customer(customer * c, char * seller_type);
// Returns the id for printing the seller
int get_seller_print_num(seller_id * id);
// Prepares the thread to stop its operations and return 
void stop_operations();
// Handles selling the ticket to the customers, this is the entry point to the threads
void * sell(void * sid);
// Wakes up all seller threads
void wakeup_all_seller_threads();
// Prints the current seating chart
void print_seating_chart();
// Prints all customer info, used for debugging only
void debug_print_customers();


//////////////////////
//      CODE        //
//////////////////////

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("please use as ./P3 N\n");
        return -1;
    }

    int i;
    curr_minute = 0;
    N = atoi(argv[1]);
    pthread_t tids[10];
    seller_id sid[10];

    initialize_seating_chart();

    //srand(100);   // 100 actually generates interesting values
    srand(time(NULL));

    // Create 10 threads representing the 10 sellers.
    printf("Creating threads \n");
    sid[0].seller_type = 'H';
    sid[0].seller_num = 0;
    pthread_create(&tids[0], NULL, sell, &(sid[0]));
    
    for (i = 1; i < 4; ++i)
    {        
        sid[i].seller_type = 'M';
        sid[i].seller_num = i;
        pthread_create(&tids[i], NULL, sell, &(sid[i]));
    }

    for (i = 4; i < 10; ++i)
    {
        sid[i].seller_type = 'L';
        sid[i].seller_num = i;
        pthread_create(&tids[i], NULL, sell, &(sid[i]));
    }

    threads_operating = 10;

    printf("Threads created \n");
    sleep(2);
    print_seating_chart();
    sleep(2);

    // Wakeup all seller threads
    while (threads_operating > 0) {        
        printf("\n%d:%02d\n", (curr_minute / 60), (curr_minute % 60));    
        wakeup_all_seller_threads();
        sleep(1);
        ++curr_minute;
    }

    // Wait for all seller threads to exit
    wakeup_all_seller_threads();
    for (i = 0; i < 10; ++i)
    {
        pthread_join(tids[i], NULL);
    }
    
    //
    printf("\n\nAll sales windows are cleared\n");
    // Printout simulation results
    print_seating_chart();

    return(0);
}


void initialize_seating_chart() {
    int i = 0;
    int j = 0;

    for (i = 0; i < 10; ++i) {
        for (j = 0; j < 10; ++j) {
            seating_chart[i][j].server.seller_type = 'X';
            seating_chart[i][j].server.seller_num = -1;
            seating_chart[i][j].customer_num = -1;
            seating_chart[i][j].arrival_time = -1;
            seating_chart[i][j].service_time = -1;
            seating_chart[i][j].served = 0;
        }
    }            
}

int compare_customers(const void * a, const void * b) {
    customer * customer_a = (customer *)a;
    customer * customer_b = (customer *)b;
    return (customer_a->arrival_time - customer_b->arrival_time);
}


int generate_and_sort_customers(customer * customers, seller_id * server) {
    int i = 0;
    
    // generate customers
    for (i = 0; i < N; ++i) {
        seller_id sid;
        sid.seller_type = server->seller_type;
        sid.seller_num = server->seller_num; 
        customers[i].server = sid; 
        customers[i].arrival_time = (int)rand() % MAX_MINUTE;
        switch (server->seller_type) {
            case 'H':
                customers[i].service_time = (((int)rand() % 2) + 1);
                break;
            case 'M':
                customers[i].service_time = (((int)rand() % 3) + 2);
                break;
            case 'L':
                customers[i].service_time = (((int)rand() % 4) + 4);
                break;
            default:
                customers[i].service_time = 1;
                printf("WARNING: generate_and_sort_customers called with invalid seller type");
        }
        customers[i].served = 0;
    }

    // sort customers
    qsort(customers, N, sizeof(customer), compare_customers);

    for (i = 0; i < N; ++i) {
        customers[i].customer_num = i;
    }

    return (customers[0].arrival_time);
}


int serve_customer(customer * c, char * seller_type) {
    int i = 0;
    int row = 0;
    int col = 0;
    int sequence_num = 0;
    int row_sequence[3][10] = 
        {{0,1,2,3,4,5,6,7,8,9},
         {4,5,3,6,2,7,1,8,0,9},
         {9,8,7,6,5,4,3,2,1,0}};

    switch (*(seller_type)) {
        case 'H':
            sequence_num = 0;
            break;
        case 'M':
            sequence_num = 1;
            break;
        case 'L':
            sequence_num = 2;
            break;
        default:
            printf("WARNING: serve_customer called with invalid seller type");
    } 
    pthread_mutex_lock(&mutex);

    for (i = 0; i < 10; ++i) {
        row = row_sequence[sequence_num][i];
        for(col = 0; col < 10; ++col) {
            if (!seating_chart[row][col].served) {
                seating_chart[row][col] = *c;
                seating_chart[row][col].served = 1;
                pthread_mutex_unlock(&mutex);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

int get_seller_print_num(seller_id * id)
{
    int offset = 0;
    switch (id->seller_type) {
        case 'H':
            offset = 0;
            break;
        case 'M':
            offset = 1;
            break;
        case 'L':
            offset = 4;
            break;
        default:
            printf("WARNING: get_seller_print_num called with invalid seller type");
    }
    return (id->seller_num - offset + 1);
}

void stop_operations() {
    pthread_mutex_lock(&mutex);
    --threads_operating;
    pthread_mutex_unlock(&mutex);
}

//seller thread to serve one time slice (1 minute)
void * sell(void * sid)
{
    int i = 0;
    int num_served = 0;
    int num_arrived = 0;
    int service_time_remaining = 0;
    int overtime_minute = 0;

    seller_id id = *((seller_id *) sid);

    customer_array[id.seller_num] = calloc(N, sizeof(customer));
    generate_and_sort_customers(customer_array[id.seller_num], &id);

    //printf("seller created with type: %c and num: %d\n", id.seller_type, get_seller_print_num(&id)); 

    // check if cuustomer is available and serve them
    while ((num_served < N && curr_minute < MAX_MINUTE) || service_time_remaining > 0) {
        // synchronize for the time slice
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);
        pthread_mutex_unlock(&mutex);

        // take in arriving customers at this time slice
        while(num_arrived < N && customer_array[id.seller_num][num_arrived].arrival_time <= curr_minute) {
            printf("0:%02d customer %c%d%02d arrived in the queue\n", 
                curr_minute, id.seller_type, get_seller_print_num(&id), 
                customer_array[id.seller_num][num_arrived].customer_num + 1); 
            ++num_arrived;
        } 

        // if you arent serving anyone and someone is waiting serve them
        if (service_time_remaining <= 0 && num_arrived > num_served) {
            if (serve_customer(&customer_array[id.seller_num][num_served], &id.seller_type)) {
                // customer got a ticket
                printf("0:%02d customer %c%d%02d got a seat\n", 
                    curr_minute, id.seller_type, get_seller_print_num(&id), 
                    customer_array[id.seller_num][num_served].customer_num + 1); 
                service_time_remaining = customer_array[id.seller_num][num_served].service_time;
                print_seating_chart();
            }
            else {
                // customer did not get a ticket
                printf("0:%02d customer %c%d%02d failed to get a seat, STOPPING SALES\n", 
                    curr_minute, id.seller_type, get_seller_print_num(&id), 
                    customer_array[id.seller_num][num_served].customer_num + 1);
                stop_operations();
                return NULL;
            }

            customer_array[id.seller_num][num_served].served = 1;
            ++num_served;
        }

        // if you are currently serving someone then advance the service
        if (service_time_remaining > 0) {
            --service_time_remaining;
            if (service_time_remaining <= 0) {
                printf("%d:%02d customer %c%d%02d left the ticket window\n", 
                    (curr_minute / 60), (curr_minute % 60), id.seller_type, get_seller_print_num(&id), 
                    customer_array[id.seller_num][num_served-1].customer_num + 1);
                if (curr_minute >= MAX_MINUTE) {
                    stop_operations();
                    return NULL;
                }
            }
        }
    }

    stop_operations();
    return NULL;
}

void wakeup_all_seller_threads()
{
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

void print_seating_chart() {
    int row;
    int col;
    pthread_mutex_lock(&mutex);
    printf("\nPrinting seating chart\n");
    for (row = 0; row < 10; ++row) {
        for (col = 0; col < 10; ++col) {
            if (seating_chart[row][col].served != 0) {
                printf("%c%d%02d ", seating_chart[row][col].server.seller_type, 
                                    get_seller_print_num(&seating_chart[row][col].server), 
                                    seating_chart[row][col].customer_num + 1);
            }
            else {
                printf("---- ");
            }
        }
        printf("\n");
    }
    pthread_mutex_unlock(&mutex);
}

void debug_print_customers() {
    int i;
    int j;
    printf("\nPrinting customers");
    for (i = 0; i < 10; ++i) {
        printf("\n=================\nCustomers for seller %d\n", i);
        for (j = 0; j < N; ++j) {
            printf("c_n: %d, a_t: %d, s_t: %d, served: %d\n", 
                customer_array[i][j].customer_num + 1, customer_array[i][j].arrival_time, 
                customer_array[i][j].service_time, customer_array[i][j].served); 
        }
    }
}


