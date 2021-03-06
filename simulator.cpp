/********************************************************************/
/*********************************** File header.h ******************/
/********************************************************************/

/* for random number generator */
#define Nrnd 624
#define Mrnd 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

/***** Define simulation *****/
#define MS 1
#define NS 9000
#define TQuantum 100
#define TDiskService 10
#define TThink 5000
#define TTS 1000000
#define FreeSystemMemory 7680
/*#define TCPU 40 These are now global variables based on MissRate
  #define BarrierTime 400
  #define TInterRequest 16*/ //this is the i/o request time

//Parameters given in the lecture slide
#define context_switch_time 0.5 //context switching times
#define MemoryQueue 0
#define CPUQueue 1
//#define DiskQueue 2
//#define TotQueues 3

//Event types mapped to integers (used in create_event)
#define RequestMemory 0
#define RequestCPU 1
#define ReleaseCPU 2
#define RequestDisk 3
#define ReleaseDisk 4

//cache stats
#define MissRate .02
#define MissCost 51

#define NUM_CPUs 4
#define NUM_Disks 1
#define Num_Parallel 6
//#define DISK 0
#define EMPTY -1
#define LowPriority 0
#define HighPriority 1

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <assert.h>
#include <iostream>
#include <algorithm>
#include <vector>
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
	//	double t_barrier; //when parallel processes need to synch
	double tinterrequest;
	double start; 
	bool parallel; //is this a parallel process
	int CPU_number;
	int disk_number;
	bool urgent;
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

