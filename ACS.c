

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>

// use this struct to record the customer information 
struct customer_info{ 
    int user_id;
	int class_type; //0: customer, 1: business
	int arrival_time;
	int service_time;
	int customer_status; // 1: cleerk1, 2: cleerk2, 3: cleerk3, 4: cleerk4, 5: cleerk5
	int position_in_line; // position in the all_customer[]
	double start_waiting; // customer start waiting time
	double end_waiting; // customer end waiting time
};

// use this struct to record the clerk information 
struct clerk{
	int clerk_id;
	int clerk_status; // 99 free, 1 busy
};

/* global variables */
struct customer_info* econ_queue[1]; //store econ customer
struct customer_info* busi_queue[1]; //store busi customer
struct customer_info* all_customers;
struct clerk clerks[5];
int number_of_econ = 0;
int number_of_busi = 0;
int length_econ_line = 0;
int length_busi_line = 0;
int total_customers = 0;
double econ_total_wait = 0;
double busi_total_wait = 0;
struct timeval init_time;
pthread_mutex_t queue_mutex[1]; 
pthread_cond_t queue_convar[2]; // 0: econ class, 1: busniness class
pthread_mutex_t clerk_mutex[5]; // 0: clerk1, 1: clerk2, 2: clerk3, 3: clerk4, 4: clerk5
pthread_cond_t clerk_convar[5];	// 0: clerk1, 1: clerk2, 2: clerk3, 3: clerk4, 4: clerk5

/* Read file line by line and  and store the useful information*/
void readfile(char* file){
	FILE* open_file = fopen(file,"r");
	if(open_file == NULL){
		printf("Empty file");
		exit(1);
	}
	//scan the file and set the space for each queue
	fscanf(open_file, "%d", &total_customers); //get the total customer
	econ_queue[0] = (struct customer_info*) malloc(total_customers * sizeof(struct customer_info));
	busi_queue[0] = (struct customer_info*) malloc(total_customers * sizeof(struct customer_info));
	all_customers =  (struct customer_info*) malloc(total_customers * sizeof(struct customer_info));

	struct customer_info* temp;  // temporary variable to store each line as nodes
    temp = (struct customer_info*) malloc(total_customers *sizeof(struct customer_info)); 

	for( int i = 0; i < total_customers; i++ ){
		//continue to read the rest of lines
		fscanf(open_file, "%d:%d,%d,%d", &temp -> user_id, &temp -> class_type, &temp -> arrival_time, &temp -> service_time);

		temp -> customer_status = 99; //set 99 means no one serve yet. and update to clerk_id later if clerk serve
		temp -> position_in_line = i; // index of the list
		all_customers[i] = *temp; //store each info from to this array

		if(temp -> class_type == 0){
			number_of_econ += 1; //count total number of economy class customers
		}
		else if(temp -> class_type == 1){
			number_of_busi += 1; //count total number of business class customers
		}
		else{
			printf("Invalid Customer");
			exit(1);
		}
	}
	fclose(open_file);
}
/* add differnt type coustomer (0 or 1) into the correct queue, and increase the queue length*/
void add_customer(struct customer_info* customer){
	if (customer -> class_type == 0){ // if the class type is economy class
		econ_queue[length_econ_line] = customer; // add to list
		length_econ_line++; 
	}
	else if(customer -> class_type == 1 ){ // if the class type is in business class	
		busi_queue[length_busi_line] = customer; // add to list
		length_busi_line++; 
	}
	else{
		printf("Invalid Customer");
		exit(1);
	}
}
/* pop 0 or 1 type customer from the econ_queue or busi_queue and decrease the length of match list, return the position of poped customer */
int pop_customer(int class_type){
	int position = 0;

	if(class_type == 0){ // econ type
		position = econ_queue[0] -> position_in_line;// get positon
		for(int i = 0; i < length_econ_line -1; i++){
			econ_queue[i] = econ_queue[i+1];
		}
		length_econ_line--;
	}
	else if(class_type == 1){ //busi type
		position = busi_queue[0] -> position_in_line;// get position
		for(int i = 0; i < length_busi_line -1; i++){
			busi_queue[i] = busi_queue[i+1];
		}
		length_busi_line--;
	}
	else{
		printf("Wrong Class Type");
		exit(1);
	}
	return position;
}

/* get current time, and return current time*/
double current_time(){
	struct timeval current;
	double init;
	double curr;
	double current_time;
	gettimeofday(&current, NULL);
	init = (init_time.tv_sec) * 1000000 + init_time.tv_usec;
	curr = (current.tv_sec) * 1000000 + current.tv_usec;
	current_time = (curr - init) / 1000000.0f;
	return current_time;
}	

