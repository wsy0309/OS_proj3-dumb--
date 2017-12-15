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
                printf("%x\n",part->inode_table[i].blocks[j]);
        }   
            
        printf("datablock!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        for(i=0;i<4088;i++){
                read(fd,&part->data_blocks[i],sizeof(blocks));   
                printf("%s\n",part->data_blocks[i].d);
        }   
        close(fd);
	
}
void fileOpen(char* filename,unsigned int mode){

	inode* root = (inode*) malloc(sizeof(inode));
	root = &part->inode_table[part->s.first_inode];
	printf("root Dir inode : %x\n",part->s.first_inode);
	printf("root Dir size : %x\n",root->size);

	dentry* entry = (dentry*) malloc(sizeof(dentry));
	entry = &root->blocks[0];
	printf("entry[0] : %s\n",entry);

	printf("dentry length : %d\n",entry->dir_length);

	int tmp;	
	while(1){
		if (strcmp(filename, entry->name) == 0){
			pcb->desc = (Descriptor*) malloc(sizeof(Descriptor));
			pcb->desc->file_name = filename;
			pcb->desc->mode = mode;
			pcb->desc->cur_node = &part->inode_table[entry->inode];

			printf("entry inode : %x, entry file_name : %s, entry mode : %d\n,",entry->inode,pcb->desc->file_name,pcb->desc->mode);
			break;
		}
		else{
			tmp = entry;
			tmp = tmp + entry->dir_length;
			entry = tmp;
			printf("entry : %s\n",entry);
			printf("entry file name : %s\n",entry->name);
		}

	}
}

int main(){

	fileMount();
	fileOpen("file_2",32151679);

	return;

}
