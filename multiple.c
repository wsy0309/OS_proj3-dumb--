#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include "fs.h"
#include "procqADT.h"


//msg key
#define QUEUE_KEY 3333
struct partition* part;

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
void fileMount();
int fileOpen(Pcb** pcb, char* filename,unsigned int mode);
void fileRead(Pcb* pcb, unsigned int buf_size);
void fileClose(Pcb* pcb);

key_t msgpid;

int main(){
	int pid, i;
	char* file;
	struct sigaction old_sa, new_sa;
	struct itimerval new_timer, old_timer;
	Pcb* next = NULL;
	
	runq = (Procq*)malloc(sizeof(Procq));
	runq = createProcq();

	fileMount();

	for(i = 0; i<10; i++){
	//for(i = 0; i<1; i++){
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
					//for(i = 0; i<1; i++){
						if(pcbs[i]->pid == msg.pid){
							sprintf(file,"file_%d",msg.filenum);
							int flag = fileOpen(&pcbs[i], file, msg.mode);
							if(flag != 0){
								//write도 할경우 여기서 타입을 비교!!
								fileRead(pcbs[i], msg.buf_size);
								//PA = checktlb(&pcbs[i]->tlb, &pcbs[i]->L1PT,&msg.buf);
								//printf("VA : 0x%08x -> PA : 0x%08x\n", &msg.buf, PA);
								fileClose(pcbs[i]);
							}
							RemoveProcq(runq, pcbs[i]);
							AddProcq(runq, pcbs[i]);
						}
					}
					next = runq->head->pcb;
					if(next != NULL){
						present = next;
						printf("global_tick(%d) schedule proc(%d)\n",global_tick, present->pid);
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
	if(global_tick > 11){
		for(int i = 0; i<10; i++){
		//for(int i = 0; i<1; i++){
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

	memset(&msg,0,sizeof(msg));

	srand(time(NULL));	
	msg.msgType = 1;
	msg.pid = getpid();
	msg.filenum = (rand() % 110) + 1;
	msg.mode = 32152775;
//	msg.buf_size = (rand() % 1024) + 1;
	msg.buf_size = (rand() % 50) + 1;
	
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

void fileMount(){
	
	int fd = open("disk.img",O_RDONLY);
            
    printf("superblock\n");
    part = (partition*) malloc(sizeof(partition));;
    read(fd,&part->s,sizeof(super_block));
    printf("%x\n",part->s.first_data_block);      

    int i;
    printf("inode!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    for(i=0;i<224;i++){
        read(fd,&(part->inode_table[i]),sizeof(inode));
        for(int j = 0;j<6;j++){
			printf("%x\t",part->inode_table[i].blocks[j]);
		}
		printf("\n");
    }   
            
    printf("datablock!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    for(i=0;i<4088;i++){
            read(fd,&part->data_blocks[i],sizeof(blocks));   
            //printf("%s\n",part->data_blocks[i].d);
    }   
    close(fd);
}


int fileOpen(Pcb** pcb, char* filename,unsigned int mode){
	inode* root = (inode*) malloc(sizeof(inode));
	root = &part->inode_table[part->s.first_inode];
	printf("root Dir inode : %x\n",part->s.first_inode);
	printf("root Dir size : %x\n",root->size);

	dentry* entry = (dentry*) malloc(sizeof(dentry));
	entry = &part->data_blocks[root->blocks[0]];
	printf("entry[0] : %s\n",entry);

	printf("dentry length : %d\n",entry->dir_length);
	unsigned int dir_size = root->size;
	printf("dir_size ; %d\n",dir_size);
	int flag = 0;

	while(dir_size > 0){
		if (strcmp(filename, entry->name) == 0){
			flag = 1;
			(*pcb)->desc = (Descriptor*) malloc(sizeof(Descriptor));
			(*pcb)->desc->file_name = filename;
			(*pcb)->desc->mode = mode;
			(*pcb)->desc->cur_node = &part->inode_table[entry->inode];

			printf("entry inode : %x, entry file_name : %s, entry mode : %d\n",entry->inode,(*pcb)->desc->file_name,(*pcb)->desc->mode);
			break;
		}
		else{
			dir_size = dir_size - entry->dir_length;
			entry = (char*)entry + entry->dir_length;
			//printf("entry : %s\n",entry);
			printf("entry file name : %s\n",entry->name);
			printf("dir_size : %d\n",dir_size);
		}

	}
	
	if(flag == 0){
		printf("No File!!\n");
		return flag;
	}
	printf("file open success!!\n");
	return flag;
}


void fileRead(Pcb* pcb, unsigned int buf_size){
		unsigned int PA;
		
		unsigned short block = (short*)malloc(sizeof(short));
		block = pcb->desc->cur_node->blocks[0];
		char* buf = (char*) malloc(sizeof(char) * buf_size);
		char* temp = part->data_blocks[block].d;

		for(int i = 0; i < buf_size; i++){
			buf[i] = temp[i];
		}
		
		printf("buf : %s\n",buf);
		printf("buf size : %d\n",sizeof(buf));
		printf("date : %s\n", part->data_blocks[block].d);
		printf("date size : %d\n", sizeof(part->data_blocks[block].d));

		PA = checktlb(&pcb->tlb, &pcb->L1PT, buf);

		free(buf);
}

void fileClose(Pcb* pcb){
	memset(pcb->desc,0,sizeof(Descriptor));
	free(pcb->desc);
	printf("pcd desc : %s, mode: %d\n", pcb->desc->file_name, pcb->desc->mode);

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




