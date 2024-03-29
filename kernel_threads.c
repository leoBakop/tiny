
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"
#include "kernel_streams.h"





/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  
  PCB* pcb = CURPROC ;
  PTCB *ptcb;
  
  if(task != NULL)
  { 
    TCB* tcb = spawn_thread(pcb, start_another_thread); //spawn thread also connects ptcb with tcb
    ptcb=initialize_PTCB(); //check if curproc is correct
    ptcb->args = args;
    ptcb->argl=argl;
    ptcb->tcb=tcb;
    tcb->owner_ptcb=ptcb;
    //new code
    ptcb->task=task;
    //end of new code
    rlist_push_front(& pcb->ptcb_list,& ptcb->ptcb_list_node); 
    pcb->thread_count ++;
    wakeup(tcb); //etoimase ena tcb gia ton scheduler
  }

  return (Tid_t) ptcb;  
}


//new code by bill
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->owner_ptcb;
}

/**
  @brief Join the given thread.
  */

int sys_ThreadJoin(Tid_t tid, int* exitval){

  if(tid==sys_ThreadSelf()){
    return -1; 
  }
  if(tid == NOTHREAD){
    return -1;
  }

  PTCB* ptcb= (PTCB*) tid;
  PCB* curproc= CURPROC;
  rlnode* node = rlist_find(&curproc->ptcb_list, ptcb, NULL); //node is the first node that contains the ptcb
  /*if this node doesn't exist (is null)
  *it means that the given ptcb
  *doesn't belong in this 
  *PCB
  */
 if(node==NULL){
  return -1;
} 

if(ptcb->detached==1 ){
  return -1;
}

refcountIncr(ptcb);
while(ptcb->exited==0 && ptcb->detached==0){
  kernel_wait(& ptcb->exit_cv, SCHED_USER);
}
if(ptcb->detached == 1 ){
  ptcb->refcount--;
  return -1;
}

if(ptcb->exited ==1){
  if(exitval!=NULL)
    *exitval=ptcb->exitval;
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
  
  if(tid == NOTHREAD)
    return -1;
  if(tid<0)
    return -1;

  PTCB* ptcb= (PTCB* ) tid;
  PCB* pcb= CURPROC;
  rlnode* node=rlist_find(& pcb->ptcb_list, ptcb, 0);

  if(node==NULL ){
    return -1;
  }
  if(ptcb->exited==1){
    return -1;
  }

  kernel_broadcast(& ptcb->exit_cv);
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
  PTCB* ptcb= cur_thread()->owner_ptcb;
  ptcb->exitval = exitval;
  ptcb->exited=1;
  kernel_broadcast(& ptcb->exit_cv);

    if (curproc->thread_count==0){
     if ( get_pid(curproc)!=1 ){
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
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);
  }

  assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));


  /* 
    Do all the other cleanup we want here, close files etc. 
   */

  /* Release the args data */
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

  // Clean up the threads that weren't joined, meaning that we kept the exit information for the case it was needed.
  while(!is_rlist_empty(& curproc->ptcb_list)){
    rlnode* ptcbio = rlist_pop_front(& curproc->ptcb_list);
    free(ptcbio->ptcb);
  }
  assert(is_rlist_empty(& curproc->ptcb_list));

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;
}else{
    CURPROC->thread_count--;
   }
   kernel_sleep(EXITED, SCHED_USER); //koimisoy ton ypno toy dikaioy

}

//end code of lui


