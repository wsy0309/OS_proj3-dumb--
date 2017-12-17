#include <stdio.h>
#include <fcntl.h> 
#include <unistd.h>   
#include "fs.h"
#include "procqADT.h"

partition* part;
Pcb* pcb;

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


int fileOpen(char* filename,unsigned int mode){

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
			pcb->desc = (Descriptor*) malloc(sizeof(Descriptor));
			pcb->desc->file_name = filename;
			pcb->desc->mode = mode;
			pcb->desc->cur_node = &part->inode_table[entry->inode];

			printf("entry inode : %x, entry file_name : %s, entry mode : %d\n",entry->inode,pcb->desc->file_name,pcb->desc->mode);
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


//buf size는 1024 내로랜덤하게 생성해야 할듯?
void fileRead(unsigned int buf_size){
	if(pcb->desc->mode == 32151679){
		unsigned short block = pcb->desc->cur_node->blocks[0];
		char* buf = (char*) malloc(sizeof(char) * buf_size);
		
		
		buf = strncpy(buf,part->data_blocks[block].d, buf_size);
		//buf = part->data_blocks[block].d;
		printf("buf : %s\n",buf);
		printf("buf size : %d\n",sizeof(buf));
		printf("date : %s\n", part->data_blocks[block].d);
		printf("date size : %d\n", sizeof(part->data_blocks[block].d));
	}
}

void fileClose(){
	memset(pcb->desc,0,sizeof(Descriptor));	
	printf("pcd desc : %s, mode: %d\n", pcb->desc->file_name, pcb->desc->mode);

}

int main(){
	pcb = (Pcb*) malloc(sizeof(Pcb));
	fileMount();
	int flag = fileOpen("file_3",32151679);
	if(flag != 0){
		printf("pcb desc : %s, mode : %d, inode->block : %d \n", pcb->desc->file_name, pcb->desc->mode, pcb->desc->cur_node->blocks[0]);
	
		fileRead(5);
		fileClose();
	}
	return;
}


