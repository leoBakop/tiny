#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

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

peer_socket* init_pipe_socket(socket_cb* peer){
	peer_socket* ps= xmalloc(sizeof(peer_socket));
	ps->peer=peer;
	return ps;
}


void initRequest(connection_request* request, socket_cb* peer){
	request->admitted=0;
	request->connected_cv=COND_INIT;
	request->peer=peer;
	rlnode_init(&request->queue_node, request);
}

//initialization end

//assistant functions

socket_cb* get_scb(Fid_t socket){
	if(socket<0){
		return NULL;
	}

	FCB* fcb= get_fcb(socket);
	if(fcb==NULL){
		return NULL;
	}
	socket_cb* retval=fcb->streamobj;
	return retval;
}



socket_cb* checkIfListener(Fid_t s){
	if(s<0||s>MAX_PORT-1){
		return NULL;
	}

	FCB* fcb=get_fcb(s);
	socket_cb* socketcb=fcb->streamobj;
	if(socketcb->type==SOCKET_LISTENER){
		return socketcb;
	}
	return NULL;
}

void socketIncRefCount (socket_cb* scb){
	scb->refcount++;
}

void socketDecRefCount(socket_cb* scb){
	scb->refcount--;
	if(scb->refcount==0){
		free(scb);
	}
}

//end of assistant functions




Fid_t sys_Socket(port_t port){	

	if (port<0||port>MAX_PORT){
		return NOFILE;
	}

	Fid_t fid;
	FCB* fcb;


	if(FCB_reserve(1, &fid, &fcb)==0 ){
		printf("Failed to allocate console Fids\n");
		return NOFILE;
	}

	socket_cb* socketcb=xmalloc(sizeof(socket_cb));
	initializeSocketcb(socketcb, fcb, port);

	//initialize the fcb
	fcb->streamfunc= &socket_file_ops;
	fcb->streamobj=socketcb;

	return fid;

}





int sys_Listen(Fid_t sock){

	if(sock<0 || sock > MAX_PROC){
		return -1;
	}

	FCB* fcb=get_fcb(sock);


	if(fcb==NULL)
		return -1;


	socket_cb* socketcb=fcb->streamobj;

	if(socketcb==NULL)
		return -1;

	//fprintf(stderr, "socketcb->port is %d\n",socketcb->port );
	if (socketcb->port<0||socketcb->port>MAX_PORT-1) //this check is not needed just to be safe
		return -1;

	if(socketcb->port==NOPORT)
		return -1;

	if(PORT_MAP[socketcb->port]!=NULL) //if this socket is already a listener
		return -1; 

	if(socketcb->type!=SOCKET_UNBOUND) //if it is peer or listener
		return -1;

	//end of ckecks
	PORT_MAP[socketcb->port]=socketcb; // addint the socketcb to the port map
	socketcb->type=SOCKET_LISTENER;
	socketcb->listener_s=init_listener_socket();

	return 0;

}


