/********************************************************************/
/*********************************** File header.h ******************/
/********************************************************************/
#include <iostream>

/* for random number generator */
#define Nrnd 624
#define Mrnd 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

/***** Define simulation *****/
#define MS 1
#define NS 9001
#define TCPU 40
#define TQuantum 100
#define TInterRequest 16 //this is the i/o request time
#define TDiskService 10
#define TThink 8
#define TTS 1000000
#define FreeSystemMemory 7680
#define BarrierTime .4

//Parameters given in the lecture slide
#define context_switich_time 0.5 //context switching times
#define MemoryQueue 0
#define CPUQueue 1
#define DiskQueue 2
#define TotQueues 3

//Event types mapped to integers (used in create_event)
#define RequestMemory 0
#define RequestCPU 1
#define ReleaseCPU 2
#define RequestDisk 3
#define ReleaseDisk 4

#define NUM_CPUs 4
//#define CPU 1
#define DISK 0
#define EMPTY -1
#define LowPriority 0
#define HighPriority 1

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <assert.h>

using namespace std;

/* for random number generator */
static unsigned long mt[Nrnd];     /* the array for the state vector  */
static int mti=Nrnd+1;             /* mti==Nrnd+1 means mt[Nrnd] is not initialized */

/* simulator data structurs */
class Task { 
public:
	double tcpu;
	double tquantum;
	//added doubles for page fault and i/o
	double t_page_fault; 
	double t_barrier; //when parallel processes need to synch
	double tinterrequest;
	double start; 
	bool parallel; //is this a parallel process
	int CPU_number;
};  /**** Job list       ****/

Task task[NS];

class Events {                              /**** Event list           ****/
public:	
	int head;
	int tail;
	int q_length;
	double time[NS]; 
	int task[NS], event[NS];
};

Events event_list;

class Queue {  /**** Queues: 0 - memory queue, 1 - CPU queue, 2 - Disk queue*/
public:	
	int head;
	int tail;
	int q_length; //current length of the queue (was q)
	int n; //number of times anything has entered the queue (n++ on entry)
	int task[NS];
	double waiting_time; //accumulated waiting time (was ws)
	double change_time; //time of the last change (updated to global time on aquiring and freeing) (was tch)
	double ts; //total time EVERYTHING has been waiting in the queue: Ts= Ts+ (TG‐ change_time)*q_length
	double entry_times[NS]; //entry times for N processes (NS = N) (was tentry[NS])
};

Queue queue[3];

class Device {                              /***  Devices: 0 - CPU, 1 - Disk*/
public:	
	int busy;
	double change_time;
	double tser;
};

Device server[NUM_CPUs +1]; //add 1 for the disk

class Barrier_Queue{
public:
	list<int> waiting_processes;
};

Barrier_Queue barrier_synch_queue;

unsigned short seeds[3];
int inmemory=0;
int finished_tasks=0;
int MPL=MS;
int N=NS; /* inmemory, actual number of tasks in memory ****/

int finished_parallel_tasks = 0;

double sum_response_time=0.0;
double TTotal=TTS;
double memory_allocated=0.0;
void Process_RequestMemory(int, double), Process_RequestCPU(int, double),   /****  Event procedures      ****/
	Process_ReleaseCPU(int, double), Process_RequestDisk(int, double), Process_ReleaseDisk(int, double);   
double erand48(unsigned short xsubi[3]), random_exponential(double);                         /****  Auxiliary functions   ****/
void place_in_queue(int, double, int), create_event(int, int, double, int), init(), 
  stats();

//gets inter page fault time 
double inter_page_fault_time(){
	//f(m)
	double page_fault_time = 0.0;
	page_fault_time = memory_allocated/160.0;
	page_fault_time += 17.0;
	//the actual time = 1/f(m)
	page_fault_time = 1.0/page_fault_time;
	return page_fault_time;
}

