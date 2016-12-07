#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>

struct Stock {
    char name[250];     // stock name
    double price;       // shares price
    int amount;         // amount of shares
    float percentage;   // increase or decrease percentage
    char up_or_down;    // whether the value has increased or decreased
};

struct Stock stock_list[1000];      // stock list from input file
struct Stock my_stock_list[1000];   // stock list of bought stocks

pthread_t tid[10002];       // thread ids array

int total_stock = 0;        // total of stock items in the file
float x, y, z;              // provided values for x, y, z
int zi;                     // z probability as int
double balance = 100000.0;  // initial balance
int finish = 0;             // flag to indicate to finish a loop regarding changing prices or printing balance

sem_t semaphore;            // semaphore

/**
 * Server thread
 */
void *pthread_server(void *arg) {

    /* loops while finish flag isn't 0 */
    while(!finish) {
        sem_wait(&semaphore); // decrements (locks) the semaphore pointed to by sem
        printf("Balance %lf \n", balance); // prints current balance
        sem_post(&semaphore); // increments (unlocks) the semaphore pointed to by sem
        sleep(5); // sleeps for 5 seconds
    }

    printf("Balance %lf \n", balance); // prints current balance
    pthread_exit(0); // ends thread
}

/**
 * Seller thread.
 */
void *pthread_seller(void *arg) {

    int r = rand() % (total_stock + 1); // generates a random integer between 0 and total_stock
    struct Stock stock_selected = my_stock_list[r]; // selects a stock randomly

    if ((stock_selected.amount > 0) &&
        ((stock_selected.up_or_down == '+' && stock_selected.percentage == x) || // price has increased by X%
        (stock_selected.up_or_down == '-' && stock_selected.percentage == y))) { // price has decreased by Y%
  
        sleep(2); // sleeps for 2 seconds

        int num_shares = rand() % stock_selected.amount + 1; // generates a random integer between 1 and stock_selected.amount
        stock_list[r].amount =+ num_shares; // add these shares to the original stock list
        my_stock_list[r].amount =- num_shares; // remove these shares from my stock list

        printf("SELL %s %d %lf \n", stock_selected.name, num_shares, stock_selected.price); // prints SELL operation

        sem_wait(&semaphore); // decrements (locks) the semaphore pointed to by sem
        balance =+ num_shares * stock_selected.price; // adds number of shares sold times their price to the current balance
        sem_post(&semaphore); // increments (unlocks) the semaphore pointed to by sem
    }

    pthread_exit(0); // ends thread
}

/**
 * Buyer thread.
 */
void *pthread_buyer(void *arg) {

    int r = rand() % (total_stock + 1); // generates a random integer between 0 and total_stock
    struct Stock stock_selected = stock_list[r]; // selects a stock randomly

    int ran = rand() % (100 + 1); // generates a random integer between 0 and 100

    /* buy decision is made with a probability of Z% */
    if (ran <= zi && stock_selected.amount > 0) {
        sleep(2); // sleeps for 2 seconds

        int num_shares = rand() % (stock_selected.amount) + 1; // generates a random integer between 1 and stock_selected.amount

        stock_list[r].amount =- num_shares; // decreases the number of shares from the original stock list
        my_stock_list[r].amount =+ num_shares; // increases the number of shares on my stock list

        sem_wait(&semaphore); // decrements (locks) the semaphore pointed to by sem
        balance =- num_shares * stock_selected.price; // decreases the balance according to the number of shares purchased
        sem_post(&semaphore); // increments (unlocks) the semaphore pointed to by sem

        printf("BUY %s %d %lf \n", stock_selected.name, num_shares, stock_selected.price); // prints BUY operation
    }

    pthread_exit(0); // ends thread
}

/**
 * Thread for changing the stock prices randomly.
 */
void *pthread_change(void *arg) {

    while (!finish) {
        int r = rand() % (total_stock + 1); // generates a random integer between 0 and total_stock
        struct Stock stock_selected = stock_list[r]; // selects a stock randomly

        int per = rand() % (100 + 1); // generates a random integer between 1 and 100
        float percentage = per / 100.0; // increased or decreased percentage
        int change_price_flag = rand() % 2; // generates a random integer between 0 and 1 in order to change the price

        stock_selected.percentage = percentage; // assigns percentage

        /* if change_price_flag is equals to 0 then decrease is applied */
        if (change_price_flag == 0) {
            stock_selected.up_or_down = '-';
            stock_selected.price = stock_selected.price - (stock_selected.price * percentage);
            printf("Decrease %s in %f%%. New price: %lf \n",stock_selected.name, stock_selected.percentage, stock_selected.price);

        /* if change_price_flag is equals to 1 then increase is applied */
        } else {
            stock_selected.up_or_down = '+';
            stock_selected.price = stock_selected.price + (stock_selected.price * percentage);
            printf("Increase %s in %f%%. New price: %lf \n",stock_selected.name, stock_selected.percentage, stock_selected.price);
        }

        sleep(1); // sleeps for 1 second
    }

    pthread_exit(0); // ends thread
}

/**
 * Program starts here
 */
