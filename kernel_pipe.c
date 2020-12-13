#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_socket.h"
#include "kernel_cc.h"
 /** method tha initializes a pipe control block */
void initializePipecb(pipe_cb* pipecb){
	pipecb->has_data=COND_INIT;
	pipecb->has_space=COND_INIT;
	pipecb->w_position=0;
	pipecb->r_position=0;
	pipecb->written_bytes=0;
}

/** method that checks if pipe's buffer is full(using the counter) */ 
int isFull(pipe_cb* pipecb){
	if (pipecb->written_bytes==PIPE_BUFFER_SIZE)
		return 1;
	return 0;
}

/** method that checks if pipe's buffer is empty(using the counter) */ 
int isEmpty(pipe_cb* pipecb){
	if(pipecb->written_bytes==0)
		return 1;
	return 0;

}
/**basic method of cyclic buffer */
int goRight(int pointer){
	return (pointer+1)%PIPE_BUFFER_SIZE;
}


/** method that writes to a pipe*/
int pipe_write(void* pipecb_t, const char* buf, unsigned int n){
	if (pipecb_t==NULL||buf==NULL)
		return -1;
	
	pipe_cb* pipecb=pipecb_t;
	int w=pipecb->w_position;
	
	int count=0;
	if(pipecb->writer==NULL){//if writer is closed, obviously you cannot write
		return -1;
	} 
	if(pipecb->reader==NULL){ //if reader is closed, none will read the thingw writer will write
		return -1;
	}
	while(isFull(pipecb)==1  && pipecb->reader!=NULL) //if is emty and there is a reader, wait
		kernel_wait(&pipecb->has_space, SCHED_PIPE);
	
	if(pipecb->reader==NULL)
		return -1;
	

	while(count<n){ //while you haven't write it all

		if(isFull(pipecb)==1&&pipecb->reader==NULL){ //if is full(you didn't write it all) and the reader is null return
			pipecb->w_position=w;
			return count;
		}

		while(isFull(pipecb)==1){ //if is full(you didn't write it all) and the reader exists then wait for somebody to wake up you
			pipecb->w_position=w;
			kernel_broadcast(&pipecb->has_data); //wake up everyone who wants to read 
			kernel_wait(&pipecb->has_space, SCHED_PIPE); //sleep 
			//den kanw return giati allios otan bgei apo thn wait aplvw ua kanei return
			
		}
		
		pipecb->BUFFER[w]=buf[count]; //write from buf to pipe
		w=goRight(w);
		count++;
		pipecb->written_bytes++;//you write in byte
	}
	pipecb->w_position=w;
	kernel_broadcast(&pipecb->has_data); //wake up everyone who wants to read
	return count;
}



int pipe_read(void* pipecb_t, char* buf, unsigned int n){
	
	if (pipecb_t==NULL||buf==NULL){
		return -1;
	}
	pipe_cb* pipecb=pipecb_t;
	int r=pipecb->r_position;
	int count=0;

	if (pipecb->reader==NULL) //if reader is closed, obviously you cannot read
		return -1;
	


	if (isEmpty(pipecb)==1 && pipecb->writer==NULL) //if is empty and now one will write return 
		return 0;
	
	
	while(isEmpty(pipecb)==1 && pipecb->writer!=NULL )//if there is no data(empty and there is writer) wait for somebody to write
		kernel_wait(&pipecb->has_data, SCHED_PIPE);
		
	
	

	while(count<n){ //while you have bytes to read

		if(isEmpty(pipecb)==1&&pipecb->writer==NULL){ //if is empty and noone will write, return 
			pipecb->r_position=r;
			return count;
		}

		while(isEmpty(pipecb)==1){//if is empty but there is a chance somedy to write, wake up him
			pipecb->r_position=r;
			kernel_broadcast(&pipecb->has_space);
			kernel_wait(&pipecb->has_data, SCHED_PIPE);
		}
		

		buf[count]=pipecb->BUFFER[r]; //read
		r=goRight(r);
		count++;
		pipecb->written_bytes--;//you read a byte
	}
	
	pipecb->r_position=r;
	kernel_broadcast(&pipecb->has_space);
	

	return count;
}

/**close a writer pipe */
int pipe_writer_close(void* pipecbt){
	if (pipecbt==NULL)
		return -1;
	pipe_cb* pipecb=pipecbt;
	if(!pipecb)
		return -1;
	pipecb->writer=NULL; //close the write end
	kernel_broadcast(&pipecb->has_data);
	if(!pipecb->reader&&!pipecb)//if reader is null and pipecb still exists, free the pipe
		free(pipecb);
	
	return 0;
}


/**method that closes a read pipe */
int pipe_reader_close(void* pipecbt){

	if (pipecbt==NULL) //if there is no args return error
		return -1;
	
	pipe_cb* pipecb=pipecbt;
	if(!pipecb) //if there is no args return error
		return -1;
	pipecb->reader=NULL; //close read end
	kernel_broadcast(&pipecb->has_space); 

	if(!pipecb->writer&&!pipecb) //if there is no writer and the pipe still exists remove it
		free(pipecb);
	return 0;

}

void* open_pipe(){
	return NULL;
}


int return_error(){
	return -1;
}


static file_ops reader_file_ops = {
  .Open = open_pipe,
  .Read = pipe_read,
  .Write = return_error,
  .Close = pipe_reader_close
};

static file_ops writer_file_ops = {
  .Open =open_pipe,
  .Read = return_error,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

/** syscal that creates a pipe, with one wrier AND one reader*/

int sys_Pipe(pipe_t* pipe){

	
	Fid_t fid[2];
	FCB* fcb[2];

	/**allocating 2 fcbs */ 
	if(FCB_reserve(2, fid, fcb)==0 ){
		printf("Failed to allocate console Fids\n");
		return -1;
	}

	pipe_cb* pipecb=xmalloc(sizeof(pipe_cb)); //allocate mem for one pcb
	initializePipecb(pipecb);//initialize the pcb


	//from struct pipe, matching read with 0, and write with 0
	pipe->read=fid[0];
	pipe->write=fid[1];

	//to pipecb block, matching the reader/writer with fcbs
	pipecb->reader=fcb[0];
	pipecb->writer=fcb[1];

	//fcb points to a pipecb
	fcb[0]->streamobj = pipecb;
	fcb[1]->streamobj = pipecb;

	fcb[0]->streamfunc = &reader_file_ops;
	fcb[1]->streamfunc = &writer_file_ops;

	return 0;

}