class Queue {  /**** Queues: 0 - memory queue, 1 - CPU queue, 2..NUM_Disks+2 Disk queues*/
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

Queue queue[NUM_Disks+2];

class Device {                              /***  Devices: 0...NUM_Disks Disks, NUM_Disks...(NUM_Disks + NUM_CPUs) CPUs*/
public:	
	int busy;
	double change_time;
	double tser;
	int num_context_switches;
	double idle_time_wd;
	double idle_time_wi;
};

Device server[NUM_Disks+NUM_CPUs]; //add 1 for the disk

class Barrier_Queue{
public:
	list<int> waiting_processes;
	double ts;
	double change_time;
};

Barrier_Queue barrier_synch_queue;

unsigned short seeds[3];
int inmemory=0;
int finished_tasks=0;
int MPL=MS;
int N=NS; /* inmemory, actual number of tasks in memory ****/
int TotQueues = 2 + NUM_Disks;

int finished_parallel_tasks = 0;

double TCPU = 20*(MissRate*50 + 1);
double TInterRequest = 8*(MissRate*50 + 1);

//This is calculated assuming the original tbs was 2sec, needs to
//be changed for simulation with tbs = .4 sec
double BarrierTime = 1000*(MissRate*50 + 1); 

bool PythonPrint = false;

double diskqs_change_time = 0;
double CPUq_change_time = 0;

double sum_response_time=0.0;
double TTotal=TTS;
double memory_allocated=0.0;
double TIP = 0;
int next_disk = 2; 

int barrier_times = 0;
//debug
int adding_to_barrier = 0;

double sum_urgent_response_time = 0;
double urgent_tasks_finished = 0;

void Process_RequestMemory(int, double), Process_RequestCPU(int, double),   /****  Event procedures      ****/
	Process_ReleaseCPU(int, double), Process_RequestDisk(int, double), Process_ReleaseDisk(int, double);   
double erand48(unsigned short xsubi[3]), random_exponential(double);                         /****  Auxiliary functions   ****/
void place_in_queue(int, double, int), create_event(int, int, double, int), init(), 
	stats();


//gets inter page fault time 
double inter_page_fault_time(){
	//f(m)
	double page_fault_time = 0.0;
	double instruction_fault_probability = pow(2,-(memory_allocated/160.0 + 17.0));

	double average_instruction_time = ((1-MissRate) * 1 + MissRate * MissCost);

	//the actual time = 1/f(m)
	page_fault_time = (1.0/instruction_fault_probability)*average_instruction_time;
	TIP = page_fault_time*pow(10,-6);
	return random_exponential(TIP);
}

bool CPUs_busy(){
	bool busy = true;
	for(int c = NUM_Disks; c < NUM_CPUs+NUM_Disks; c++){
		if (server[c].busy == 0){
			busy = false;
		}
	}
	return busy;
}

int free_CPU(){
	for(int c = NUM_Disks; c < NUM_CPUs+NUM_Disks; c++){
		if(server[c].busy == 0){
			return c;
		}
	}
	return -1;
}

bool disks_busy(){
	bool busy = true;

	for(int d = 0; d < NUM_Disks; d++){
		if(server[d].busy == 0){
			busy = false;
		}
	}
	return busy;
}

int free_Disk(){
	for(int d = 0; d < NUM_Disks; d++){
		if(server[d].busy == 0){
			return d;
		}
	}
	return -1;
}

void increment_disk(){
	next_disk = (next_disk + 1)%NUM_Disks + 2;
}

bool disk_queues_empty(){
	bool empty = true;

	for(int d = 0; d < NUM_Disks; d++){
		if(queue[d+2].q_length != 0){
			empty = false;
		}
	}
	return empty;
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

	//if there is ANY forth argument, we are printing for the python script
	if(argc>4){
		PythonPrint = true;
	}

	init();
	/***** Main simulation loop *****/
	while (global_time<=TTotal) {

		/***** Select the event e from the head of event list *****/
		process=event_list.task[event_list.head];
		//cout << "Time before change: " << global_time << endl;
		global_time = event_list.time[event_list.head];
		//cout << "Time after change: " << global_time << endl;
		event = event_list.event[event_list.head];
		event_list.head=(event_list.head+1)%(N+Num_Parallel);
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
	//cout<<"inmemory: " << inmemory << " MPL: " << MPL << endl;
	//cout<<"parallel?: "<< task[process].parallel << endl;
	if (inmemory<MPL) {
		inmemory++;
		create_event(process, RequestCPU, time, LowPriority);
	}
	else {
		place_in_queue(process, time, MemoryQueue);
		//cout<<"Something is going in the memQ"<<endl;
	}
}

void Process_RequestCPU(int process, double time)
{
	double release_time;

	/**** Place in CPU queue if server is busy                       ****/
	if (CPUs_busy()) {
		//This probably isn't necessary
		/*if(queue[CPUqueue].q_length == 0) {
			CPUq_change_time = time;
		}*/
		place_in_queue(process,time,CPUQueue);
	}
	else {
		int CPU = free_CPU();
		if(server[CPU].busy == 0){
			if(disk_queues_empty() && (queue[CPUQueue].q_length == 0)) {
				server[CPU].idle_time_wi+=time - max(max(server[CPU].change_time, diskqs_change_time), CPUq_change_time);
			}
			else if(!disk_queues_empty())
				server[CPU].idle_time_wd+=time - max(server[CPU].change_time, diskqs_change_time);
		}
		server[CPU].busy=1;
		server[CPU].change_time=time;
		task[process].CPU_number = CPU;
		/**** Find the time of leaving CPU                               ****/
		if (task[process].tcpu<task[process].tquantum) release_time=task[process].tcpu;
		else release_time=task[process].tquantum;
		if (release_time>task[process].tinterrequest) release_time=task[process].tinterrequest;
		if (release_time>task[process].t_page_fault) release_time=task[process].t_page_fault;

		/**** Update the process times and create Process_ReleaseCPU event           ****/
		task[process].tcpu-=release_time;
		task[process].tinterrequest-=release_time;
		task[process].tquantum-=release_time;
		task[process].t_page_fault-=release_time;
		server[CPU].num_context_switches++;
		create_event(process, ReleaseCPU, time+release_time+context_switch_time, LowPriority);
	}
}

void Process_ReleaseCPU(int process, double time){
    
	int queue_head;

	int CPU = task[process].CPU_number;
	/**** Update CPU statistics                                            ****/
	server[CPU].busy=0;
	server[CPU].tser+=(time-server[CPU].change_time);
	server[CPU].change_time = time;
	queue_head=remove_from_queue(CPUQueue, time);           /* remove head of CPU queue ****/
	if (queue_head!=EMPTY) {
		if(queue[CPUQueue].q_length == 0) {
			CPUq_change_time = time;
		}
		create_event(queue_head, RequestCPU, time, HighPriority);
	}
	/**** Depending on reason for leaving CPU, select the next event       ****/
	if(task[process].t_page_fault <=  0){
		//cout<<"page fault"<<endl;
		task[process].t_page_fault = inter_page_fault_time();
		create_event(process, RequestDisk, time, LowPriority);		
	}
	//this is the same for both parallel + interactive
	//both go back in the CPU queue
	else if(task[process].tinterrequest <= 0){
		//cout<<"disk request"<<endl;
		task[process].tinterrequest=random_exponential(TInterRequest);
		create_event(process, RequestDisk, time, LowPriority);	
	}
	else if (task[process].tcpu <= 0) {             /* task termination         ****/
		if(task[process].parallel == true){
			//cout<<"tcpu, parallel"<<endl;
			//the process needs to go to the barrier synchronization queue
			task[process].tcpu=random_exponential(BarrierTime);
			task[process].tinterrequest = random_exponential(TInterRequest);
			task[process].t_page_fault = inter_page_fault_time();
			task[process].tquantum  =   TQuantum;
			//update ts

			//cout << "Barrier Queue Size: " << barrier_synch_queue.waiting_processes.size() << " and global time: " << time << endl;
			barrier_synch_queue.ts+=(time-barrier_synch_queue.change_time)*barrier_synch_queue.waiting_processes.size();
			barrier_synch_queue.waiting_processes.push_back(process);
			//change time after ts calculation
			barrier_synch_queue.change_time = time;
			finished_parallel_tasks++;

			if (barrier_synch_queue.waiting_processes.size() >= Num_Parallel) {
				//don't have to update ts because we just did when the 6th one was added
				for(list<int>::iterator b = barrier_synch_queue.waiting_processes.begin();
				    b != barrier_synch_queue.waiting_processes.end(); b++ ) {
					create_event(*b, RequestCPU, time, LowPriority);
				}
				barrier_synch_queue.waiting_processes.clear();
				barrier_times++;
			}

		}
		//interactive processes:  need to make a new process at the monitor
		else{
			//cout<<"tcpu, interactive"<<endl;
			sum_response_time+=time-task[process].start;
			finished_tasks++;

			if(task[process].urgent) {
				sum_urgent_response_time+=time-task[process].start;
				urgent_tasks_finished++;
			}

			task[process].tcpu=random_exponential(TCPU);
			if(task[process].tcpu < 50) {
				task[process].urgent = true;
			}
			else {
				task[process].urgent = false;
			}
			/**** Create a new task                                          ****/
			task[process].tquantum  =   TQuantum;
			task[process].tinterrequest = random_exponential(TInterRequest);
			task[process].start=time+random_exponential(TThink);
			task[process].t_page_fault = inter_page_fault_time();
			create_event(process, RequestMemory, task[process].start, LowPriority);
			inmemory--;
			queue_head=remove_from_queue(MemoryQueue, time);
			if (queue_head!=EMPTY) create_event(queue_head, RequestMemory, time, HighPriority);
		}
	}
	else if (task[process].tquantum <= 0) {          /* time slice interrupt     ****/

		task[process].tquantum=TQuantum;
		//parallel processes only
		if(task[process].parallel == true){
			//cout<<"time quantum parallel"<<endl;
			create_event(process, RequestCPU, time, LowPriority);
		}
		//interactive processes only
		else{

			inmemory--; //we just removed a process from memory
			//cout<<"time quantum interactive"<<endl;
			//first check the memory queue and put the first item into the CPU
			queue_head = remove_from_queue(MemoryQueue, time);

			//this means we got a process form the queue
			if(queue_head != EMPTY){
				//put this process in CPU
				//inmemory++; //adding a process to the CPU queue
				create_event(queue_head, RequestMemory, time, HighPriority);


			}
			//either way, first process needs to request memory again
			create_event(process, RequestMemory, time, LowPriority);

			//this was in the code, but it's not what the assignment asks for
			//task[process].tquantum=TQuantum;
			//create_event(process, RequestCPU, time, LowPriority);

		}
	}
}

void Process_RequestDisk(int process, double time)
{
	/**** If Disk busy go to Disk queue, if not create Process_ReleaseDisk event    ****/
	if (disks_busy()) {
		if(disk_queues_empty()) {
			for(int c = NUM_Disks; c < NUM_CPUs+NUM_Disks; c++){
				if(server[c].busy == 0){
					server[c].idle_time_wi+=time - max(max(server[c].change_time, diskqs_change_time), CPUq_change_time);
				}
			}
			diskqs_change_time = time;
		}
		place_in_queue(process,time,next_disk);
		increment_disk();
	}
	else {
		int DISK = free_Disk();
		task[process].disk_number = DISK;
		server[DISK].busy=1;
		server[DISK].change_time=time;
		create_event(process, ReleaseDisk, time+random_exponential(TDiskService), LowPriority);
	}
}

void Process_ReleaseDisk(int process, double time)
{
	int queue_head;
	int DISK = task[process].disk_number;
	/**** Update statistics for Disk and create Process_RequestCPU event         ****/
	server[DISK].busy=0;
	server[DISK].tser+=(time-server[DISK].change_time);
	queue_head=remove_from_queue(DISK+2, time);
	if (queue_head!=EMPTY) {
		create_event(queue_head, RequestDisk, time, HighPriority);
		if(disk_queues_empty()){
			for(int c = NUM_Disks; c < NUM_CPUs+NUM_Disks; c++){
				if(server[c].busy == 0) {
					server[c].idle_time_wd+=time - max(server[c].change_time, diskqs_change_time);
				}
			}
			diskqs_change_time = time;
		}
	}
	create_event(process, RequestCPU, time, LowPriority);
}

	/********************************************************************/
	/******************* Auxiliary Functions ****************************/
	/********************************************************************/

	int remove_from_queue(int current_queue, double time)
	{
		int process;

		/**** If queue not empty, remove the head of the queue              ****/
		/*if(current_queue == MemoryQueue) {
		  cout<<"Removing from memory queue, length: "<< queue[current_queue].q_length<<endl;
		  }*/
		if (queue[current_queue].q_length>0) {
			process=queue[current_queue].task[queue[current_queue].head];
			/**** Update statistics for the queue                               ****/
			queue[current_queue].waiting_time+=time-queue[current_queue].entry_times[queue[current_queue].head];
			/*if(current_queue == MemoryQueue) {
			  cout<<"Queue wating time: "<<queue[current_queue].waiting_time<<endl;
			  }*/
			queue[current_queue].ts+=(time-queue[current_queue].change_time)*queue[current_queue].q_length;
			queue[current_queue].q_length--;
			queue[current_queue].change_time=time;
			/**** Create a new event for the task at the head and move the head ****/
			queue[current_queue].head=(queue[current_queue].head+1)%(N+Num_Parallel);
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
		queue[current_queue].entry_times[queue[current_queue].tail]=time;
		queue[current_queue].tail=(queue[current_queue].tail+1)%(N+Num_Parallel);
	}

	void create_event(int process, int event, double time, int priority)
	{
		int i, notdone=1, place=event_list.tail;

		/**** Move all more futuristic tasks by one position                ****/
		//If this were a list this wouldn't have to happen at all
		for(i=(event_list.tail+N+Num_Parallel-1)%(N+Num_Parallel); notdone & (event_list.q_length>0); i=(i+N+Num_Parallel-1)%(N+Num_Parallel)) {
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
		event_list.tail=(event_list.tail+1)%(N+Num_Parallel);
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
		for(i=0;i<NUM_Disks + NUM_CPUs;i++) {
			server[i].busy=0;
			server[i].change_time=server[i].tser=0.0;
			server[i].num_context_switches = 0;
			server[i].idle_time_wd =0;
			server[i].idle_time_wi = 0;
		}
		//initialize barrier queue
		barrier_synch_queue.ts = 0;
		barrier_synch_queue.change_time = 0;
		barrier_synch_queue.waiting_processes.clear();

		for(i=0;i<N+Num_Parallel;i++) {
			/**** Create a new task                                          ****/

			//all processes have this
			task[i].tquantum  =   TQuantum;
			task[i].tinterrequest = random_exponential(TInterRequest);
			task[i].t_page_fault = inter_page_fault_time();
			//cout << "Page Fault Time: " << inter_page_fault_time();
			task[i].CPU_number = -1;

			//interactive processes
			if(i < N){
				task[i].parallel = false;
				task[i].tcpu=random_exponential(TCPU);
				if(task[i].tcpu < 50) {
					task[i].urgent = true;
				}
				else {
					task[i].urgent = false;
				}
				task[i].start=random_exponential(TThink);
			}
			else{ //these are parallel processes
				task[i].parallel = true;
				task[i].tcpu=random_exponential(BarrierTime);
				task[i].start=0;
			}
			create_event(i, RequestMemory, task[i].start, LowPriority);

		}

	}

	void stats()
	{
		//if we are printing for python, we want to print these things, then return
		if(PythonPrint){
			//doubles for calculating average
			double Ucpu_avg = 0.0;
			double Ucpu_wi_avg = 0.0;
			double Ucpu_wd_avg = 0.0;
			double Ucpu_ps_avg = 0.0;
			vector<double> pup_holder;
			for(int CPU = 1; CPU < NUM_CPUs+1; CPU++){
			double pups = 100.0*((double)server[CPU].tser - ((double)server[CPU].num_context_switches*(double)context_switch_time))/(double)TTotal;
			double wi = 100.0*server[CPU].idle_time_wi/TTotal;
			double wd = 100.0*server[CPU].idle_time_wd/TTotal;
			pup_holder.push_back(pups);
			//get the sum of the doubles above
			Ucpu_avg += (100.0*server[CPU].tser/TTotal);
			Ucpu_ps_avg += pups;
			Ucpu_wi_avg += wi;
			Ucpu_wd_avg += wd;
			}
			Ucpu_avg /= NUM_CPUs;
			Ucpu_wi_avg /= NUM_CPUs;
			Ucpu_wd_avg /= NUM_CPUs;
			Ucpu_ps_avg /= NUM_CPUs;
			for (int p = 0; p < pup_holder.size(); p++){
				cout << pup_holder[p] << " " ;
			}
			double te =  queue[MemoryQueue].waiting_time?queue[MemoryQueue].waiting_time/(queue[MemoryQueue].n-queue[MemoryQueue].q_length):0.0;
			cout << Ucpu_ps_avg << " " << Ucpu_wi_avg << " " << Ucpu_wd_avg << " " << te << endl;
			return;
		}
		//maple calculations
		double m = FreeSystemMemory/MPL;
		double cmstart = 0.02; 
		double cm = 0.02;
		double cmc = 51; 
		double amatstart = 1+cmstart*(cmc-1);
		double amat = 1+cm*(cmc-1);

		double tinterio = 0.016/amatstart*amat;

		double tt = TThink;
		double tinterpage = pow(0.5,(-m/160.0-17.0))*pow(10.0,(-9.0)*amat);

		printf("System definitions: N %2d MPL %2d TTotal %6.0f\n",N, MPL, TTotal);

		//total simulation stats
		cout << "m " << m << " amat " << amat <<  " TIP " << TIP << endl; 


		/**** Update utilizations                                          ****/

		//for multiple CPUs
		for(int CPU = 0; CPU < NUM_CPUs + 1; CPU++){
			if (server[CPU].busy==1) server[CPU].tser+=(TTotal-server[CPU].change_time);
		}
		//old code
		//if (server[CPU].busy==1) server[CPU].tser+=(TTotal-server[CPU].change_time);
		//if (server[DISK].busy==1) server[DISK].tser+=(TTotal-server[DISK].change_time);

		//doubles for calculating average
		double Ucpu_avg = 0.0;
		double Ucpu_wi_avg = 0.0;
		double Ucpu_wd_avg = 0.0;
		double Ucpu_ps_avg = 0.0;
		//CPUs
		for(int CPU = 1; CPU < NUM_CPUs+1; CPU++){
			double pups = 100.0*((double)server[CPU].tser - ((double)server[CPU].num_context_switches*(double)context_switch_time))/(double)TTotal;
			double wi = 100.0*server[CPU].idle_time_wi/TTotal;
			double wd = 100.0*server[CPU].idle_time_wd/TTotal;
			cout << "CPU  " << CPU <<  " Ucpu " << 100.0*server[CPU].tser/TTotal << " Ucpu-wi  " << wi << " Ucpu-wd " << wd << " Ucpups " << pups << endl;
			//get the sum of the doubles above
			Ucpu_avg += (100.0*server[CPU].tser/TTotal);
			Ucpu_ps_avg += pups;
			Ucpu_wi_avg += wi;
			Ucpu_wd_avg += wd;
		}
		Ucpu_avg /= NUM_CPUs;
		Ucpu_wi_avg /= NUM_CPUs;
		Ucpu_wd_avg /= NUM_CPUs;
		Ucpu_ps_avg /= NUM_CPUs;
		cout << "CPUavg" <<  " Ucpu " << Ucpu_avg << " Ucpu-wi  " << Ucpu_wi_avg << " Ucpu-wd " << Ucpu_wd_avg << " Ucpups " << Ucpu_ps_avg << endl;

		//for the single disk
		double disk_average;
		for(int d = 0; d < NUM_Disks; d++) {
			cout << "disk "<< d << " utilization " << 100.0*server[d].tser/TTotal << endl;
			disk_average += 100.0*server[d].tser/TTotal;
		}
		cout << "disk average utilization " << disk_average/NUM_Disks << endl;

		/**** Print statistics                                             ****/
		//old code
		//printf("utilizations are: CPU %5.2f Disk %5.2f\n", 100.0*server[0].tser/TTotal, 100.0*server[1].tser/TTotal);
		for(int d = 0; d < NUM_Disks; d++) {
			double qDisk_mean_time = queue[d+2].waiting_time?queue[d+2].waiting_time/(queue[d+2].n-queue[d+2].q_length):0.0;
			cout << "mean waiting time in qDisk " << d << " " << qDisk_mean_time << endl;
		}


		double qe_mean = queue[MemoryQueue].waiting_time?queue[MemoryQueue].waiting_time/(queue[MemoryQueue].n-queue[MemoryQueue].q_length):0.0;
		double qCPU_mean = queue[CPUQueue].waiting_time?queue[CPUQueue].waiting_time/(queue[CPUQueue].n-queue[CPUQueue].q_length):0.0;

		cout << "mean waiting time in qe "<< qe_mean << " qCPU "<< qCPU_mean << " barrier " << barrier_synch_queue.ts/(barrier_times*Num_Parallel)
			 << " total barrier wait " << barrier_synch_queue.ts/(barrier_times) <<endl;

		cout <<"barrier ts: "<<barrier_synch_queue.ts<<endl;
		cout <<"number of barrier fills: "<<barrier_times<<endl;

		for(int d = 0; d < NUM_Disks; d++) {
			double qDisk_mean_length = queue[d+2].change_time?queue[d+2].ts/queue[d+2].change_time:0.0;
			cout << "mean queue length in qDisk " << d << " " << qDisk_mean_length << endl;
		}

		printf("mean queue length in qe %5.2f qCPU %5.2f \n", 
			   queue[MemoryQueue].change_time?queue[MemoryQueue].ts/queue[MemoryQueue].change_time:0.0,
			   queue[CPUQueue].change_time?queue[CPUQueue].ts/queue[CPUQueue].change_time:0.0);

		for(int d = 0; d < NUM_Disks; d++) {
			cout << "number of visits in qDisk " << d << " " << queue[d+2].n-queue[d+2].q_length << endl;
		}

		printf("number of visits in qe  %5d qCPU %5d \n", 
			   queue[MemoryQueue].n-queue[MemoryQueue].q_length,
			   queue[CPUQueue].n-queue[CPUQueue].q_length);
		cout << "average response time " << sum_response_time/finished_tasks <<  " processes finished " 
			 << finished_tasks << " parallel tasks finished " << finished_parallel_tasks << endl;

		cout << "Number of times releasing parallel processing:  " << barrier_times << " " << TTotal/barrier_times << endl;

		cout << "average urgent response time " << sum_urgent_response_time/urgent_tasks_finished << endl;
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
