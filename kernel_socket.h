#include "tinyos.h"
#include "bios.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_dev.h"



typedef enum socket_type_enum{
	SOCKET_LISTENER,
	SOCKET_UNBOUND,
	SOCKET_PEER,
}socket_type;

typedef struct socket_control_block socket_cb;


typedef struct listener_socket_cb{
	rlnode queue;
	CondVar req_available;
}listener_socket;

typedef struct unbound_socket_cb{
	rlnode unbound_socket;
}unbound_socket;


typedef struct peer_socket_cb{
	socket_cb* peer;
	pipe_cb* write_pipe;
	pipe_cb* read_pipe;
}peer_socket;



typedef struct socket_control_block{
	uint refcount;
	FCB* fcb;
	socket_type type;
	port_t port;

	union{
		listener_socket* listener_s;
		unbound_socket* unbound_s;
		peer_socket* peer_s;
	};

} socket_cb;


socket_cb* PORT_MAP[MAX_PORT]; //MAX PORT has already defined in tinyos.h
// NOPORT already defined in tinyos.h




typedef struct connection_request_struct{
	int admitted;
	socket_cb* peer;

	CondVar connected_cv;
	rlnode queue_node;
}connection_request;



//syscals

Fid_t sys_Socket(port_t port);
int sys_Listen(Fid_t sock);
Fid_t sys_Accept(Fid_t lsock);
int sys_Connect(Fid_t sock, port_t port, timeout_t timeout);
int sys_ShutDown(Fid_t sock, shutdown_mode how);


//assitant methods
void initializeSocketcb(socket_cb* socketcb, FCB* fcb, port_t port);
int socket_read();
int socket_write();
int socket_close();
void init_portmap();

//implmented in kernel_pipe.c
void initializePipecb(pipe_cb* pipecb);
int pipe_writer_close(void* pipecbt);
int pipe_reader_close(void* pipecbt);
int pipe_read(void* pipecb_t, char* buf, unsigned int n);
int pipe_write(void* pipecb_t, const char* buf, unsigned int n);



