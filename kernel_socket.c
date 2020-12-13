#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

static file_ops socket_file_ops = {  //streamfunc of socket_cb
  .Open = open_pipe,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};

//initialization's function

void init_portmap(){  //function that initialiazes the port map (every pointer to null)
	for (int i=0; i<MAX_PORT; i++)
		PORT_MAP[i]=NULL;
}

/**initialize a listener socket */
listener_socket* init_listener_socket(){  
 	listener_socket* ls=xmalloc(sizeof(listener_socket));
 	rlnode_init(& ls->queue, NULL);
 	ls->req_available=COND_INIT;
 	return ls;
}

/** initialize an UNBOUND socket */
 void initializeSocketcb(socket_cb* socketcb, FCB* fcb, port_t port){ 
	socketcb->refcount=1; // because if i refcount is initialized in 0 when i do refIncr and refDec it would be closed(and we don't want it)
	socketcb->fcb=fcb;
	socketcb->port=port;
	socketcb->type=SOCKET_UNBOUND;
	socketcb->unbound_s=xmalloc(sizeof(unbound_socket));

	rlnode_init(&socketcb->unbound_s->unbound_socket, socketcb); //initialize the rlnode as a NODE
}

/** initialize a peer socket */
peer_socket* init_peer_socket(socket_cb* peer){
	peer_socket* ps= xmalloc(sizeof(peer_socket));
	ps->peer=peer;
	return ps;
}

/** initialize a request */
void initRequest(connection_request* request, socket_cb* peer){  //initializes a connection request struct
	request->admitted=0;
	request->connected_cv=COND_INIT;
	request->peer=peer;
	rlnode_init(&request->queue_node, request);
}

//initialization end

//assistant functions

socket_cb* get_scb(Fid_t socket){
	if(socket<0)
		return NULL;
	
	FCB* fcb= get_fcb(socket);
	if(fcb==NULL)
		return NULL;
	
	socket_cb* retval=fcb->streamobj;
	return retval;
}


/** method that checks if a sth is sock and also if listener */
socket_cb* checkIfListener(Fid_t s){
	
	socket_cb* socketcb=get_scb(s);

	if(socketcb==NULL)
		return NULL;
	
	if(socketcb->type==SOCKET_LISTENER)
		return socketcb;
	
	return NULL;
}

void socketIncRefCount (socket_cb* scb){
	scb->refcount++;
}

void socketDecRefCount(socket_cb* scb){
	scb->refcount--;
	if(scb->refcount==0)
		free(scb);
	
}

//end of assistant functions



/** syscalls that creates AN UNBOUND socket socket**/
Fid_t sys_Socket(port_t port){	

	/**checks for illegal port */
	if (port<0||port>MAX_PORT)//for and unbound socket port =0 is LEGAL (in case of listener it would be checked) 
		return NOFILE;
	

	Fid_t fid;
	FCB* fcb;


	if(FCB_reserve(1, &fid, &fcb)==0 ){
		printf("Failed to allocate console Fids\n");
		return NOFILE;
	}

	socket_cb* socketcb=xmalloc(sizeof(socket_cb));
	initializeSocketcb(socketcb, fcb, port);

	//initialize the fcb
	fcb->streamfunc= &socket_file_ops; //connects fcb with socketcb
	fcb->streamobj=socketcb; //connect gcb with the functions of the socket_cb

	return fid;

}





