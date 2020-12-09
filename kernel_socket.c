#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_streams.h"

static file_ops socket_file_ops = {
  .Open = NULL,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};





void init_portmap(){
	for (int i=0; i<MAX_PORT; i++)
		PORT_MAP[i]=NULL;
}



Fid_t sys_Socket(port_t port)
{	
	if (port<0)
		return -1;

	Fid_t fid;
	FCB* fcb;


	if(FCB_reserve(1, &fid, &fcb)==0 ){
		printf("Failed to allocate console Fids\n");
		return -1;
	}

	socket_cb* socketcb=xmalloc(sizeof(socket_cb));
	initializeSocketcb(socketcb, fcb, port);

	//initialize the fcb
	fcb->streamfunc= &socket_file_ops;
	fcb->streamobj=socketcb;

	return fid;

}


void initializeSocketcb(socket_cb* socketcb, FCB* fcb, port_t port){ //initialize a socket as a UNBOUND socket
	socketcb->refcount=0;
	socketcb->fcb=fcb;
	socketcb->port=port;
	socketcb->type=SOCKET_UNBOUND;
	socketcb->unbound_s=xmalloc(sizeof(unbound_socket));

	rlnode_init(&socketcb->unbound_s->unbound_socket, socketcb); //initialize the rlnode as a NODE
}





int sys_Listen(Fid_t sock)
{
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

int socket_write(){
	return 0;
}

int socket_read(){
	return 0;
}

int socket_close(){
	return 0;
}