Fid_t sys_Accept(Fid_t lsock){

	socket_cb* scb=checkIfListener(lsock);
	rlnode* request;

	if(scb==NULL){
		return NOFILE;
	}

	socketIncRefCount(scb);

	while(is_rlist_empty(&scb->listener_s->queue)==1&&PORT_MAP[scb->port] != NULL){
		kernel_wait(&scb->listener_s->req_available, SCHED_PIPE);
	}


	if (PORT_MAP[scb->port] == NULL){
		return NOFILE;
	}

	if(is_rlist_empty(&scb->listener_s->queue)!=1){
		request=rlist_pop_front(&scb->listener_s->queue);
	}

	connection_request* con =request->req;

	if(con->peer==NULL){
		return -1;
	}

	con->admitted=1;

	//upgrade the socket which made the connection request
	socket_cb* client=con->peer;
	client->type=SOCKET_PEER;
	


	//creation of the new socket(listener's peer)
	Fid_t serverFidt= sys_Socket(0);
	if (serverFidt==-1)
		return -1;

	FCB* serverFCB= get_fcb(serverFidt);
	socket_cb* server= serverFCB->streamobj;
	server->type=SOCKET_PEER;
	server->peer_s=init_pipe_socket(client);

	//upgrade the socket which made the connection request
	client->peer_s=init_pipe_socket(server);

	//create the two pipecbs that are going to fully connect the two sockets
	pipe_cb* pipecb1=xmalloc(sizeof(pipe_cb));
	pipe_cb* pipecb2=xmalloc(sizeof(pipe_cb));

	//initialization of two pipecbs

	initializePipecb(pipecb1);
	initializePipecb(pipecb2);
	//pipecb1 has for writer the server, and for reader the client
	pipecb1->writer=server->fcb;
	pipecb1->reader=client->fcb;

	//pipecb2 has for writer the client, and for reader the server
	pipecb2->writer=client->fcb;
	pipecb2->reader=server->fcb;

	kernel_signal(&con->connected_cv);

	socketDecRefCount(scb);

	return serverFidt;


}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout){

	if(sock<0|| sock >MAX_PORT-1){
		return -1;
	}

	if(port<=0|| port >MAX_PORT-1){
		return -1;
	}
	if(PORT_MAP[port]==NULL){
		return -1;
	}

	socket_cb* listener=PORT_MAP[port];
	socketIncRefCount(listener);

	FCB* fcb= get_fcb(sock);

	if(fcb==NULL){
		return -1;
	}

	socket_cb* client=fcb->streamobj;

	connection_request* request=xmalloc(sizeof(connection_request));
	initRequest(request, client);

	rlist_push_front(&listener->listener_s->queue, &request->queue_node);
	kernel_broadcast(&listener->listener_s->req_available);

	if(request->admitted==0){ 										    //if i write while(request->admitted==0){} i will create a infinite loop in case 
		kernel_timedwait(&request->connected_cv, SCHED_PIPE, timeout); //of timeout because in this situation request->admitted will be 0 so i wont escape from the loop
	}
	if(request->admitted==0){ // the timeout expired
		socketDecRefCount(listener); //new code
		return -1;
	}

	socketDecRefCount(listener);

	return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how){
	if(sock<0||sock>MAX_PORT){
		return -1;
	}

	FCB* fcb =get_fcb(sock);
	if (fcb==NULL){
		return -1;
	}
	socket_cb* socket=fcb->streamobj;

	switch(how){
		case 1:
			pipe_reader_close(socket->peer_s->read_pipe);
			socket->peer_s->read_pipe=NULL;
			break;
		case 2:
			pipe_writer_close(socket->peer_s->read_pipe);
			socket->peer_s->write_pipe=NULL;
			break;
		case 3:
			pipe_reader_close(socket->peer_s->read_pipe);
			pipe_writer_close(socket->peer_s->read_pipe);
			socket->peer_s->read_pipe=NULL;
			socket->peer_s->write_pipe=NULL;
		break;
		default:
			return -1;
	}
	return 0;
}

int socket_write(void* socketcb, const char* buf, unsigned int n){

	socket_cb* socket=(socket_cb*) socketcb;
	int retval=0;

	if(socket==NULL){
		return -1;
	}

	if(socket->type!=SOCKET_PEER){
		return -1;
	}

	pipe_cb* toWrite= socket->peer_s->write_pipe;
	if(toWrite==NULL){
		retval=pipe_write(toWrite, buf, n);
	}
	return retval;
}

int socket_read(void* socketcb, char* buf, unsigned int n){

	socket_cb* socket=(socket_cb*) socketcb;
	int retval=0;

	if(socket==NULL){
		return -1;
	}

	if(socket->type!=SOCKET_PEER){
		return -1;
	}

	pipe_cb* toRead= socket->peer_s->read_pipe;
	if(toRead==NULL){
		retval=pipe_read(toRead, buf, n);
	}
	return retval;
}

int socket_close(void* socketcb){
	socket_cb* socket=(socket_cb*) socketcb;

	if(socket==NULL){
		return -1;
	}

	switch(socket->type){

		case SOCKET_UNBOUND:
			break;
		case SOCKET_PEER:
			socket->peer_s->peer=NULL;
			pipe_writer_close(socket->peer_s->write_pipe);
			pipe_reader_close(socket->peer_s->read_pipe);
			free(socket->peer_s);
			socket->type=SOCKET_UNBOUND;
			break;
		case SOCKET_LISTENER:
			while(is_rlist_empty(&socket->listener_s->queue)==0){ //ampting listener's queue 
				rlist_pop_front(&socket->listener_s->queue);
			}
			free(socket->listener_s);
			PORT_MAP[socket->port]=NULL;
			socket->type=SOCKET_UNBOUND;
			socket->port=0;
			break;
		default:
			return -1;
	}

		socketDecRefCount(socket);

	return 0;
}