/*customer entry the threads*/
void* customer_entry(void* customer_info){
	struct customer_info* p_myInfo = (struct customer_info*) customer_info;
	usleep(p_myInfo -> arrival_time * 100000);
	// sample format 
	fprintf(stdout, "A customer arrives: customer ID %2d. \n", p_myInfo->user_id);
	//lock the queue
	pthread_mutex_lock(&queue_mutex[0]);
	//add customer into match queue (econ or busi queue)
	int type = p_myInfo-> class_type;
	add_customer(p_myInfo);
	// print sample format depends on different class type and print the information 
	if(type == 0){
		printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d.\n", type, length_econ_line);
	}
	else if(type == 1){
		printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d.\n", type, length_busi_line);
	}
	else{
		printf("Wrong Class Type");
		exit(1);
	}
	// store the time that customer start waiting
	p_myInfo -> start_waiting = current_time();
	while(p_myInfo -> customer_status == 99){  // if the customer is not served by clerk
		//depends on the class type, wait for conditional variable 0 or 1, and mutex
		pthread_cond_wait(&queue_convar[p_myInfo->class_type], &queue_mutex[0]);
	}
	// store the current time as the time finish waiting and get service
	p_myInfo -> end_waiting = current_time();
	// update total waiting time for econ and busi customer
	double wait_time = (p_myInfo -> end_waiting) - (p_myInfo -> start_waiting); 
	if(type == 0){
		econ_total_wait += wait_time;
	}
	else if(type == 1){
		busi_total_wait += wait_time;
	}
	else{
		printf("Error: waiting time error");
		exit(1);
	}
	// start to server
	printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d.\n", p_myInfo -> end_waiting, p_myInfo -> user_id, p_myInfo -> customer_status);
	// unlock the queue
	pthread_mutex_unlock(&queue_mutex[0]);
	//get some time to serve
	usleep(p_myInfo -> service_time * 100000);
	printf("-->>> A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d.\n", current_time(), p_myInfo -> user_id, p_myInfo -> customer_status);
	//get the served clerk index by using customer_status: 0:clerk1, 1:clerk2 ... 4:clerk4
	int served_clerk_index = (p_myInfo -> customer_status) -1 ;
	clerks[served_clerk_index].clerk_status = 99; //set the clerk status to free
	//send the signal to clerk, and able to serve next customer
	pthread_cond_signal(&clerk_convar[served_clerk_index]);
	//pthread_cond_broadcast(&queue_convar[0]);
	//pthread_cond_broadcast(&queue_convar[0]);
}

/*clerk entry the threads*/
void* clerk_entry(void* clerkNum){
	struct clerk* clerk_Info = (struct clerk*) clerkNum;
	int position;
	bool loop = true;
	while(loop){
		// lock the queue
		pthread_mutex_lock(&queue_mutex[0]);
		// if busi line is not empty, get the buis customer from the queue
		if(length_busi_line > 0){
			position = pop_customer(1); // pop a customer from busi_queue
			// set the clerk is busy 
			clerks[(clerk_Info -> clerk_id) -1].clerk_status = 1;
			//send a clerk to the customer, update customer_status
			all_customers[position].customer_status = clerk_Info -> clerk_id;
			//send the signal to queue, and able to serve next customer
			pthread_cond_broadcast(&queue_convar[1]);
			//send the signal to clerk
			pthread_cond_broadcast(&clerk_convar[clerk_Info -> clerk_id -1 ]);
			// unlock the queue
			pthread_mutex_unlock(&queue_mutex[0]);
		}
		// if econ line is not empty, get the econ customer from the queue
		else if(length_econ_line > 0){
			position = pop_customer(0); // pop a customer from econ_queue
			// set the clerk is busy 
			clerks[(clerk_Info -> clerk_id) -1].clerk_status = 1;
			//send a clerk to the customer, update customer_status
			all_customers[position].customer_status = clerk_Info -> clerk_id -1;
			//send the signal to queue, and able to serve next customer
			pthread_cond_broadcast(&queue_convar[0]);
			//send the signal to clerk
			pthread_cond_broadcast(&clerk_convar[clerk_Info -> clerk_id -1 ]);
			// unlock the queue
			pthread_mutex_unlock(&queue_mutex[0]);
		}
		else{
			// unlock clerk
			pthread_mutex_unlock(&queue_mutex[0]); 
			// wait for waiting thread all come back
			usleep(300); 
		}
		//lock the busy clerk
		int clerk_id_index = (clerk_Info -> clerk_id) -1;
		pthread_mutex_lock(&clerk_mutex[clerk_id_index]);
		// if the clerk is busy
		if(clerks[clerk_id_index].clerk_status == 1){
			pthread_cond_wait(&clerk_convar[clerk_id_index], &clerk_mutex[clerk_id_index]); // wait clerk 
		}
		// unlock clerk
		
		pthread_mutex_unlock(&clerk_mutex[clerk_id_index]);
	}
}

struct timeval init_time;
int main(int argc, char* argv[]){
	gettimeofday(&init_time, NULL);
	readfile(argv[1]);
	//initialize queue_mutex and queue_convar
	pthread_mutex_init(&queue_mutex[0], NULL);
	pthread_cond_init(&queue_convar[0], NULL);
	pthread_cond_init(&queue_convar[1], NULL);
	//initialize clerk_mutex and clerk_convar
	pthread_t clerk_thread[5];
	for(int i = 0 ; i < 5; i++){
		pthread_mutex_init(&clerk_mutex[i], NULL);
		pthread_cond_init(&clerk_convar[i], NULL);
	}
	//create clerk thread and set value for clerk
	for(int i = 0 ; i < 5; i++){
		//set value for clerk 
		clerks[i].clerk_id = i + 1;
		clerks[i].clerk_status = 99;
		//create clerk thread
		pthread_create(&clerk_thread[i], NULL, clerk_entry, (void*)&clerks[i]);
	}
	//create customer thread
	pthread_t customer_thread[total_customers];
	for(int i = 0; i < total_customers; i++){
		pthread_create(&customer_thread[i], NULL, customer_entry, (void*)&all_customers[i]);
	}
	//join the customer thread, wait one by one
	int count = 0;
	while( count < total_customers){
		pthread_join(customer_thread[count], NULL);
		count++;
	}

	printf("All jobs done...\n");
	printf("The average waiting time for all customers in the system is: %.2f seconds.\n", ((econ_total_wait + busi_total_wait)/total_customers)); 
	printf("The average waiting time for all business-class customers is: %.2f seconds.\n", busi_total_wait/length_busi_line);
	printf("The average waiting time for all economy-class customers is: %.2f seocnds.\n", econ_total_wait/length_econ_line);
}