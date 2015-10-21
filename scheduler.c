#define _CRT_SECURE_NO_WARNINGS
#include<stdio.h>
#include<stdlib.h>


struct ttask{
	int id;
	float offset;
	float period;
	float execution;
	float deadline;
};

typedef struct ttask Task;

struct tttask{
	Task* task;
	int priority;
	float remaining;
	float nextRun;
	float slackTime;
	int instance;
	int exist;
};

typedef struct tttask RunTask;
typedef unsigned long int _uint;


int ReadFile(Task**);
void Scheduler(char* filename,Task* tasks, int num,int (*scheduleComparer)(const void*,const void*), int isDynamic,int (*dynamicComparer)(const void*,const void*));
int RMSCompare(const void* a, const void* b){ /* Compare function for Rate Monotonic Schedule. */
	if ( (*(Task*)a).period > (*(Task*)b).period) /* if a's period higher than b's	*/
		return  1; /*send 'a' to back */
	else
		return -1; /*send 'b' to back */
}
/* For more information please check -> qsort, C Standart Library*/
int DMSCompare(const void* a, const void* b){
	if ( (*(Task*)a).deadline > (*(Task*)b).deadline)
		return  1;
	else
		return -1;
}
int EDFCompare(const void* a, const void* b){
	RunTask* ptr1 = (RunTask*)a;
	RunTask* ptr2 = (RunTask*)b;
	if ( ptr1->task->deadline + ptr1->nextRun > ptr2->task->deadline + ptr2->nextRun)
		return  1;
	else
		return -1;
}
int StaLSTCompare(const void* a, const void* b){
	Task* ptr1 = (Task*)a;
	Task* ptr2 = (Task*)b;
	if ( ptr1->deadline - ptr1->execution > ptr2->deadline- ptr2->execution)
		return  1;
	else
		return -1;
}
int LSTCompare(const void* a, const void* b){
	RunTask* ptr1 = (RunTask*)a;
	RunTask* ptr2 = (RunTask*)b;
	if ( ptr1->slackTime > ptr2->slackTime)
		return  1;
	else
		return -1;
}
void printTitles(FILE* output, int num){
	/*for (i = 0; i < num; i++)
	{
		fprintf(output,"T%d\t",i + 1);
	}*/
	fprintf(output,"StartTime\tStopTime\tTask\n");
	fprintf(output,"--------------------------------------------\n");
}
void debugPrint(Task* tasks, int num){ 
	int i;
	for (i = 0; i < num; i++)
	{
		printf("ID:%d O:%.2f P:%.2f E:%.2f D:%.2f\n", tasks[i].id,tasks[i].offset, tasks[i].period, tasks[i].execution, tasks[i].deadline);
	}
}
int gcd(_uint a,_uint b){ /* grand common divisor*/
	int c;
	while ( a != 0 ) {
		c = a; a = b%a;  b = c;
	}
	return b;

}
int lcm(_uint a,_uint b){ /*least common multipiler*/
	return (a*b)/gcd(a,b);
}
float lcmPeriod(Task* tasks,int num){ /*only works for number that has two digit after point 0.00*/
	int i;
	_uint result = 1;
	for (i = 0; i < num; i++)
	{
		result = lcm((_uint)(tasks[i].period*100),result);
	}
	return (float)result / 100.0f;
}
float MIN2(float a, float b){
	if(a<b)
		return a;
	return b;
}
void CreateTaskQueue(RunTask** pQueue, Task* tasks, int num, int (*compar)(const void*,const void*));
void PickTask(RunTask* tasks, int num,RunTask** currentTask);
float CalculateNext(RunTask** tasks, int num, RunTask* currentTask, float currentTime);
void UpdateQueue(RunTask** pTask,int num, RunTask* currentTask, float currentTime, float nextTime, 
				 int isDynamic, Task* tasks,int (*compar)(const void*,const void*),FILE* output);
int CheckDeadLineMiss(RunTask* tasks, int num, float nextTime);

