
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
// new code by Alexandra. 
/*
In this new function , named new_start_main_thread() 
 we are going to slightly change start_main_thread() 
 in order to call this as an argument to the new spawn function, called in CreateThread
 as asked by the project
*/

void start_another_thread()
{
int exitval;

Task call = CURTHREAD->ptcb->task;
int argl = CURTHREAD->argl;
void* args = CURTHREAD->args;

exitval = call(argl,args);

sys_ThreadExit(exitval);
}

//end of new code



/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
 PCB* pcb = CURPROC ;
 CURPROC->thread_count ++;
 PTCB *ptcb = xmalloc(sizeof(PTCB));

initialize_PTCB(ptcb,CURPROC); //check if curproc is correct

if( args == NULL){
	ptcb-> args = NULL;
}
else
{
	ptcb-> args = malloc(argl);
	memcpy(ptcb->args , args, argl); 
}

rlist_push_back(& CURPROC->ptcb_list, ptcb); 
//anti gia ptcb mipos prepei na kano initialize enan neo pointer se komvo //rlnode 
//*newNode =rlnode_init(&ptcb->node , ptcb);

if(task != NULL)
{
	TCB* tcb = spawn_thread(CURPROC, start_another_thread);
	wakeup(tcb); //etoimase ena tcb gia ton scheduler
}

return (Tid_t) ptcb;	
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread();
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