int main(int argc, char *argv[]) {

    FILE * file;            // file pointer used to open and close the input file with the stock list
    char * line = NULL;     // line pointer used to read one line of the file
    size_t len = 0;         // length of the line
    int num_stock = 100;    // the amount of stock items to read from file
    int num_thread = 10000; // number of threads to create
    int comma_position;     // indicates the comma position on the read line so we know which field we are retrieving any given moment
    int i = 0;              // index for stock position

    srand(time(NULL));          // seeds the pseudo random number generator that rand() uses
    sem_init(&semaphore, 0, 1); // initializes the unnamed semaphore at the address pointed to by sem

    /* if the right number of arguments is not supplied then an error message is raised and the program ends */
    if (argc != 4) {
        printf("ERROR: Invalid syntax. Please try again\n");
        printf("You must indicate the X, Y and Z values as arguments\n");
        printf("For example: ./stock.o x y z\n");
        exit(EXIT_SUCCESS);
    }

    x = atof(argv[1]); // reads x from args
    y = atof(argv[2]); // reads y from args
    z = atof(argv[3]); // reads z from args

    zi = (int) (z * 100.0 + 0.5); // converts z value to an int in order to manipulate it easier

    file = fopen("quotes.csv", "r"); // opens csv input data file in "read" mode

    /* exits program if the file cannot be read */
    if (file == NULL)
        exit(EXIT_FAILURE);

    /* loops while reading each line of the file */
    while (getline(&line, &len, file) != -1 && i < num_stock) {

        /* reads stock name */
        char *comma = NULL;
        comma = strchr(line, ',');
        comma_position = (int)(comma - line);

        /* sets stock name */
        strncpy(stock_list[i].name, line, comma_position);
        stock_list[i].name[comma_position] = '\0';
        printf("%s ", stock_list[i].name);

        /* reads stock price */
        int size = strlen(line);
        char rest[size - comma_position];
        strncpy(rest, &line[comma_position + 1], size);
        rest[size - comma_position - 1] = '\0';

        comma = strchr(rest, ',');
        comma_position = (int)(comma - rest);

        /* sets stock price */
        char price[comma_position + 1];
        strncpy(price, rest, comma_position);
        price[comma_position] = '\0';
        sscanf(price, "%lf", &stock_list[i].price);
        printf("%lf ", stock_list[i].price);

        /* ignore date because we don't need this param */
        size = strlen(rest);
        char rest2[size - comma_position];
        strncpy(rest2, &rest[comma_position + 1], size);
        rest2[size - comma_position - 1] = '\0';

        comma = strchr(rest2, ',');
        comma_position = (int)(comma - rest2);

        /* ignore time because we don't need this param */
        size = strlen(rest2);
        char rest3[size - comma_position];
        strncpy(rest3, &rest2[comma_position + 1], size);
        rest3[size - comma_position - 1] = '\0';

        comma = strchr(rest3, ',');
        comma_position = (int)(comma - rest3);

        /* reads the increment or decrement percentage */
        size = strlen(rest3);
        char rest4[size - comma_position];
        strncpy(rest4, &rest3[comma_position + 1], size);
        rest4[size - comma_position - 1] = '\0';
 
        /* sets up_or_down attribute */
        stock_list[i].up_or_down = rest4[0];
        printf("%c ", stock_list[i].up_or_down);

        size = strlen(rest4);
        char rest5[size];
        strncpy(rest5, &rest4[1], size);
        rest5[size - 1] = '\0';

        comma = strchr(rest5, ',');
        comma_position = (int)(comma - rest5);

        /* sets percentage */
        char perc[comma_position + 1];
        strncpy(perc, rest5, comma_position);
        perc[comma_position] = '\0';
        float percentage = atof(perc);
        stock_list[i].percentage = percentage  / stock_list[i].price;
        printf("%f = %f \n", percentage, stock_list[i].percentage);

        stock_list[i].amount = 10000; // hardcoded number of shares to 10000

        /* fills my_stock_list */
        strcpy(my_stock_list[i].name, stock_list[i].name);
        my_stock_list[i].price = stock_list[i].price;
        my_stock_list[i].up_or_down = stock_list[i].up_or_down;
        my_stock_list[i].percentage = stock_list[i].percentage;

        int amount = rand() % (9999 + 1); // Generates a random integer between 0 and 9999
        my_stock_list[i].amount = amount;

        i++; // next line
    }

    total_stock = i; // total of stock item read

    fclose(file); // closes file

    /* free line */
    if (line)
        free(line);

    pthread_create(&tid[num_thread], NULL, pthread_server, NULL); // creates server thread
    pthread_create(&tid[num_thread + 1], NULL, pthread_change, NULL); // creates thread to change prices

    /* creates the threads up to the established number of threads to work with */
    for (i = 0; i < num_thread; i++) {
        pthread_create(&tid[i], NULL, pthread_buyer, NULL); // creates buyer thread
        i++;
        pthread_create(&tid[i], NULL, pthread_seller, NULL); // creates seller thread
    }

    /* suspends execution of the calling thread until the target thread terminates */
    for (i = 0; i < num_thread; i++)
        pthread_join(tid[i], NULL);

    finish = 1;
    pthread_join(tid[num_thread], NULL);
    pthread_join(tid[num_thread + 1], NULL);

    exit(EXIT_SUCCESS); // end of the program
}