int sys_Listen(Fid_t sock){

	socket_cb* socketcb=get_scb(sock);
	if(socketcb==NULL) 
		return -1;

	if (socketcb->port<0||socketcb->port>MAX_PORT-1) //this check is not needed just to be safe
		return -1;

	if(socketcb->port==NOPORT)//you cannot make a listener with port =0
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

/**method that the listener after all accepts a request from an unbound socket*/
Fid_t sys_Accept(Fid_t lsock){

	socket_cb* scb=checkIfListener(lsock); //first time that i realize that socket_cb could be writted as scb
	rlnode* request;

	if(scb==NULL)
		return NOFILE;
	


	socketIncRefCount(scb);

	while(is_rlist_empty(&scb->listener_s->queue)==1&&PORT_MAP[scb->port] != NULL) //if there is no request, sleep
		kernel_wait(&scb->listener_s->req_available, SCHED_PIPE);
	


	if (PORT_MAP[scb->port] == NULL)//check if scb is still a legal listener 
		return NOFILE;
	

	if(is_rlist_empty(&scb->listener_s->queue)!=1){// if somedy made you a request (and he woke up you) find the request
		request=rlist_pop_front(&scb->listener_s->queue);
	}else{
		return NOFILE;
	}

	connection_request* con =request->req;

	if(con->peer==NULL)//check if the one who made the request, still exists
		return -1;
	

	


	//creation of the new socket(unbound socket)
	Fid_t serverFidt= sys_Socket(0);
	if (serverFidt==NOFILE){
		return -1;
	}

	con->admitted=1; //if you can create a socket then "inform" the request that it is served


	//upgrade the socket which made the connection request
	socket_cb* client=con->peer;

	if(client->type!=SOCKET_UNBOUND){
		return NOFILE;
	}

	



	client->type=SOCKET_PEER; //mark the new socket as a peer socket

	/**initialize the peer socket*/
	FCB* serverFCB= get_fcb(serverFidt);
	socket_cb* server= serverFCB->streamobj;
	server->type=SOCKET_PEER;
	server->peer_s=init_peer_socket(client);

	//upgrade the socket which made the connection request
	client->peer_s=init_peer_socket(server);

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

	//client has for reader the pipecb1, and for writer pipecb2
	client->peer_s->write_pipe=pipecb2;
	client->peer_s->read_pipe=pipecb1;


	//server has for reader pipecb2, and for writer pipecb1
	server->peer_s->write_pipe=pipecb1;
	server->peer_s->read_pipe=pipecb2;

	kernel_signal(&con->connected_cv); //wake up the "client" who made the request to you
	socketDecRefCount(scb);

	return serverFidt;


}


/** syscalls that makes a request 
* sock= from which client
* port=to which port/listener
*/
int sys_Connect(Fid_t sock, port_t port, timeout_t timeout){

	if(sock<0|| sock >MAX_PORT-1)
		return -1;
	
	if(port<=0|| port >MAX_PORT-1) //check if port is legal
		return -1;
	
	if(PORT_MAP[port]==NULL) //if there is no listener in this port, error
		return -1;
	

	socket_cb* listener=PORT_MAP[port]; //get the listener you are going to make the cannection request
	socketIncRefCount(listener);

	socket_cb* client=get_scb(sock);  //get the client
	if(client==NULL||client->type!=SOCKET_UNBOUND)
		return -1;


	connection_request* request=xmalloc(sizeof(connection_request));
	initRequest(request, client);   //create the request

	rlist_push_front(&listener->listener_s->queue, &request->queue_node); //send the request
	kernel_broadcast(&listener->listener_s->req_available); //and then wake up the receiver

/**after all go to sleep (in the request struct, in case that the listener want to wake up you ), for timeout */
	if(request->admitted==0)			 //if i write while(request->admitted==0){} i will create a infinite loop in case 
		kernel_timedwait(&request->connected_cv, SCHED_PIPE, timeout); //of timeout because in this situation request->admitted will be 0 so i wont escape from the loop
	
	if(request->admitted==0){// the timeout expired
		socketDecRefCount(listener); 
		return -1;
	}

	socketDecRefCount(listener);

	return 0;
}



/** method that shuts down a peer pipe (delete the connections) */ 
int sys_ShutDown(Fid_t sock, shutdown_mode how){
	if(sock<0 || sock>=MAX_FILEID)
	return -1;

	//if fid invalid.
	socket_cb* socket=get_scb(sock);
	if (socket==NULL)
		return-1;
 
	if (socket->type != SOCKET_PEER)
		return -1;

	 
	switch(how){
		case SHUTDOWN_READ: //delete connections between socket and read pipe
			pipe_reader_close(socket->peer_s->read_pipe);
			break; 
		case SHUTDOWN_WRITE: //delete connections between socket and write pipe
			pipe_writer_close(socket->peer_s->write_pipe);
			break;
		case SHUTDOWN_BOTH://delete connections between socket and both pipes
			pipe_writer_close(socket->peer_s->write_pipe);
			pipe_reader_close(socket->peer_s->read_pipe);
			break;
		default:
			break;
		}

	return 0;
	}

	/**method and not syscalls */
/**method that a socket write into the write pipe */
int socket_write(void* socketcb, const char* buf, unsigned int n){

	socket_cb* socket=(socket_cb*) socketcb;
	int retval=0;

	if(socket==NULL)
		return -1;
	

	if(socket->type!=SOCKET_PEER) //if socket is not peer, return with error
		return -1;
	
	pipe_cb* toWrite= socket->peer_s->write_pipe;	
		retval=pipe_write(toWrite, buf, n);
	
	return retval;
}


/** method that reads from the read pipe */
int socket_read(void* socketcb, char* buf, unsigned int n){

	socket_cb* socket=(socket_cb*) socketcb;
	int retval=0;

	if(socket==NULL)
		return -1;
	

	if(socket->type!=SOCKET_PEER)//if socket is not peer, return with error
		return -1;
	


	pipe_cb* toRead= socket->peer_s->read_pipe;
	retval=pipe_read(toRead, buf, n);
	
	return retval;
}



/** method that closes a socket*/
int socket_close(void* socketcb){
	socket_cb* socket=(socket_cb*) socketcb;
	peer_socket* peer;

	if(socket==NULL){
		return -1;
	}

	switch(socket->type){

		case SOCKET_UNBOUND:
			break;
		case SOCKET_PEER:
			peer=socket->peer_s;
			if(peer->write_pipe!=NULL){
				pipe_writer_close(peer->write_pipe);
			}
			if(peer->read_pipe!=NULL){
				pipe_reader_close(peer->read_pipe);
			}
			
			break;

		case SOCKET_LISTENER:
			PORT_MAP[socket->port]=NULL;
			if(socket->refcount!=0){
				kernel_broadcast(& socket->listener_s->req_available);
				while(is_rlist_empty(& socket->listener_s->queue)==0){
					rlist_pop_front(&socket->listener_s->queue);
				}
			}
			break;
		default:
			return -1;
	}


	return 0;
	
}

