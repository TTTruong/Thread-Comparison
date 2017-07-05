// Name: Truyen Truong
// Student Number: 100516976
// Final Project: OpenMP Implementation

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
 
#define INITIAL_BALANCE 10000.00
#define TRANSACTION_VALUE_MAX 10500.00
#define MONEY_LAUNDERING_AMOUNT 10000.00
#define TIMEOUT_VALUE 1000

int num_tellers;
int num_accounts;
int num_transactions;
struct account *bank_accounts;
struct send send_values;

int curr_transactions;

omp_lock_t *acc_locks;
omp_lock_t transactions_lock;
pthread_mutex_t monitor_mutex;
pthread_cond_t monitor_cond;

// Structure for data to be sent to the monitor thread from the
// bank teller threads.
struct send
{
	int type;
	int teller_id;
	int acc_id1;
	int acc_id2;
	double value;
};

// Structure for a bank account.
struct account
{
  int acc_id;
  double balance;
};

// Initial data to be sent to the monitor thread.
struct monitor_data
{
	int total_transactions;
};

// Initializing variables and printing program input.
void init(int arg1, int arg2, int arg3)
{
	num_tellers = arg1;
	num_accounts = arg2;
	num_transactions = arg3;
	curr_transactions = 0;
	acc_locks = calloc(num_accounts, sizeof(omp_lock_t));
	bank_accounts = calloc(num_accounts, sizeof(struct account));

	// Initialize locks and mutexes.
	omp_init_lock(&transactions_lock);
	pthread_mutex_init(&monitor_mutex, NULL);

	//Initialize locks and account variables
	for (int i=0; i < num_accounts; i++) {
   	omp_init_lock(&acc_locks[i]);
    bank_accounts[i].acc_id = i;
    bank_accounts[i].balance = INITIAL_BALANCE;
  }

  // Generate a new time seed.
  srand(time(NULL));
 
  // Prints out program input arguments.
	printf("\n~~~Values~~~\n");
	printf("Number of tellers: %i\n", num_tellers);
	printf("Number of accounts: %i\n", num_accounts);
	printf("Number of transactions: %i\n\n", num_transactions);
}

// Generates and returns a random float between float a and float b.
float RandomFloat(float a, float b)
{
  float random = ((float) rand()) / (float) RAND_MAX;
  float diff = b - a;
  float r = random * diff;
  return a + r;
}

// Deposit function.
void deposit(int teller_id, int acc_id, double value)
{	
	// Checks if the transaction is money laundering.
	if (value > MONEY_LAUNDERING_AMOUNT) {
		// Locks the monitor mutex.
		pthread_mutex_lock(&monitor_mutex);

		// Set values to send to the monitor thread with deposit type.
  	send_values.type = 1;
  	send_values.teller_id = teller_id;
  	send_values.acc_id1 = acc_id;
  	send_values.acc_id2 = -1;
  	send_values.value = value;

  	// Sends a condition signal to the monitor thread to execute.
  	pthread_cond_signal(&monitor_cond);
  	// Unlocks the monitor mutex.
    pthread_mutex_unlock(&monitor_mutex);

	} else {
		// Sets the account lock, deposits into the account and then unlocks the account lock.
		omp_set_lock(&acc_locks[acc_id]);
		bank_accounts[acc_id].balance += value;

		// Uncomment line below to print out deposit transactions.
		//printf("Deposited $%.2f into %d.\n", value, acc_id);

		omp_unset_lock(&acc_locks[acc_id]);
	}
}

// Withdraw function.
void withdraw(int teller_id, int acc_id, double value)
{
	// Checks if the transaction is money laundering.
	if (value > MONEY_LAUNDERING_AMOUNT) {
		// Locks the monitor mutex.
		pthread_mutex_lock(&monitor_mutex);

		// Sets values to send to the monitor thread with withdraw type.
  	send_values.type = 2;
  	send_values.teller_id = teller_id;
  	send_values.acc_id1 = acc_id;
  	send_values.acc_id2 = -1;
  	send_values.value = value;

  	// Sends a condition signal to the monitor thread to execute.
  	pthread_cond_signal(&monitor_cond);
  	// Unlocks the monitor thread.
    pthread_mutex_unlock(&monitor_mutex);

	} else if (bank_accounts[acc_id].balance >= value) {
		// Sets the account lock, withdraws from the account and then unlocks the account lock.
		omp_set_lock(&acc_locks[acc_id]);
		bank_accounts[acc_id].balance -= value;

		// Uncomment line below to print out withdraw transactions.
		//printf("Withdraw $%.2f into %d.\n", value, acc_id);

		omp_unset_lock(&acc_locks[acc_id]);
	}
}

// Transfer function.
void transfer(int teller_id, int to_id, int from_id, double value)
{
	// Checks if the transaction is money laundering.
	if (value > MONEY_LAUNDERING_AMOUNT) {
		// Locks the monitor mutex.
		pthread_mutex_lock(&monitor_mutex);

		// Set values to send to the monitor thread with transfer type.
  	send_values.type = 3;
  	send_values.teller_id = teller_id;
  	send_values.acc_id1 = to_id;
  	send_values.acc_id2 = from_id;
  	send_values.value = value;

		// Sends a condition signal to the monitor thread to execute.
  	pthread_cond_signal(&monitor_cond);
  	// Unlocks the monitor mutex.
    pthread_mutex_unlock(&monitor_mutex);

	} else if (bank_accounts[from_id].balance >= value) {

		int transferred = 0;
    int timeout = 0;

    // Keeps retrying until the money has transferred accounts.
    while (transferred == 0) {

    	// Locks the account the money is being sent to.
      omp_set_lock(&acc_locks[to_id]);

      // Tests to grab the account the money is being sent from.
      if (omp_test_lock(&acc_locks[from_id]) != 0) {
      	// If both accounts are locked, transfer the money.
        bank_accounts[from_id].balance -= value;
        bank_accounts[to_id].balance += value;

        // Uncomment line below to print out transfer transactions.
        //printf("Transferred $%.2f from %d into %d.\n", value, to_id, from_id);

        // Unlocks both accounts.
        omp_unset_lock(&acc_locks[to_id]);
        omp_unset_lock(&acc_locks[from_id]);

       	// Sets variable to say the money has been transferred.
        transferred = 1;
        // Reset timeout counter.
        timeout = 0;

      } else {
      	// Unlocks the first lock and increases timeout counter.
        omp_unset_lock(&acc_locks[to_id]);
        timeout++;

        // If the timeout counter hits 1000, it times out to prevent a deadlock.
        if (timeout > TIMEOUT_VALUE) {
          break;
        }
      }
    }
	}
}

