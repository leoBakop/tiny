
#include <assert.h>
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"
#include "util.h"

/* 
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];
unsigned int process_count;

PCB* get_pcb(Pid_t pid)
{
  return PT[pid].pstate==FREE ? NULL : &PT[pid];
}

Pid_t get_pid(PCB* pcb)
{
  return pcb==NULL ? NOPROC : pcb-PT;
}

/* Initialize a PCB */
static inline void initialize_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;

  for(int i=0;i<MAX_FILEID;i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(& pcb->children_list, NULL);
  rlnode_init(& pcb->exited_list, NULL);
  rlnode_init(& pcb->ptcb_list, NULL);
  rlnode_init(& pcb->children_node, pcb);
  rlnode_init(& pcb->exited_node, pcb);

  pcb->child_exit = COND_INIT;

  //new code that was added by lui
  pcb->thread_count=0;
  //end of new code

}  


//start of new code by lui

/**
*initialize a PTCB
**/

 PTCB* initialize_PTCB(){

  PTCB* ptcb=(PTCB*)xmalloc(sizeof(PTCB));
    
    /* initialize in ptcb*/
	
	ptcb->tcb=NULL; //this variable initialized in spawnThread()
	ptcb->exited=0;
	ptcb->detached=0;
  ptcb->task = NULL;
  ptcb->argl = 0;
  ptcb->args = NULL;
	ptcb->exit_cv = COND_INIT;
	ptcb->refcount=0;
	rlnode_init(& ptcb->ptcb_list_node, ptcb);

	/*inform pcb for the new ptcb*/
	//rlist_push_back(& pcb->ptcb_list,& ptcb->ptcb_list_node);
	//pcb->thread_count++;
  return ptcb;

}

//end of new code by lui


void refcountIncr(PTCB* ptcb){
	ptcb->refcount++;
}

void refcountDec(PTCB* ptcb){
	ptcb->refcount--;
	if(ptcb->refcount==0){
		rlist_remove(& ptcb->ptcb_list_node);
		free(ptcb);
	}
}

//end of new code by lui
//end of new code by lui


static PCB* pcb_freelist;