bool CPUs_busy(){
	bool busy = true;
	//start at 1 to avoid disk
	for(int c = 1; c < NUM_CPUs; c++){
		if (server[c].busy == 0){
			busy = false;
		}
	}
	return busy;
}

int free_CPU(){
	//start at 1 to avoid disk
	for(int c = 1; c < NUM_CPUs+1; c++){
		if(server[c].busy == 0){
			return c;
		}
	}
	return -1;
}

//int = queue #, double = time of removal.  Note: this is pop_front();
int remove_from_queue(int, double);

/* for random number generator */
unsigned long genrand_int32(void);
double genrand_real2(void);


/********************************************************************/
/********************** File sim.c **********************************/
/* can be separated from header.c by using include below  ***********/
/* #include "header.h"                                             **/
int main(int argc, char *argv[])
{
  double global_time=0.0;
  int process, event;

/* read in parameters */
  if (argc>1) sscanf(argv[1], "%d", &MPL);
  if (argc>2) sscanf(argv[2], "%d", &N);
  if (argc>3) sscanf(argv[3], "%lf", &TTotal);

  init();
/***** Main simulation loop *****/
  while (global_time<=TTotal) {
/***** Select the event e from the head of event list *****/
    process=event_list.task[event_list.head];
    global_time = event_list.time[event_list.head];
    event = event_list.event[event_list.head];
    event_list.head=(event_list.head+1)%N;
    event_list.q_length--;
/***** Execute the event e ******/
    switch(event) {
    case RequestMemory: Process_RequestMemory(process, global_time);
      break;
    case RequestCPU: Process_RequestCPU(process, global_time);
      break;
    case ReleaseCPU: Process_ReleaseCPU(process, global_time);
      break;
    case RequestDisk: Process_RequestDisk(process, global_time);
      break;
    case ReleaseDisk: Process_ReleaseDisk(process, global_time);
    }
  }
  stats();
  return 0;
}

/********************************************************************/
/********************* Event Functions ******************************/
/********************************************************************/
//If there is space in memory, add to memory.  Otherwise, go to the memory (eligible) queue
void Process_RequestMemory(int process, double time)
{
/**** Create a Process_RequestCPU event or place a task in memory queue      ****/
  if (inmemory<MPL) {
    inmemory++;
    create_event(process, RequestCPU, time, LowPriority);
  }
  else place_in_queue(process, time, MemoryQueue);
}

void Process_RequestCPU(int process, double time)
{
  double release_time;

/**** Place in CPU queue if server is busy                       ****/
  if (CPUs_busy) place_in_queue(process,time,CPUQueue);
  else {
  	int CPU = free_CPU();
    server[CPU].busy=1;
    server[CPU].change_time=time;
    task[process].CPU_number = CPU;
/**** Find the time of leaving CPU                               ****/
    if (task[process].tcpu<task[process].tquantum) release_time=task[process].tcpu;
    else release_time=task[process].tquantum;
    if (release_time>task[process].tinterrequest) release_time=task[process].tinterrequest;
    if (task[process].t_page_fault >= 0 && release_time>task[process].t_page_fault) release_time=task[process].t_page_fault;
/**** Update the process times and create Process_ReleaseCPU event           ****/
    task[process].tcpu-=release_time;
    task[process].tinterrequest-=release_time;
    task[process].tquantum-=release_time;
    task[process].t_page_fault-=release_time;
    task[process].t_barrier-=release_time;
    create_event(process, ReleaseCPU, time+release_time+context_switich_time, LowPriority);
  }
}

