#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

void initializePipecb(pipe_cb* pipecb){
	pipecb->has_data=COND_INIT;
	pipecb->has_space=COND_INIT;
	pipecb->w_position=0;
	pipecb->r_position=0;
	pipecb->written_bytes=0;
}
int isFull(pipe_cb* pipecb){
	if (pipecb->written_bytes==PIPE_BUFFER_SIZE)
		return 1;
	return 0;
}

int isEmpty(pipe_cb* pipecb){
	if(pipecb->written_bytes==0)
		return 1;
	return 0;

}

int goRight(int pointer){
	return (pointer+1)%PIPE_BUFFER_SIZE;
}

int pipe_write(void* pipecb_t, const char* buf, unsigned int n){
	if (pipecb_t==NULL||buf==NULL)
		return -1;
	pipe_cb* pipecb=pipecb_t;
	int w=pipecb->w_position;
	
	int count=0;
	if(pipecb->writer==NULL) //if writer is closed, obviously you cannot write
		return -1;
	if(pipecb->reader==NULL) //if reader is closed, none will read the thingw writer will write
		return -1;
	while(isFull(pipecb)==1  && pipecb->reader!=NULL){
		//kernel_broadcast(&pipecb->has_data);
		kernel_wait(&pipecb->has_space, SCHED_PIPE);
	}
	if(pipecb->reader==NULL)
		return -1;

	while(count<n){

		if(isFull(pipecb)==1&&pipecb->reader==NULL){
			pipecb->w_position=w;
			return count;
		}

		while(isFull(pipecb)==1){
			pipecb->w_position=w;
			kernel_broadcast(&pipecb->has_data);
			kernel_wait(&pipecb->has_space, SCHED_PIPE);
			//den kanw return giati allios otan bgei apo thn wait aplvw ua kanei return
			
		}
		
		pipecb->BUFFER[w]=buf[count];
		w=goRight(w);
		count++;
		pipecb->written_bytes++;
	}
	pipecb->w_position=w;
	kernel_broadcast(&pipecb->has_data);
	return count;
}



int pipe_read(void* pipecb_t, char* buf, unsigned int n){
	if (pipecb_t==NULL||buf==NULL)
		return -1;
	pipe_cb* pipecb=pipecb_t;
	int r=pipecb->r_position;
	
	int count=0;

	if (pipecb->reader==NULL) //if reader is closed, obviously you cannot read
		return -1;
	if (isEmpty(pipecb)==1 && pipecb->writer==NULL)
		return 0;
	
	while(isEmpty(pipecb)==1 && pipecb->writer!=NULL ){
		//kernel_broadcast(&pipecb->has_space);
		kernel_wait(&pipecb->has_data, SCHED_PIPE);
		
	}
	if(pipecb->writer==NULL)
		return -1;


	while(count<n){
		if(isEmpty(pipecb)==1&&pipecb->writer==NULL){
			pipecb->r_position=r;
			return count;
		}
		while(isEmpty(pipecb)==1){
			pipecb->r_position=r;
			kernel_broadcast(&pipecb->has_space);
			kernel_wait(&pipecb->has_data, SCHED_PIPE);

		}
		

		buf[count]=pipecb->BUFFER[r];
		r=goRight(r);
		count++;
		pipecb->written_bytes--;
	}

	pipecb->r_position=r;
	kernel_broadcast(&pipecb->has_space);
	return count;
}

int pipe_writer_close(void* pipecbt){
	if (pipecbt==NULL)
		return -1;
	pipe_cb* pipecb=pipecbt;
	pipecb->writer=NULL;
	kernel_broadcast(&pipecb->has_data);
	if(pipecb->reader==NULL){
		free(pipecb);
	}

	return 0;
}

int pipe_reader_close(void* pipecbt){
	if (pipecbt==NULL)
		return -1;
	pipe_cb* pipecb=pipecbt;
	pipecb->reader=NULL;
	
	if(pipecb->writer==NULL){
		free(pipecb);
	}else{
		kernel_broadcast(&pipecb->has_space);
	}

	return 0;

}


static file_ops reader_file_ops = {
  .Open = NULL,
  .Read = pipe_read,
  .Write = NULL,
  .Close = pipe_reader_close
};

static file_ops writer_file_ops = {
  .Open =NULL,
  .Read = NULL,
  .Write = pipe_write,
  .Close = pipe_writer_close
};


int sys_Pipe(pipe_t* pipe){

	pipe_cb* pipecb=xmalloc(sizeof(pipe_cb));
	Fid_t fid[2];
	FCB* fcb[2];
	initializePipecb(pipecb);

	
	if(FCB_reserve(2, fid, fcb)==0 ){
		printf("Failed to allocate console Fids\n");
		return -1;
	}
	pipe->read=fid[0];
	pipe->write=fid[1];

	pipecb->reader=fcb[0];
	pipecb->writer=fcb[1];

	fcb[0]->streamobj = pipecb;
	fcb[1]->streamobj = pipecb;

	fcb[0]->streamfunc = &reader_file_ops;
	fcb[1]->streamfunc = &writer_file_ops;

	return 0;

}