void initialize_processes()
{
  /* initialize the PCBs */
  for(Pid_t p=0; p<MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* use the parent field to build a free list */
  PCB* pcbiter;
  pcb_freelist = NULL;
  for(pcbiter = PT+MAX_PROC; pcbiter!=PT; ) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Execute a null "idle" process */
  if(Exec(NULL,0,NULL)!=0)
    FATAL("The scheduler process does not have pid==0");
}


/*
  Must be called with kernel_mutex held
*/
PCB* acquire_PCB()
{
  PCB* pcb = NULL;

  if(pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}


/*
 *
 * Process creation
 *
 */

/*
	This function is provided as an argument to spawn,
	to execute the main thread of a process.
*/
void start_main_thread()
{
  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  Exit(exitval);
}

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
TCB* curthread= cur_thread();

Task call = curthread->owner_ptcb->task;
int argl=curthread->owner_ptcb->argl;
void* args= curthread->owner_ptcb->args;

exitval = call(argl,args);

sys_ThreadExit(exitval);
}

//end of new code

/*
	System call to create a new process.
 */
Pid_t sys_Exec(Task call, int argl, void* args)
{
  PCB *curproc, *newproc;
  
  /* The new process PCB */
  newproc = acquire_PCB();

  if(newproc == NULL) goto finish;  /* We have run out of PIDs! */

  if(get_pid(newproc)<=1) {
    /* Processes with pid<=1 (the scheduler and the init process) 
       are parentless and are treated specially. */
    newproc->parent = NULL;
  }
  else
  {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(& curproc->children_list, & newproc->children_node);

    /* Inherit file streams from parent */
    for(int i=0; i<MAX_FILEID; i++) {
       newproc->FIDT[i] = curproc->FIDT[i];
       if(newproc->FIDT[i])
          FCB_incref(newproc->FIDT[i]);
    }
  }


  /* Set the main thread's function */
  newproc->main_task = call;

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  if(args!=NULL) {
    newproc->args = malloc(argl);
    memcpy(newproc->args, args, argl);
  }
  else
    newproc->args=NULL;

  /* 
    Create and wake up the thread for the main function. This must be the last thing
    we do, because once we wakeup the new thread it may run! so we need to have finished
    the initialization of the PCB.
   */
 //new code segment by Alexandra :

  PTCB*  ptcb;

  ptcb=initialize_PTCB();
  rlnode_init(& newproc->ptcb_list, NULL);



if(call != NULL) {
    newproc->main_thread =spawn_thread(newproc,start_main_thread);
    newproc->main_thread->owner_ptcb=ptcb;
    ptcb->tcb=newproc->main_thread;
    ptcb->argl = newproc->argl; //new code by me
    ptcb->args = newproc->args; //new code by me
    rlist_push_back(& newproc->ptcb_list ,& ptcb->ptcb_list_node);
    wakeup(ptcb->tcb);
  }
finish:
  return get_pid(newproc);
}


/* System call */
Pid_t sys_GetPid()
{
  return get_pid(CURPROC);
}


Pid_t sys_GetPPid()
{
  return get_pid(CURPROC->parent);
}


static void cleanup_zombie(PCB* pcb, int* status)
{
  if(status != NULL)
    *status = pcb->exitval;

  rlist_remove(& pcb->children_node);
  rlist_remove(& pcb->exited_node);

  release_PCB(pcb);
}


static Pid_t wait_for_specific_child(Pid_t cpid, int* status)
{

  /* Legality checks */
  if((cpid<0) || (cpid>=MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB* parent = CURPROC;
  PCB* child = get_pcb(cpid);
  if( child == NULL || child->parent != parent)
  {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while(child->pstate == ALIVE)
    kernel_wait(& parent->child_exit, SCHED_USER);
  
  cleanup_zombie(child, status);
  
finish:
  return cpid;
}


static Pid_t wait_for_any_child(int* status)
{
  Pid_t cpid;

  PCB* parent = CURPROC;

  /* Make sure I have children! */
  if(is_rlist_empty(& parent->children_list)) {
    cpid = NOPROC;
    goto finish;
  }

  while(is_rlist_empty(& parent->exited_list)) {
    kernel_wait(& parent->child_exit, SCHED_USER);
  }

  PCB* child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);

finish:
  return cpid;
}


Pid_t sys_WaitChild(Pid_t cpid, int* status)
{
  /* Wait for specific child. */
  if(cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }

}


/**
*changes by lui
*comment the initial state
*/

void sys_Exit(int exitval)
{
  /* Right here, we must check that we are not the boot task. If we are, 
     we must wait until all processes exit. */
  if(sys_GetPid()==1) {
    while(sys_WaitChild(NOPROC,NULL)!=NOPROC);
  }

  PCB *curproc = CURPROC;  /* cache for efficiency */
  curproc->exitval = exitval;

  sys_ThreadExit(exitval);

}


/** methods for sysinfo*/




/**in this method, we memcpy and then we move the cursor */
int procinfo_read(void* procinfocb, char* buf, unsigned int size){
  procinfo_cb* prcb;
  prcb=(procinfo_cb*) procinfocb;
  int i=prcb->cursor;
  PCB* pcb=& PT[i];
  if(pcb->pstate==FREE)
   return NOFILE;

  prcb->procInfomv->pid=get_pid(pcb);
  prcb->procInfomv->ppid=get_pid(pcb->parent);

  switch(pcb->pstate){
    case ZOMBIE:
      prcb->procInfomv->alive=0;
      break;
    case ALIVE:
      prcb->procInfomv->alive=1;
      break;
    default:
    FATAL("error");
    break;
  }

  prcb->procInfomv->thread_count=pcb->thread_count;
  prcb->procInfomv->main_task=pcb->main_task;
  prcb->procInfomv->argl=pcb->argl;
  memcpy(&prcb->procInfomv->args,(char*)&pcb->args, sizeof(pcb->args));


  memcpy(buf, (char*) prcb->procInfomv, sizeof(*(prcb->procInfomv)));

  prcb->cursor++;
  while(PT[prcb->cursor].pstate==FREE&&prcb->cursor<MAX_PROC){
    prcb->cursor++;
  }

  if(prcb->cursor>=MAX_PROC-1)
    return NOFILE;

  return sizeof(prcb->procInfomv);

}





/** */
int procinfo_close(void* procinfocb){
  procinfo_cb* prcb;
  prcb=(procinfo_cb*) procinfocb;
  free(prcb);//free the procinfo
  return 0;
}

static file_ops procinfo_file_ops = {
  .Open = NULL,
  .Read = procinfo_read,
  .Write = NULL,
  .Close = procinfo_close
};


/**syscall that creates the proces who counts the processes */
Fid_t sys_OpenInfo(){
  Fid_t fid;
  FCB* fcb;
   if(FCB_reserve(1, &fid, &fcb)==0 ){
    printf("Failed to allocate console Fids\n");
    return NOFILE;
  }
  
 

  procinfo_cb* prcb=xmalloc(sizeof(procinfo_cb));
  prcb->procInfomv=xmalloc(sizeof(procinfo));


  prcb->cursor=0;//init is int the first position so we are sure that this is the next alive pcb


  fcb->streamobj=prcb;
  fcb->streamfunc= &procinfo_file_ops;


  return fid;
  
  
}









