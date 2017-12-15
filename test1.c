#include <stdio.h>
#include <fcntl.h> 
#include <unistd.h>   
#include "fs.h"
#include "procqADT.h"

partition* part;
Pcb* pcb;

void fileMount(){
	
	int fd = open("disk.img",O_RDONLY);
            
    //printf("superblock\n");
    part = (partition*) malloc(sizeof(partition));;
    read(fd,&part->s,sizeof(super_block));
    //printf("%x\n",part->s.first_data_block);      

    int i;
    //printf("inode!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    for(i=0;i<224;i++){
        read(fd,&(part->inode_table[i]),sizeof(inode));
        //for(int j = 0;j<6;j++){
	//		printf("%x\t",part->inode_table[i].blocks[j]);
	//	}
	//	printf("\n");
    }   
            
    //printf("datablock!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    for(i=0;i<4088;i++){
            read(fd,&part->data_blocks[i],sizeof(blocks));   
            //printf("%s\n",part->data_blocks[i].d);
    }   
    close(fd);
}
void fileOpen(char* filename,unsigned int mode){

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
			entry = (char*)entry + entry->dir_length;
			dir_size = dir_size - entry->dir_length;
			//printf("entry : %s\n",entry);
			printf("entry file name : %s\n",entry->name);
			printf("dir_size : %d\n",dir_size);
		}

	}
	
	if(flag == 0){
		printf("No File!!\n");
		return;
	}
	printf("file open success!!\n");
	return;
}

void fileRead(unsigned int buf_size){

	if(pcb->desc->mode == 32151679){
		unsigned short block = pcb->desc->cur_node->blocks[0];
		printf("date : %s\n", part->data_blocks[block].d);
		printf("date size : %d\n", sizeof(part->data_blocks[block].d));
	}
}

int main(){
	pcb = (Pcb*) malloc(sizeof(Pcb));
	fileMount();
	fileOpen("file_101",32151679);

	printf("pcb desc : %s, mode : %d, inode->block : %d \n", pcb->desc->file_name, pcb->desc->mode, pcb->desc->cur_node->blocks[0]);
	
	fileRead(1024);

	return;
}

