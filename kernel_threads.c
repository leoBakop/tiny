
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"
//#include "kernel_sched.c"
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

Task call = CURTHREAD->owner_ptcb->task;
//Task call = CURTHREAD->owner_ptcb->task;
//int argl = CURTHREAD->argl;
//void* args = CURTHREAD->args;
int argl=CURTHREAD->owner_ptcb->argl;
void* args= CURTHREAD->owner_ptcb->args;

exitval = call(argl,args);

ThreadExit(exitval);
}

//end of new code



/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  PCB* pcb = CURPROC;
   
  PTCB *ptcb = xmalloc(sizeof(PTCB));

  initialize_PTCB(ptcb,pcb); 

  if( args == NULL){
    ptcb-> args = NULL;
  }
  else
  {
    ptcb-> args = malloc(argl);
    memcpy(ptcb->args , args, argl); 
  }

  if(task != NULL)
  { 
    
    TCB* tcb = spawn_thread(ptcb, start_another_thread); //spawn thread also connects ptcb with tcb
    rlist_push_back(& pcb->ptcb_list, ptcb); 
    pcb->thread_count ++;
    wakeup(tcb); //etoimase ena tcb gia ton scheduler
  }

  return (Tid_t) ptcb;  
}


//new code by bill
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD->owner_ptcb;
}

/**
  @brief Join the given thread.
  */

int sys_ThreadJoin(Tid_t tid, int* exitval){

  PTCB* ptcb= (PTCB*) tid;
  PTCB* currptcb= CURTHREAD->owner_ptcb;
  PCB* curproc= CURPROC;

if(ptcb->detached == 1 ){
  return -1;
}

if(currptcb->tcb == ptcb->tcb ){
  return -1;
}

if(rlist_find(& curproc->ptcb_list, ptcb, & curproc->ptcb_list)== & curproc->ptcb_list){
  return -1;
}

refcountIncr(ptcb);
kernel_wait(& ptcb->exit_cv, SCHED_USER);
if(ptcb->exited ==0){
  return -1;
}

if(ptcb->exited ==1){
  exitval= & ptcb->exitval;
  refcountDec(ptcb);
  return 0;
}
return -1;

}

//new code by bill


/**
  @brief Detach the given thread.
  */
//new code by adiaforos

int sys_ThreadDetach(Tid_t tid)
{
  PTCB* ptcb= (PTCB* ) tid;
  PCB* pcb= CURPROC;


  if(rlist_find(& pcb->ptcb_list, ptcb, & pcb->ptcb_list)== & pcb->ptcb_list){
    return -1;
  }
  if(ptcb->exited==1){
    return -1;
  }

  kernel_broadcast(& ptcb->exit_cv);
  ptcb->refcount=0;
  ptcb->detached=1;
  return 0;

	
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
      ptcb->exited=1;
      ptcb->exitval=exitval;  //save the thread exitval to the ptcb exitval
      ptcb->tcb->state= EXITED;
      if(ptcb->refcount > 0){  //if there are some THREAD who haved join ptcb
          kernel_broadcast(& ptcb->exit_cv); //inform the other threads 
         //refcountDec(ptcb);   
      }else{
          //if refcount==0 then destroy the ptcb and remove it from the pcb's list
          rlist_remove(ptcb);
          free(ptcb);  

          }
   }
   kernel_sleep(EXITED, SCHED_USER); //koimisoy ton ypno toy dikaioy

}

//end code of lui