void Process_ReleaseCPU(int process, double time)
{
	int queue_head;

 /**** Update CPU statistics                                            ****/
	server[CPU].busy=0;
	server[CPU].tser+=(time-server[CPU].change_time);
	queue_head=remove_from_queue(CPUQueue, time);           /* remove head of CPU queue ****/
	if (queue_head!=EMPTY) create_event(queue_head, RequestCPU, time, HighPriority);
	/**** Depending on reason for leaving CPU, select the next event       ****/
	if(task[process].t_page_fault ==  0){
	    task[process].t_page_fault = inter_page_fault_time();
	    create_event(process, RequestDisk, time, LowPriority);		
	}
	//this is the same for both parallel + interactive
	//both go back in the CPU queue
	else if(task[process].tinterrequest == 0){
		task[process].tinterrequest=random_exponential(TInterRequest);
		create_event(process, RequestDisk, time, LowPriority);	
	}
	else if (task[process].tcpu==0) {             /* task termination         ****/
	    task[process].tcpu=random_exponential(TCPU);
	    if(task[process].parallel == true){
		    //the process needs to go to the barrier synchronization queue
		    barrier_synch_queue.waiting_processes.push_back(process);
		    finished_parallel_tasks++;
		    if (barrier_synch_queue.waiting_processes.size() >= 6) {
			for(list<int>::iterator b = barrier_synch_queue.waiting_processes.begin();
			    b != barrier_synch_queue.waiting_processes.end(); b++ ) {

			    task[*b].t_barrier = BarrierTime;
			    create_event(*b, RequestCPU, time, LowPriority);
			}
		    }
	    }
	    //interactive processes:  need to make a new process at the monitor
	    else{ 
		    sum_response_time+=time-task[process].start;
		    finished_tasks++;
		    /**** Create a new task                                          ****/
		    task[process].tquantum  =   TQuantum;
		    task[process].tinterrequest = random_exponential(TInterRequest);
		    task[process].start=time+random_exponential(TThink);
		    task[process].t_page_fault = inter_page_fault_time();
		    task[process].t_barrier = BarrierTime;
		    create_event(process, RequestMemory, task[process].start, LowPriority);
		    inmemory--;
		    queue_head=remove_from_queue(MemoryQueue, time);
		    if (queue_head!=EMPTY) create_event(queue_head, RequestMemory, time, HighPriority);
	    }
	}
  	else if (task[process].tquantum==0) {          /* time slice interrupt     ****/
 		
	    task[process].tquantum=TQuantum;
	    //parallel processes only
	    if(task[process].parallel == true){	
		create_event(process, RequestCPU, time, LowPriority);
	    }
	    //interactive processes only
	    else{
		//first check the memory queue and put the first item into the CPU
		int process_from_queue = remove_from_queue(MemoryQueue, time);
		inmemory--; //we just removed a process from memory

		//this means we got a process form the queue
		if(process_from_queue != -1){
			//put this process in CPU
			inmemory++; //adding a process to the CPU queue
			create_event(process_from_queue, RequestCPU, time, HighPriority);


		}
		//either way, first process needs to request memory again
		create_event(process, RequestMemory, time, LowPriority);

		//this was in the code, but it's not what the assignment asks for
		//task[process].tquantum=TQuantum;
		//create_event(process, RequestCPU, time, LowPriority);

	    }
 	}
 	//the only thing left is barrier synchronization
  	else { //
    	//should never happen for interactive processes
 		if(task[process].parallel == false){
 			assert(false);
 		}
 		else{
			barrier_synch_queue.waiting_processes.push_back(process);
			if (barrier_synch_queue.waiting_processes.size() >= 6) {
			    for(list<int>::iterator b = barrier_synch_queue.waiting_processes.begin();
				b != barrier_synch_queue.waiting_processes.end(); b++ ) {

				task[*b].t_barrier = BarrierTime;
				create_event(*b, RequestCPU, time, LowPriority);
			    }
			}

 		}
 		
  	}	
}

void Process_RequestDisk(int process, double time)
{
/**** If Disk busy go to Disk queue, if not create Process_ReleaseDisk event    ****/
  if (server[DISK].busy) place_in_queue(process,time,DiskQueue);
  else {
    server[DISK].busy=1;
    server[DISK].change_time=time;
    create_event(process, ReleaseDisk, time+random_exponential(TDiskService), LowPriority);
  }
}

