
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
 pcb->thread_count ++;
 PTCB *ptcb = xmalloc(sizeof(PTCB));

initialize_PTCB(ptcb,pcb); //check if curproc is correct

if( args == NULL){
	ptcb-> args = NULL;
}
else
{
	ptcb-> args = malloc(argl);
	memcpy(ptcb->args , args, argl); 
}

rlist_push_back(& pcb->ptcb_list, ptcb); 
//anti gia ptcb mipos prepei na kano initialize enan neo pointer se komvo //rlnode 
//*newNode =rlnode_init(&ptcb->node , ptcb);

if(task != NULL)
{
	TCB* tcb = spawn_thread(ptcb, start_another_thread);
	wakeup(tcb); //etoimase ena tcb gia ton scheduler
}

return (Tid_t) ptcb;	
}

//new code by bill
Tid_t sys_ThreadSelf()
{
	return (PTCB* ) CURTHREAD->ptcb_owner;
}

/**
  @brief Join the given thread.
  */

int sys_ThreadJoin(Tid_t tid, int* exitval){

  PTCB* ptcb= (PTCB*) tid;
  PTCB* currptcb= CURTHREAD->ptcb_owner;
  PCB* curproc= CURPROC;

if(ptcb->detached == 1 ){
  return -1;
}

if(currptcb equals ptcb ){

  return -1;
}

if(rlist_find(& pcb->ptcb_list, ptcb, -1)==-1){
  return -1;
}

kernel_wait(ptcb->exit_cv, SCHED_USER);
refcountIncr(ptcb);
exitval= & ptcb->exitval;
return 0;
}

//new code by bill


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

//new code by lui
void sys_ThreadExit(int exitval)
{   
    PCB* curproc=CURPROC;

    if (curproc->thread_count==1){ //if this thread is the last one
        /* Do all the other cleanup we want here, close files etc. */
        if(curproc->args) {
          free(curproc->args);
          curproc->args = NULL;
        }

        /* Clean up FIDT */
        for(int i=0;i<MAX_FILEID;i++) {
          if(curproc->FIDT[i] != NULL) {
            FCB_decref(curproc->FIDT[i]);
            curproc->FIDT[i] = NULL;
          }
        }

        /* Reparent any children of the exiting process to the 
           initial task */
        PCB* initpcb = get_pcb(1);
        while(!is_rlist_empty(& curproc->children_list)) {
          rlnode* child = rlist_pop_front(& curproc->children_list);
          child->pcb->parent = initpcb;
          rlist_push_front(& initpcb->children_list, child);
        }

        /* Add exited children to the initial task's exited list 
           and signal the initial task */
        if(!is_rlist_empty(& curproc->exited_list)) {
          rlist_append(& initpcb->exited_list, &curproc->exited_list);
          kernel_broadcast(& initpcb->child_exit);
        }

        /* Put me into my parent's exited list */
        if(curproc->parent != NULL) {   /* Maybe this is init */
          rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
          kernel_broadcast(& curproc->parent->child_exit);
        }

        /* Disconnect my main_thread */
        curproc->main_thread = NULL;

        /* Now, mark the process as exited. */
        curproc->pstate = ZOMBIE;
        curproc->exitval = exitval;
    }else{

      PTCB* ptcb=CURTHREAD->owner_ptcb;
      ptcb->exitval=exitval  //save the thread exitval to the ptcb exitval
      if(ptcb->refcount > 0){  //if there are some THREAD who haved join ptcb
          kernel_broadcast(CURTHREAD->ptcb->exit_cv); //inform the other threads 
          refcountDec(ptcb);   
      }else{
          //if refcount==0 then destroy the ptcb and remove it from the pcb's list
          rlist_remove(& curproc->ptcb_list, ptcb);
          free(ptcb)  

          }
   }
   kerenel_sleep(EXITED, SCHED_USER); //koimisoy ton ypno toy dikaioy

}

//end code of lui
