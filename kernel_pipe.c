#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

int pipe_write(void* pipecb_t, const char *buf, unsigned int n)
{	

	FCB* fcb=pipecb_t;
	pipe_cb* pipecb= fcb->streamobj;
	int previousState=pipecb->is_empty;
	if(pipecb_t==NULL)
		return -1;
	int w=pipecb->w_position;
	int r=pipecb->r_position;
	if(w==r && pipecb->is_empty==0){ //full list
		kernel_wait(& pipecb->has_space, SCHED_PIPE);
	}
	for(int i=0; i<n; i++){
		if((w+i)%PIPE_BUFFER_SIZE==r && pipecb->is_empty==0){
			w=(w+i)%PIPE_BUFFER_SIZE;
			pipecb->w_position=w;
			if(previousState==1 && i>0){
				pipecb->is_empty=0;
				kernel_broadcast(&pipecb->has_data);
			}
			return i;
		}
		pipecb->BUFFER[(w+i)%PIPE_BUFFER_SIZE]=buf[i];
	}
	w=(w+n)%PIPE_BUFFER_SIZE;
	pipecb->w_position=w;
	if(previousState==1 && n>0){
		pipecb->is_empty=0;
		kernel_broadcast(&pipecb->has_data);
	}

	return n;

}


int pipe_read(void* pipecb_t, char *buf, unsigned int n)
{

	return 0;

}


int pipe_writer_close(void* _pipecb)
{	
	pipe_cb* pipecb=(pipe_cb*) _pipecb;
	if(pipecb==NULL)
		return-1;
	pipecb->writer=NULL;
	if(pipecb->reader==NULL){
		free(pipecb);
	}
	//kernel_broadcast(& pipecb->has_space);

	return 0;
}


int pipe_reader_close(void* _pipecb)
{
	pipe_cb* pipecb=(pipe_cb*) _pipecb;
	if(pipecb==NULL)
		return-1;
	pipecb->reader=NULL;
	if(pipecb->writer==NULL){
		free(pipecb);
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


int sys_Pipe(pipe_t* pipe)
{	
	pipe_cb* pipecb;
	Fid_t fid[2];
	FCB* fcb[2];
	pipe = xmalloc(sizeof(pipe_t));
	if(FCB_reserve(2, fid, fcb)==0)
	{
		printf("Failed to allocate reader and writer\n");
		return -1;
	}
	pipe->read=fid[0];
	pipe->write=fid[1];
	pipecb=xmalloc(sizeof(pipe_cb));
	pipecb->has_space=COND_INIT;
	pipecb->has_data=COND_INIT;
	pipecb->writer=fcb[1];
	pipecb->reader=fcb[0];
	pipecb->w_position=0; 
	pipecb->r_position=0;
	pipecb->is_empty=1;
	fcb[0]->streamfunc=&reader_file_ops;
	fcb[1]->streamfunc=&writer_file_ops;
	fcb[0]->streamobj=pipecb;
	fcb[1]->streamobj=pipecb;
	return 0;
}