void Process_ReleaseDisk(int process, double time)
{
  int queue_head;
/**** Update statistics for Disk and create Process_RequestCPU event         ****/
  server[DISK].busy=0;
  server[DISK].tser+=(time-server[DISK].change_time);
  queue_head=remove_from_queue(DiskQueue, time);
  if (queue_head!=EMPTY) 
    create_event(queue_head, RequestDisk, time, HighPriority);
  create_event(process, RequestCPU, time, LowPriority);
}

/********************************************************************/
/******************* Auxiliary Functions ****************************/
/********************************************************************/

int remove_from_queue(int current_queue, double time)
{
  int process;

/**** If queue not empty, remove the head of the queue              ****/
  if (queue[current_queue].q_length>0) {
    process=queue[current_queue].task[queue[current_queue].head];
/**** Update statistics for the queue                               ****/
    queue[current_queue].waiting_time+=time
      -queue[current_queue].entry_times
    [queue[current_queue].head];
    queue[current_queue].ts+=
      (time-queue[current_queue].change_time)*queue[current_queue].q_length;
    queue[current_queue].q_length--;
    queue[current_queue].change_time=time;
/**** Create a new event for the task at the head and move the head ****/
    queue[current_queue].head=(queue[current_queue].head+1)%N;
    return(process);
  }
  else return(EMPTY);
}

void place_in_queue(int process, double time, int current_queue)
{
/**** Update statistics for the queue                               ****/
	queue[current_queue].ts+=
    	(time-queue[current_queue].change_time)*queue[current_queue].q_length;
  	queue[current_queue].q_length++;
  	queue[current_queue].n++;
  	queue[current_queue].change_time=time;
	/**** Place the process at the tail of queue and move the tail       ****/
  	queue[current_queue].task[queue[current_queue].tail]=process;
  	queue[current_queue].entry_times
 [queue[current_queue].tail]=time;
  	queue[current_queue].tail=(queue[current_queue].tail+1)%N;
}

void create_event(int process, int event, double time, int priority)
{
	int i, notdone=1, place=event_list.tail;
  
/**** Move all more futuristic tasks by one position                ****/
//If this were a list this wouldn't have to happen at all
	for(i=(event_list.tail+N-1)%N; notdone & (event_list.q_length>0); i=(i+N-1)%N) {
    	if ((event_list.time[i]<time) | ((priority==LowPriority) & (event_list.time[i]==time))) 
      		notdone=0;
    	else {
      		event_list.time[place]=event_list.time[i];
      		event_list.task[place]=event_list.task[i];
      		event_list.event[place]=event_list.event[i];
      		place=i;
    	}
    	if (i==event_list.head) notdone=0;
  	}
/**** Place the argument event in the newly created space           ****/
  	event_list.time[place]=time;
  	event_list.task[place]=process;
  	event_list.event[place]=event;
  	event_list.tail=(event_list.tail+1)%N;
  	event_list.q_length++;
}

void init()
{
  	int i;

	memory_allocated = FreeSystemMemory/MPL;

/**** Initialize structures                                         ****/
  	event_list.head=event_list.tail=event_list.q_length=0;
  	for(i=0;i<TotQueues;i++) {
    	queue[i].head=queue[i].tail=queue[i].q_length=queue[i].n=0;
    	queue[i].waiting_time=queue[i].ts=0.0;
    	queue[i].change_time=0;
  	}
  	for(i=0;i<2;i++) {
    	server[i].busy=0;
    	server[i].change_time=server[i].tser=0.0;
  	}
  	for(i=0;i<N+6;i++) {
		/**** Create a new task                                          ****/

		//all processes have this
		task[i].tcpu=random_exponential(TCPU);
	    task[i].tquantum  =   TQuantum;
	    task[i].tinterrequest = random_exponential(TInterRequest);
	    task[i].t_page_fault = inter_page_fault_time();
	    task[i].start=random_exponential(TThink);
	    task[i].t_barrier = BarrierTime;
	    task[i].CPU_number = -1;

    	//interactive processes
    	if(i < N){
	    	
	    	task[i].parallel = false;
	    	
	    }
	    else{ //these are parallel processes
	    	task[i].parallel = true;
	    }
	    create_event(i, RequestMemory, task[i].start, LowPriority);

	}

}