// Thread to monitor money laundering.
void *monitor(void *t)
{
	// Get the monitor thread data.
	struct thread_data *data;
	data = (struct thread_data*) t;

	// Opens log file to print into.
	FILE *fp;
	fp = fopen("./OpenMPLog.txt", "w+");

	// Print error message if log file could not be opened.
	if (!fp) {
  	fprintf(stderr, "Can't open log file %s!\n", "OpenMPLog.txt");
  	exit(1);
	}

	// Keeps checking to see if anything needs to be logged.
	while (1) {
		// Locks the monitor mutex.
		pthread_mutex_lock(&monitor_mutex);
		// Waits for something to be logged.
  	pthread_cond_wait(&monitor_cond, &monitor_mutex);
  	
  	// Prints out the log to the file based on the transaction type.
  	if (send_values.type == 1) {
  		fprintf(fp, "Transaction: Deposit\n");
  		fprintf(fp, "Teller ID: %d\n", send_values.teller_id);
  		fprintf(fp, "Account ID: %d\n", send_values.acc_id1);
  		fprintf(fp, "Amount: $%.2f\n\n", send_values.value);
  	} else if (send_values.type == 2) {
  		fprintf(fp, "Transaction: Withdraw\n");
  		fprintf(fp, "Teller ID: %d\n", send_values.teller_id);
  		fprintf(fp, "Account ID: %d\n", send_values.acc_id1);
  		fprintf(fp, "Amount: $%.2f\n\n", send_values.value);
  	} else if (send_values.type == 3) {
  		fprintf(fp, "Transaction: Transfer\n");
  		fprintf(fp, "Teller ID: %d\n", send_values.teller_id);
  		fprintf(fp, "Account ID 1: %d\n", send_values.acc_id1);
  		fprintf(fp, "Account ID 2: %d\n", send_values.acc_id2);
  		fprintf(fp, "Amount: $%.2f\n\n", send_values.value);
  	} else {
  		printf("Invalid send values type.\n");
  	}
  	
  	// Resets the log values.
  	send_values.type = -1;
  	send_values.teller_id = -1;
  	send_values.acc_id1 = -1;
  	send_values.acc_id2 = -1;
  	send_values.value = -1.0;

  	// Unlocks the monitor mutex.
  	pthread_mutex_unlock(&monitor_mutex);
	}

	// Exits the monitor thread.
	pthread_exit(NULL);	
}

int main(int argc, char *argv[])
{
	// Requires 3 program input arguments.
	if (argc == 4) {

		// Initializes variables.
		init(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
 
 		// Sets number of threads.
		omp_set_num_threads(num_tellers+1);

		// Start the parallel section the current and total transaction are shared between the threads.
		#pragma omp parallel shared (num_transactions, curr_transactions)
		{
			//  Gets the thread id for each thread.
			int thread_id = omp_get_thread_num();

			// The thread with thread id 0 will be the monitor thread.
			if (thread_id == 0) {
				pthread_t monitor_thread;
				int error;
				pthread_attr_t attr;
				struct monitor_data monitor_data_values;
				pthread_attr_init(&attr);
  			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  			// Create monitor thread.
  			monitor_data_values.total_transactions = num_transactions;
  			error = pthread_create(&monitor_thread, &attr, monitor, (void *) &monitor_data_values);
    		if (error) {
     			printf("ERROR: create() monitor: %d\n", error);
      		exit(-1);
    		}
			} else {

				// Each thread will continue until the total number of transactions is reached.
				while (curr_transactions <= num_transactions) {

					// Lock the current transactions, increase the counter and unlock the lock.
					omp_set_lock(&transactions_lock);
					curr_transactions++;
					omp_unset_lock(&transactions_lock);

					// Generate a random transaction.
					int rand_tran = rand()%3+1;

					// Execute the random transaction.
					if (rand_tran == 1) {
						deposit(thread_id, rand()%num_accounts, RandomFloat(0.0, TRANSACTION_VALUE_MAX));
					} else if(rand_tran == 2) {
						withdraw(thread_id, rand()%num_accounts, RandomFloat(0.0, TRANSACTION_VALUE_MAX));
					} else if (rand_tran == 3) {
						transfer(thread_id, rand()%num_accounts, rand()%num_accounts, RandomFloat(0.0, TRANSACTION_VALUE_MAX));
					} else {
						printf("Invalid transaction selected.\n");
					}
				}
			}
		}

		// Destroy all account locks.
		for (int j=0; j<num_accounts; j++) {
			omp_destroy_lock(&acc_locks[j]);
		}
		
		free(acc_locks);
		free(bank_accounts);
	} else {
		// Lets the user know 3 inputs are required and what the 3 inputs are.
		printf("\nThis program accepts 3 inputs.\n");
		printf("# of tellers, # of accounts, and # of transactions.\n\n");
	}
}