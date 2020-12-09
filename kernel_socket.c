#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_streams.h"

static file_ops socket_file_ops = {
  .Open = NULL,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};

//initialization's function

void init_portmap(){
	for (int i=0; i<MAX_PORT; i++)
		PORT_MAP[i]=NULL;
}
 listener_socket* init_listener_socket(){
 	listener_socket* ls=xmalloc(sizeof(listener_socket));
 	rlnode_init(& ls->queue, NULL);
 	ls->req_available=COND_INIT;
 	return ls;
 }

 void initializeSocketcb(socket_cb* socketcb, FCB* fcb, port_t port){ //initialize a socket as a UNBOUND socket
	socketcb->refcount=0;
	socketcb->fcb=fcb;
	socketcb->port=port;
	socketcb->type=SOCKET_UNBOUND;
	socketcb->unbound_s=xmalloc(sizeof(unbound_socket));

	rlnode_init(&socketcb->unbound_s->unbound_socket, socketcb); //initialize the rlnode as a NODE
}
//initialization end



Fid_t sys_Socket(port_t port)
{	
	if (port<0||port>MAX_PORT-1)
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





int sys_Listen(Fid_t sock){

	FCB* fcb=get_fcb(sock);
	socket_cb* socketcb=fcb->streamobj;

	if (socketcb->port<0||socketcb->port>MAX_PORT-1) //this check is not needed just to be safe
		return -1;

	if(socketcb->port==NOPORT)
		return -1;

	if(PORT_MAP[socketcb->port]!=NULL) //if this socket is already a listener
		return -1;

	if(socketcb->type!=SOCKET_UNBOUND)
		return -1;

	//end of ckecks
	PORT_MAP[socketcb->port]=socketcb; // addint the socketcb to the port map
	socketcb->type=SOCKET_LISTENER;
	socketcb->listener_s=init_listener_socket();

	return 0;

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




