/***********************************************
RR scheduling policy

time_quantum :  2
cpu_time     :  random
io_time      :  random

 + 2-Level paging
 + tlb
**********************************************/
#include "fs.h"
#include "procqADT.h" //linked list
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>

//msg key
#define QUEUE_KEY 3333


struct Pcb* pcbs[10];
struct Pcb* present;
struct msgNode msg;

struct Procq* runq;

int global_tick = 0;

int free_min = 0;
int free_max = 1000000;
int hit = 0;
int cold_miss = 0;
int conflict_miss = 0;

void PrintQueue(Procq *q);
void pAlarmHandler(int signo);
void cAlarmHandler(int signo);
void updateWaitq();
void child_action();
void searchShort();
void checkfree();
unsigned int checktlb(tlb_t** tlb, L1Page** L1PT, unsigned int VA);
unsigned int addrTranslator(L1Page** L1PT, unsigned int VA);

key_t msgpid;

int main(){
	int pid, i;
	struct sigaction old_sa, new_sa;
	struct itimerval new_timer, old_timer;
	Pcb* next = NULL;
	
	runq = (Procq*)malloc(sizeof(Procq));
	runq = createProcq();

	for(i = 0; i<10; i++){
		pid = fork();
		if(pid < 0){
			printf("fork error\n");
			exit(0);
		}
		else if(pid == 0){
			//child
			child_action();
			exit(0);
		}
		else{
			//parent
			pcbs[i] = (Pcb*)malloc(sizeof(Pcb));
			memset(pcbs[i],0,sizeof(Pcb));
			pcbs[i]->pid = pid;
			pcbs[i]->tlb = (tlb_t*)malloc(sizeof(tlb_t)*1024);
			AddProcq(runq, pcbs[i]);
		}
	}
	
	memset(&new_sa,0,sizeof(sigaction));
	new_sa.sa_handler = &pAlarmHandler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	//timer
	new_timer.it_value.tv_sec = 1;
	new_timer.it_value.tv_usec = 0;
	new_timer.it_interval.tv_sec = 1;
	new_timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &new_timer, &old_timer);

	if((msgpid = msgget((key_t)QUEUE_KEY, IPC_CREAT|0644)) == -1){
		printf("msgget error \n");
		exit(0);
	}
	printf("msg (key : %d, id : %d) created\n",QUEUE_KEY, msgpid);

	while(1){
		//메세지 종류에따라 io action 처리를할지
		//address translation을 할지 
		//PA = addrTranslater(현재실행 프로세스의 L1PT, 메세지에서 받은 VA)
		
		if(msgpid > 0){
		//receive msg
			if((msgrcv(msgpid, &msg, (sizeof(msg) - sizeof(long)), 0, 0)) > 0){
				if (msg.msgType == 1){
					for(i = 0; i<10; i++){
						if(pcbs[i]->pid == msg.pid){
							//여기에 이제 파일읽는걸 넣어야 겟지???????????
							//리드할때 va 매핑 하는것두!!!!!!!!!!!!!!!
							
							RemoveProcq(runq, pcbs[i]);
							AddProcq(runq, pcbs[i]);
							printf("global_tick (%d) proc(%d) sleep (%d) ticks\n", global_tick, pcbs[i]->pid, pcbs[i]->remain_io_time);
						}
					}
					next = runq->head->pcb;
					if(next != NULL){
						present = next;
						printf("global_tick(%d) schedule proc(%d)\n",global_tick, present->pid);
					}
				}
				else if(msg.msgType == 2){
                                	int i;
                                	unsigned int PA;
                                	for(i=0; i<10; i++){
                                        	PA = checktlb(&runq->head->pcb->tlb, &runq->head->pcb->L1PT,msg.vaddr[i]);
                                        	printf("VA :0x%08x -> PA :0x%08x\n",msg.vaddr[i], PA);
                               		}
				}
			}
		}
	}
	exit(0);
}
/*********************************************
 pAlarmHandler : parent signal handler
 - 종료: 일정 tick발생 후 child & parent kill
 - update time quantum : 업데이트후 AddProq
 - 자식 프로세서에 signal 보냄
**********************************************/
void pAlarmHandler(int signo){
	Pcb* next = NULL;
	global_tick++;

	present = runq->head->pcb;
	if(global_tick >= 30){
		for(int i = 0; i<10; i++){
			printf("parent killed child)(%d)\n",pcbs[i]->pid);
			kill(pcbs[i]->pid, SIGKILL);		
		}
		kill(getpid(), SIGKILL);
		exit(0);
	}

	printf("runq : ");
	PrintQueue(runq);

	kill(present->pid, SIGALRM);
}
/************************************************
 cAlarmHandler : child signal handler
 - update remain_cpu_time
 - io_action : remain_cpu_time == 0 인경우
*************************************************/

void cAlarmHandler(int signo){
	int mspid;	
	
	if((mspid = msgget((key_t)QUEUE_KEY, IPC_CREAT|0644)) == -1){
                printf("msgget error \n");
                exit(0);
                }   
	//읽을 파일, 모드, 사이즈 정해서 메시지 보낼꺼얌!!!!!!!!	

	if((msgsnd(mspid, &msg, (sizeof(msg) - sizeof(long)), IPC_NOWAIT)) == -1){
                printf("msgsnd error \n");
                exit(0);
        }
	return;
}