void stats()
{
/**** Update utilizations                                          ****/
	if (server[CPU].busy==1) server[CPU].tser+=(TTotal-server[CPU].change_time);
  	if (server[DISK].busy==1) server[DISK].tser+=(TTotal-server[DISK].change_time);

	/**** Print statistics                                             ****/

 	printf("System definitions: N %2d MPL %2d TTotal %6.0f\n",N, MPL, TTotal);
  	printf("utilizations are: CPU %5.2f Disk %5.2f\n", 100.0*server[0].tser/TTotal, 100.0*server[1].tser/TTotal);
  	printf("mean waiting time in qe %5.2f qCPU %5.2f qDisk %5.2f\n", 
	 queue[MemoryQueue].waiting_time?queue[MemoryQueue].waiting_time/(queue[MemoryQueue].n-queue[MemoryQueue].q_length):0.0,
	 queue[CPUQueue].waiting_time?queue[CPUQueue].waiting_time/(queue[CPUQueue].n-queue[CPUQueue].q_length):0.0,
         queue[DiskQueue].waiting_time?queue[DiskQueue].waiting_time/(queue[DiskQueue].n-queue[DiskQueue].q_length):0.0);
  	printf("mean queue length in qe %5.2f qCPU %5.2f qDisk %5.2f\n", 
	 queue[MemoryQueue].change_time?queue[MemoryQueue].ts/queue[MemoryQueue].change_time:0.0,
	 queue[CPUQueue].change_time?queue[CPUQueue].ts/queue[CPUQueue].change_time:0.0, 
	 queue[DiskQueue].change_time?queue[DiskQueue].ts/queue[DiskQueue].change_time:0.0);
  	printf("number of visits in qe  %5d qCPU %5d qDisk %5d\n", 
	 queue[0].n-queue[0].q_length,
	 queue[CPUQueue].n-queue[CPUQueue].q_length, 
	 queue[DiskQueue].n-queue[DiskQueue].q_length);
  	printf("average response time   %5.2f processes finished %5d\n",
	 sum_response_time/finished_tasks, finished_tasks); 
}

/*------------------------------ Random Number Generator --------------------------*/
   
void init_genrand(unsigned long s) {
  
	mt[0]= s & 0xffffffffUL;
  	for (mti=1; mti<Nrnd; mti++) {
    	mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti); 
    	mt[mti] &= 0xffffffffUL;
  }
}

unsigned long genrand_int32(void) {
	unsigned long y;
  	static unsigned long mag01[2]={0x0UL, MATRIX_A};
  	if (mti >= Nrnd) { 
    	int kk;
    
    	if (mti == Nrnd+1) init_genrand(5489UL); 

    	for (kk=0;kk<Nrnd-Mrnd;kk++) {
      		y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
      		mt[kk] = mt[kk+Mrnd] ^ (y >> 1) ^ mag01[y & 0x1UL];
    	}
    	for (;kk<Nrnd-1;kk++) {
      		y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
      		mt[kk] = mt[kk+(Mrnd-Nrnd)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    	}
    	y = (mt[Nrnd-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
    	mt[Nrnd-1] = mt[Mrnd-1] ^ (y >> 1) ^ mag01[y & 0x1UL];
    
    	mti = 0;
  	}
  
  	y = mt[mti++];
  	y ^= (y >> 11);
  	y ^= (y << 7) & 0x9d2c5680UL;
  	y ^= (y << 15) & 0xefc60000UL;
  	y ^= (y >> 18);
  
  	return y;
}

double genrand_real2(void) {
	return genrand_int32()*(1.0/4294967296.0); 
}

double random_exponential (double y) {
 	return -y*log(genrand_real2());
}