/*
* Scheduler function is a general scheduling funciton which can be modified by only changing compare function
* Only issue is that the function needs two compare function one for static scheduling and one for dynamic.
* It could be done in only one function but there wasn't sufficient time to update code.
*/

int main(){

	Task* tasks = NULL;
	int num;
	num = ReadFile(&tasks);
	if (num == -1){
		printf("There was error to read tasks. Please check your input file and it's format.\n");
		return 1;
	}
	Scheduler("rate_monotonic.txt",tasks,num,RMSCompare,0,NULL);
	Scheduler("deadline_monotonic.txt",tasks,num,DMSCompare,0,NULL);
	Scheduler("earliest_deadline_first.txt",tasks,num,DMSCompare,1,EDFCompare);
	Scheduler("least_slack_time.txt",tasks,num,StaLSTCompare,1,LSTCompare);
	free(tasks);
	return 0;
}

void Scheduler(char* filename,Task* tasks, int num,int (*staticComparer)(const void*,const void*), int isDynamic, int (*dynamicComparer)(const void*,const void*)){
	FILE* output;
	float currentTime, finishTime, nextTime;
	int missedTaskID;
	RunTask* taskQueue = NULL,*currentTask = NULL;
	currentTime = 0.0;
	finishTime = lcmPeriod(tasks,num);

	taskQueue = (RunTask*)malloc( sizeof(RunTask)*(num + 1) ); /*PLUS ONE FOR IDLE*/
	CreateTaskQueue(&taskQueue,tasks,num,staticComparer);
	
	output = fopen(filename,"w");
	printTitles(output,num);

	while(currentTime < finishTime){
		PickTask(taskQueue,num, &currentTask);
		nextTime = CalculateNext(&taskQueue,num,currentTask,currentTime);
		missedTaskID = CheckDeadLineMiss(taskQueue,num,nextTime);
		UpdateQueue(&taskQueue, num, currentTask,currentTime,nextTime,isDynamic,tasks,dynamicComparer,output);
		if (missedTaskID){
			fprintf(output," T%d missed it's deadline.", missedTaskID);
			break;
		}
		currentTime = nextTime;
		fprintf(output,"\n");
	}
	fclose(output);
	free(taskQueue);
}
void PickTask(RunTask* tasks, int num, RunTask** currentTask){
	int i;
	*currentTask = &tasks[num]; /*IDLE*/
	for (i = 0; i < num; i++)
	{
		if(!tasks[i].exist) continue;
		if ((*currentTask)->priority > tasks[i].priority) /*higher prio*/
			*currentTask = &tasks[i];
	}

}
float CalculateNext(RunTask** pTask, int num, RunTask* currentTask, float currentTime){
	int i = 0;
	RunTask* tasks = *pTask;
	float nextRunTime = currentTime + currentTask->remaining;

	if(currentTask->priority == num + 1) { /*means IDLE*/
		nextRunTime = tasks[0].nextRun;
		for (i = 1; i < num; i++) /*find closest start task*/
		{
			nextRunTime = MIN2(nextRunTime,tasks[i].nextRun);
		}
	}
	else{ /*NOT IDLE*/
		for (i = 0; i < num; i++)
		{
			if( !tasks[i].exist && tasks[i].nextRun < nextRunTime /*closer run time*/){
				tasks[i].exist = 1;
				tasks[i].remaining = tasks[i].task->execution;
				nextRunTime = tasks[i].nextRun;
				tasks[i].slackTime = tasks[i].nextRun + tasks[i].task->deadline - nextRunTime - tasks[i].remaining;
			}
		}
	}
	return nextRunTime;

}
void UpdateQueue(RunTask** pTask,int num, RunTask* currentTask, float currentTime, float nextTime, 
				 int isDynamic, Task* tasks,int (*compar)(const void*,const void*),FILE* output){
	
	int i;
	RunTask* taskQueue = *pTask;
	float FLT_ERR = 0.001f; /* ONLY FOR 0.00 (two digits)*/
	if(currentTask->priority == num + 1){/*means IDLE*/
		fprintf(output,"%.2f\t\t%.2f\t\tIDLE->%.2f",currentTime,nextTime,nextTime-currentTime);
	}
	else{ /*NOT IDLE */
		currentTask->remaining = currentTask->remaining - (nextTime-currentTime);
		fprintf(output,"%.2f\t\t%.2f\t\tT%d,%d->%.2f",currentTime,nextTime,currentTask->task->id,currentTask->instance,nextTime-currentTime);
		if(currentTask->remaining <= FLT_ERR){ /*we are done */
			currentTask->remaining = 0.0f;
			currentTask->exist = 0;
			currentTask->nextRun = currentTask->instance * currentTask->task->period + currentTask->task->offset;
			currentTask->instance++;
		}
	}
	
	if (isDynamic){ /* if it's dynamic sort tasks with given function*/
		for (i = 0; i < num; i++)
		{
			taskQueue[i].slackTime = taskQueue[i].nextRun + taskQueue[i].task->deadline - nextTime - taskQueue[i].remaining; /* şüpheli*/
		}
		qsort(taskQueue, num, sizeof(RunTask),compar);
	}
	for (i = 0; i < num; i++) /*update new instances*/
		{ 
			if (isDynamic) taskQueue[i].priority = i; /*update priority*/
			
			if (taskQueue[i].nextRun <= nextTime && taskQueue[i].remaining == 0){
				taskQueue[i].exist = 1;
				taskQueue[i].remaining = taskQueue[i].task->execution;
			}
		}
}