/************************************************
 child_action : initial signal action
*************************************************/

void child_action(int cpu_time){
	struct sigaction old_sa, new_sa;

	memset(&new_sa,0,sizeof(struct sigaction));
	new_sa.sa_handler = &cAlarmHandler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	msgpid = (key_t)malloc(sizeof(key_t));
	msgpid = -1;

	while(1){
	}
}

void PrintQueue(Procq* q){
	ProcqNode* tmp = (ProcqNode*)malloc(sizeof(ProcqNode));
 	tmp = q->head;
	if(tmp == NULL){
		printf("-\n");
		return;
	}
	do{
		printf("%d  ",tmp->pcb->pid);
		tmp = tmp->next;
	}while(tmp != NULL);
	printf("\n");
}

/***************************************************
 addrTranslator : VA -> PA
***************************************************/
unsigned int addrTranslator(L1Page** L1PT, unsigned int VA){
	
	unsigned int L1Index = VA >> 22;
	unsigned int L2Index = (VA & 0x003ff000) >> 12;
	unsigned int offset = VA & 0x00000fff;
	
	unsigned int PA = 0;
	int pfn = 0;

	//해당 프로세스의 L1PageTable이 없을 경우
	if (*L1PT == NULL){
		pfn = free_min;
		free_min++;
		checkfree();

		*L1PT = (L1Page*)malloc((sizeof(L1Page)*1024));
		memset(*L1PT, 0, sizeof(L1Page)*1024);
	} 
	
	//해당 L1PTE에 mapping이 없을 경우
	if ((*L1PT)[L1Index].valid == 0){
		pfn = free_min;
		free_min++;
		checkfree();

		(*L1PT)[L1Index].L2PT = (L2Page*)malloc((sizeof(L2Page)*1024));
		memset((*L1PT)[L1Index].L2PT, 0, sizeof(L2Page)*1024);
		
		(*L1PT)[L1Index].baseAddr = (0x1000)*pfn;
		(*L1PT)[L1Index].valid = 1;
	}

	//해당 L2PTE에 mapping이 없을 경우
	if ((*L1PT)[L1Index].L2PT[L2Index].valid == 0){
		pfn = free_min;
		free_min++;
		checkfree();

		(*L1PT)[L1Index].L2PT[L2Index].page = (unsigned int*)malloc((sizeof(unsigned int)*1024));
		memset((*L1PT)[L1Index].L2PT[L2Index].page, 0, sizeof(unsigned int)*1024);

		(*L1PT)[L1Index].L2PT[L2Index].baseAddr = (0x1000)*pfn;
		(*L1PT)[L1Index].L2PT[L2Index].valid = 1;
	}
	
	//PA 계산
	PA = (*L1PT)[L1Index].L2PT[L2Index].baseAddr|offset;
	//printf("VA :0x%08x -> PA :0x%08x",VA, PA);

	return PA;
	
}
/***********************************************
 checkfree : freepagelist check
  - kill
***********************************************/
void checkfree(){
    
    if(free_min == free_max){
        for(int i = 0; i<10; i++){
            printf("parent killed child)(%d)\n",pcbs[i]->pid);
            kill(pcbs[i]->pid, SIGKILL);
        }
        kill(getpid(), SIGKILL);
        exit(0);
    }
}

/*************************************************************************
checktlb : tlb에 VA - PA mapping 존재
 - valid : tlb에 해당 index의 pfn확인 (유무)
    -> L1Index 확인(일치, 불일치)
        -> 불일치의 경우pageTable 생성
 - hit : tlb에 해당 L2index, L1Index의 pfn이 존재하는 경우
 - cold miss : tlb에 해당 L2index의 valid = 0일 경우
   -> pageTable 생성 후 valid = 1로 set
 - conflict miss : tlb에 해당 L2index의 pfn가 존재하나 L1Index가 다를 경우
*************************************************************************/

unsigned int checktlb(tlb_t** tlb, L1Page** L1PT, unsigned int VA) {
    unsigned int L1Index = VA >> 22;
    unsigned int L2Index = (VA & 0x003ff000) >> 12;
	unsigned int offset = VA & 0x00000fff;    
	unsigned int PA;
    
	//tlb에 data존재
    if ((*tlb)[L2Index].valid) {
        if ((*tlb)[L2Index].L1Index != L1Index) {
            printf("    CONFLICT MISS : tlb[%d] \n", L2Index);
            //pageTable 생성
			PA = addrTranslator(L1PT, VA);

			//cache 채우기
            (*tlb)[L2Index].L1Index = L1Index;
            (*tlb)[L2Index].pfn = (*L1PT)[L1Index].L2PT[L2Index].baseAddr;
            conflict_miss++;
        }
		else {
            hit++;
			PA = (*tlb)[L2Index].pfn|offset;
            printf("    HIT : tlb[%d] \n",L2Index);
        }
    }
    //valid = 0일 경우
    else {
        printf("    COLD MISS : tlb[%d] \n",L2Index);
        
		PA = addrTranslator(L1PT, VA);

        (*tlb)[L2Index].L1Index = L1Index;
        (*tlb)[L2Index].valid = 1;
        (*tlb)[L2Index].pfn = (*L1PT)[L1Index].L2PT[L2Index].baseAddr;
        cold_miss++;
    }
    printf("    L1Index = 0x%x \n", L1Index);
	
	return PA;
}