int CheckDeadLineMiss(RunTask* tasks,int num, float nextTime){
	int i;
	for (i = 0; i < num; i++)
	{
		if (tasks[i].remaining > 0 && tasks[i].nextRun + tasks[i].task->deadline < nextTime)
		{
			return tasks[i].task->id;
		}

	}
	return 0;
}
void CreateTaskQueue(RunTask** pQueue, Task* tasks, int num, int (*compar)(const void*,const void*)){
	int i,j;
	RunTask* taskQueue = *pQueue;
	qsort(tasks, num, sizeof(Task),compar); /* sorting to find priorities*/
	
	for (i = 0; i < num; i++)
	{
		j = tasks[i].id - 1;
		taskQueue[j].task = &tasks[i];
		taskQueue[j].priority = i;
		taskQueue[j].instance = 1;
		taskQueue[j].nextRun = tasks[i].offset;
		taskQueue[j].remaining = tasks[i].offset ? 0 : tasks[i].execution;
		taskQueue[j].slackTime = taskQueue[j].nextRun + taskQueue[j].task->deadline - taskQueue[j].remaining; 
		taskQueue[j].exist = tasks[i].offset ? 0:1; /*if there is offset then it's not exist at beginning*/
	}
	/* VIRTUAL PROCESS FOR IDLE*/
	taskQueue[num].task = NULL;
	taskQueue[num].priority = num + 1; /*lowest priority*/
	taskQueue[num].exist = 0;
	taskQueue[num].nextRun = 0;
	taskQueue[num].remaining = 0;
	taskQueue[num].instance = 0;
}
int ReadFile(Task** tasks){

	FILE* input;
	int task_number;
	int i;
	Task* temp_task;
	input = fopen("tasks.txt", "r");
	
	if (input == NULL){ 
		printf("Please put tasks.txt file to same directory with application.\n");
		return -1; 
	}
	
	task_number = fgetc(input) - '0';
	if (task_number < 1)
		return -1;
	*tasks = (Task*)malloc(sizeof(Task) * task_number);
	temp_task = *tasks; /* easier way*/
	for ( i = 0; i < task_number; i++)
	{
		temp_task[i].id = i + 1;
		fscanf(input, "%f %f %f %f", &temp_task[i].offset,&temp_task[i].period, &temp_task[i].execution, &temp_task[i].deadline);
		if (ferror(input))
			return -1;
		if (temp_task[i].deadline == 0) /*if not explicitly given make it period*/
			temp_task[i].deadline = temp_task[i].period;
	}
	
	fclose(input);
	return task_number;
}
