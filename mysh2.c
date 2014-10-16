#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> //wait
#include <sys/wait.h> //wait
#include <fcntl.h> //open
#include <sysexits.h> //exit
#include <signal.h> //signal
#include <errno.h> //open
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <pwd.h>

#define LBUFSIZE 256
#define NARGS 32
#define MAXBG 20	//max background
#define PPMAX 10	//max pipe
#define STOP 0
#define RUN 1

typedef struct jobs_proc {
  int job_num;
  int stat; //0:stopped 1:running
  pid_t pid;
  char name[40];
  struct jobs_proc *prev;
  struct jobs_proc *next;
} JOBS;

void getargs(char *buf, int *argc, char *argv[], int flag[]);
void changed(int argc, char *argv[]);
void redirect(int argc, char *argv[], int flag[]);	//
void pp(int argc, char *argv[], int flag[]);
void ctrl_IGN(int sig);
void kill_proc(int sig); 
void stp_proc(int sig); 
void chld_proc(int sig);
int fg(int argc, char *argv[]);
int bg(int argc, char *argv[]);
JOBS *killCommand(int arg, char *argv[]);
void bground(int sig); 
void quit(); 
void set_sigign();
void set_sigdfl();
void push_jobs(int argc, char *argv[], int flag[]);
JOBS *pop_jobs(int argc, char *argv[]);
void print_jobs();
void remove_jobs(JOBS *p);
JOBS *search_jobs(pid_t pid);

pid_t shell_pgid; //shell pid & pgid 
int ctrl_flag; //Ctrl-C & Ctrl-Z flag 
int sigint_flag; 
int sigtstp_flag; 
pid_t child_pid;
JOBS bg_head = {0, 0, 0, "", &bg_head, &bg_head};
char prompt1[100] = {'['};
char prompt2[100] = {'\0'};

void pp(int argc, char *argv[], int flag[]) {
  int i, j;
  int stat1, stat2;
  int pp_pos[PPMAX] = {0};
  int *pfd2[PPMAX];
  pid_t pgid, pid;

  pgid = getpid();
  setpgid(pgid, pgid);

  //signal(SIGTSTP, stp_proc);
  //signal(SIGINT, kill_proc);
  //signal(SIGCHLD, SIG_IGN);
  //set_sigign();
  set_sigdfl();

  for (i = 0, j = 1; i < argc; i++) {
    if (!strcmp(argv[i], "|")) {
      argv[i] = NULL;
      pp_pos[j++] = i + 1;			
    }
  }

  for (i = 0; i < flag[1]; i++) {
    pfd2[i] = (int *)malloc(sizeof(int) * 2);
  }

  for (i = 0; i < flag[1]; i++) {
    pipe(pfd2[i]);
  }

  for (i = 0; i < flag[1]; i++) {
    if (!i) {
      if ((pid = fork()) == 0) {
	set_sigdfl();
      	close(1);
	dup(*(pfd2[i] + 1));
	close(*pfd2[i]);
	close(*(pfd2[i] + 1));

	//debug
	fprintf(stderr, "1: %s id = %d\n", argv[pp_pos[i]], getpgrp());

	if (execvp(argv[pp_pos[i]], &argv[pp_pos[i]]) < 0) {
	  perror("execvp");
	  exit(1);
	}
      } else setpgid(pid, pgid);
    } else if (flag[1] == i + 1){
      if ((pid = fork()) == 0) {
	set_sigdfl();
	close(0);
	dup(*(pfd2[i]));
	close(*pfd2[i]);
	close(*(pfd2[i] + 1));

	//debug
	fprintf(stderr, "2: %s id = %d\n", argv[pp_pos[i + 1]], getpgrp());

	if (flag[0]) redirect(argc, argv, flag);
	if (execvp(argv[pp_pos[i+1]], &argv[pp_pos[i+1]]) < 0) {
	  perror("execvp");
	  exit(1);
	}
      } else setpgid(pid, pgid);
    } 
    if (!(flag[1] == i + 1 && flag[1] != 1)) {
      if ((pid = fork()) == 0) {
	set_sigdfl();
	close(0);
	dup(*(pfd2[i]));
	close(*pfd2[i]);
	close(*(pfd2[i] + 1));
	if (flag[1] > 1) {
	  close(1);
	  dup(*(pfd2[i+1] + 1));
	  close(*pfd2[i+1]);
	  close(*(pfd2[i+1] + 1));
	}

	//debug
	fprintf(stderr, "3: %s id = %d\n", argv[pp_pos[i + 1]], getpgrp());

	if (flag[0] && flag[1] == 1) redirect(argc, argv, flag);
	if (execvp(argv[pp_pos[i+1]], &argv[pp_pos[i+1]]) < 0) {
	  perror("execvp");
	  exit(1);
	}
      } else setpgid(pid, pgid);
    }

    close(*(pfd2[i]));
    close(*(pfd2[i] + 1));
    wait(&stat1);
    wait(&stat2);
  }
  for (i = 0; i < flag[1]; i++) {
    free(pfd2[i]);
  }

  //debug
  fprintf(stderr, "\n");
  exit(0);
}

void quit() {
  exit(0);
}

void set_sigign() {
  if (signal(SIGINT, ctrl_IGN) == SIG_ERR) {
    perror("signal");
  }

  if (signal(SIGTSTP, ctrl_IGN) == SIG_ERR) {
    perror("signal");
  }

  if (signal(SIGQUIT, ctrl_IGN) == SIG_ERR) {
    perror("signal");
  }

  sigint_flag = 0;
  sigtstp_flag = 0;
}

void set_sigdfl() {
  if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
    perror("signal");
  }

  if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
    perror("signal");
  }

  if (signal(SIGQUIT, SIG_DFL) == SIG_ERR) {
    perror("signal");
  }

  if (signal(SIGTTOU, SIG_DFL) == SIG_ERR) {
    perror("signal");
  }
  
  if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
    perror("signal");
  }
}

void ctrl_IGN(int sig) {
  //fprintf(stderr, "call ctrl_IGN pgid = %d", getpgid(getpid()));
  
  fprintf(stderr, "\n");
  //fprintf(stderr, "mysh%%");
  fprintf(stderr, "%s%s$", prompt1, prompt2);

  ctrl_flag = 1;
}

void kill_proc(int sig) {
  //debug
  fprintf(stderr, "\n");
  fprintf(stderr, "call kill_proc pgid = %d\n", getpgid(getpid()));
  fprintf(stderr, "kill pgid = %d\n", child_pid);
  fprintf(stderr, "\n");

  tcsetpgrp(0, shell_pgid); //set foreground shell

  sigint_flag = 1;
  ctrl_flag = 0;
  if (getpid() == shell_pgid) {
    if (killpg(child_pid, SIGKILL) < 0) {
      perror("killpg");
    }
  } else {
    if (killpg(getpid(), SIGKILL) < 0) {
      perror("killpg");
    }
  }
}

void stp_proc(int sig) {
  pid_t pgid;
 
  signal(SIGTSTP, SIG_IGN);

  //debug
  fprintf(stderr, "call stp_proc pgid = %d\n", getpgid(getpid()));
  fprintf(stderr, "stp pgid = %d\n", child_pid);
  fprintf(stderr, "\n");
  
  if ((pgid = getpgid(getpid())) == shell_pgid) {
    tcsetpgrp(0, shell_pgid); //set foreground shell
    tcsetpgrp(1, shell_pgid);
    fprintf(stderr, "shellpid stp\n");
  } else {
    tcsetpgrp(0, shell_pgid); //set foreground shell
    tcsetpgrp(1, shell_pgid);
    killpg(pgid, SIGTSTP);
    fprintf(stderr, "otherpid stp\n");
    //killpg(shell_pgid, SIGCHLD);
  }

  sigtstp_flag = 1;
  ctrl_flag = 0;

  signal(SIGTSTP, SIG_DFL);
}

void chld_proc(int sig) {
  //fprintf(stderr, "call chld_proc pgid = %d\n", getpgid(getpid()));
  //fprintf(stderr, "chld pgid = %d\n", child_pid);
  //fprintf(stderr, "\n");

  //killpg(shell_pgid, SIGCONT);
  if (getpgid(getpid()) == shell_pgid) {
    fprintf(stderr, "set foreground shell\n");
    tcsetpgrp(0, shell_pgid);
    tcsetpgrp(1, shell_pgid);
  }
  //signal(SIGTSTP, SIG_DFL);
}

int fg(int argc, char *argv[]) {
  int status;
  JOBS *p;
  
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTSTP, stp_proc);
  signal(SIGCHLD, chld_proc);
  signal(SIGINT, kill_proc);
 
  //---
  signal(SIGCHLD, SIG_IGN);
  //---
  
  if (!(p = pop_jobs(argc, argv))) return -1;
  
  if (killpg(p->pid, SIGCONT) < 0) {
    perror("killpg");
  }


  fprintf(stderr, "send signal %d\n", p->pid);
  tcsetpgrp(0, p->pid);
  tcsetpgrp(1, p->pid);
  //killpg(SIGCONT, shell_pgid);
  waitpid(-p->pid, &status, WUNTRACED);
  signal(SIGCHLD, chld_proc);
  killpg(shell_pgid, SIGCONT);
  tcsetpgrp(0, shell_pgid);
  tcsetpgrp(1, shell_pgid);

  if (!WIFSTOPPED(status)) remove_jobs(p);
  else p->stat = STOP;

  return 0;
}

int bg(int argc, char *argv[]) {
  JOBS *p;

  signal(SIGTTOU, SIG_IGN);
  signal(SIGTSTP, stp_proc);
  signal(SIGCHLD, chld_proc);

  signal(SIGCHLD, SIG_IGN);
  
  if (!(p = pop_jobs(argc, argv))) return -1;
  if (p->stat == RUN) {
    fprintf(stderr, "%d is running\n", p->job_num);
    return 0;
  } else p->stat = RUN;

  if (killpg(p->pid, SIGCONT) < 0) {
    perror("killpg");
  }

  fprintf(stderr, "send signal %d\n", p->pid);
  killpg(SIGCONT, shell_pgid);
  signal(SIGCHLD, chld_proc);
  return 0;
}
  
JOBS *killCommand(int argc, char *argv[]) {
  int num;
  JOBS *p;
  static char pid_num[15];
  if (argc != 2) return NULL;
  if (!(num = atoi(&argv[1][1]))) return NULL;
  for (p = bg_head.next; p != &bg_head; p = p->next) {
    if (p->job_num == num) break;
  }
  sprintf(pid_num, "%d", p->pid);
  argv[2] = pid_num;
  argv[1] = "-KILL";
  argv[3] = NULL;
  //fprintf(stderr, "%s %s %s\n", argv[0], argv[1], argv[2]);
  return p;
}


int main() {
  int flag[4] = {0};//0:rd_flag, 1:pipe_flag,2:bg_flag,3:error_flag
  char lbuf[LBUFSIZE], *argv[NARGS];
  int status, argc;
  pid_t finpid;
  JOBS *p;
  struct passwd *pw;
  char temp[100];
  char *cdirec, *c;
  int len;

  pw = getpwuid(getuid());
  strcat(prompt1, pw->pw_name);
  strcat(prompt1, "@localhostt ");
  getcwd(temp, 100);
  len = strlen(temp);
  for (c =  &temp[len - 1]; c != temp; c--) {
    if (*c == '/') {
      cdirec = c + 1;
      break;
    }
  }
  strcpy(prompt2, cdirec);
  strcat(prompt2, "]");

  //----------set shell pgid----------//
  shell_pgid = getpid();
  setpgid(shell_pgid, shell_pgid);
  fprintf(stderr, "set shell pgid = %d\n", shell_pgid);
  fprintf(stderr, "\n");
  //----------set end pgid----------//
  
  for ( ; ; ) {
    for (;(finpid = wait3(&status, WNOHANG, 0)) > 0; ) {
      if ((p = search_jobs(finpid)) != NULL) {
	fprintf(stderr, "[%d]  finish          %s\n", 
		p->job_num, p->name);
	remove_jobs(p);
      }
    }

    set_sigign();

    if (!ctrl_flag) fprintf(stderr, "%s%s$", prompt1, prompt2);
    //fprintf(stdout, "mysh%%");
    else ctrl_flag = 0;

    //----------readline----------//
    if (fgets(lbuf, LBUFSIZE, stdin) == NULL) {
      if (feof(stdin)) exit(0);
      perror("fgets");
      clearerr(stdin);
      continue;
    }
    lbuf[strlen(lbuf) - 1] = '\0';
    getargs(lbuf, &argc, argv, flag);
    argv[argc] = NULL;
    //----------end readline----------//


    //----------input handling----------//
    if (!argc) { //no input
      ctrl_flag = 0;
      continue;
    }
    if (flag[3]) { //input error
      fprintf(stderr, "ERROR:input error.\n");
      continue;
    }

    if (!strcmp(argv[0], "exit")) { //quit myshell
      quit();
    } else if (!strcmp(argv[0], "cd")) { //change directory
      changed (argc, argv);

      len = strlen(temp);
      getcwd(temp, 100);
      len = strlen(temp);
      for (c =  &temp[len - 1]; c != temp; c--) {
	if (*c == '/') {
	  cdirec = c + 1;
	  break;
	}
      }
      strcpy(prompt2, cdirec);
      strcat(prompt2, "]");
      continue;
    } 

    if (flag[2]) { //back ground
      argv[argc - 1] = NULL;
    } 

    if (!strcmp(argv[0], "fg")) {
      if (fg(argc, argv)) 
	fprintf(stderr, "not found bg process\n");
      continue;
    } 

    if (!strcmp(argv[0], "bg")) {
      if (bg(argc, argv)) 
	fprintf(stderr, "not found bg process\n");
      ctrl_flag = 0;
      continue;
    } 

    if (!strcmp(argv[0], "jobs")) {
      print_jobs();
      ctrl_flag = 0;
      continue;
    }

    if (!strcmp(argv[0], "kill") && (argv[1][0] == '%')) {
      JOBS *q;
      if (!(q = killCommand(argc, argv))) {
	fprintf(stderr, "kill: %s: processID or %%job number.", 
		argv[1]);
	exit(0);
      } else if (q == &bg_head) {
	fprintf(stderr, "kill: %s: there is no such job.",
		argv[1]);
	exit(0);
      }
      fprintf(stderr, "[%d]   Terminated: %s\n", 
	      q->job_num, q->name);
      remove_jobs(q);
    }
    //----------input handling end----------//

    //----------------fork-----------------//
    if ((child_pid = fork()) < 0) {
      //----------error----------//
      perror("fork");
      return -1;

    } else if (child_pid == 0) { 
      //----------child process----------//
      if (!flag[1]) setpgid(getpid(), getpid());
      //signal(SIGINT, kill_proc);
      //signal(SIGTSTP, stp_proc);
      //signal(SIGSTOP, stp_proc);
      //signal(SIGTTOU, SIG_IGN);
      
      if (flag[1]) { //pipe phase
	if (flag[1] > PPMAX) exit(0);//continue;
	pp(argc, argv, flag);
      } else if (flag[0]) { //redirect phase
	redirect(argc, argv, flag);
      } 
      if (execvp(argv[0], argv) < 0) {
	perror("execvp");
	return -1;
      } 

      exit(0);
    } else {
      //----------parent process----------//
      signal(SIGINT, kill_proc);
      signal(SIGTSTP, stp_proc);
      signal(SIGCHLD, chld_proc);
      signal(SIGTTOU, SIG_IGN);

      if (!flag[2]) { //is not background flag
	tcsetpgrp(0, child_pid); //set foreground child process
	tcsetpgrp(1, child_pid);
	fprintf(stderr, "child_pid = %d\n", child_pid);
	waitpid(-1, &status, WUNTRACED);
	if (WIFSTOPPED(status)) {
	  fprintf(stderr, "stop process %d\n", child_pid);
	  push_jobs(argc, argv, flag);	
	}
	
	ctrl_flag = 0;
      } else {
	tcsetpgrp(0, shell_pgid);
	tcsetpgrp(1, shell_pgid);
	push_jobs(argc, argv, flag);
	ctrl_flag = 0;
	if (killpg(shell_pgid, SIGCONT) < 0) {
	  perror("killpg");
	}
      }
    } // end fork if
  } //end while
  return 0;
}

void push_jobs(int argc, char *argv[], int flag[]) {
  int len, i;
  JOBS *p;

  p = (JOBS *)malloc(sizeof(JOBS));
  bzero(p, sizeof(JOBS));

  p->job_num = bg_head.prev->job_num + 1;
  p->pid = child_pid;

  p->next = &bg_head;
  p->prev = bg_head.prev;
  bg_head.prev->next = p;
  bg_head.prev = p;

  if (flag[2]) {
    p->stat = 1;
    argc--;
  } else p->stat = 0;

  if (!flag[1]) {
    for (i = 0; i < argc; i++) {
      len = strlen(argv[i]);
      strncat(p->name, argv[i], len);
      if (i + 1 != argc) strcat(p->name, " ");
    }
    
    if (flag[2]) strcat(p->name, "&");
    fprintf(stderr, "p->name = %s\n", p->name);
  } else {
    int pipe_pos;
    for (i = argc - 1; i > 0; i--) {
      if (!strcmp(argv[i], "|")) {
	pipe_pos = i;
	break;
      }
    }
    for (i = pipe_pos + 1; i < argc; i++) {
      len = strlen(argv[i]);
      strncat(p->name, argv[i], len);
      if (i + 1 != argc) strcat(p->name, " ");
    }
    fprintf(stderr, "p->name = %s\n", p->name);
  }
}

JOBS *pop_jobs(int argc, char *argv[]) {
  int num;
  JOBS *p;

  if (argc == 1) {
    if (bg_head.prev != &bg_head) return bg_head.prev; 
    else return NULL;
  }
  if (argc != 2) return NULL;

  if (!(num = atoi(argv[1]))) return NULL;

  for (p = bg_head.next; p != &bg_head; p = p->next) {
    if (p->job_num == num) return p;
  }
  return NULL;
}

void remove_jobs(JOBS *p) {
  p->prev->next = p->next;
  p->next->prev = p->prev;
  free(p);
}
    

void print_jobs() {
  JOBS *p;

  for (p = bg_head.next; p != &bg_head; p = p->next) {
    fprintf(stderr, "[%d]   ", p->job_num);
    if (p->stat) fprintf(stderr, "Running          ");
    else fprintf(stderr, "Stopped          ");
    fprintf(stderr, "%s     pid = %d\n", p->name, p->pid);
  }
}

JOBS *search_jobs(pid_t pid) {
  JOBS *p;
  
  for (p = bg_head.next; p != &bg_head; p = p->next) {
    if (pid == p->pid) return p;
  }
  return NULL;
}

//0:rd_flag, 1:pipe_flag,2:bg_flag,3:error_flag 
void getargs(char *buf, int *argc, char *argv[], int flag[]) { 
  static char moji[4][2] = {"<", ">", "|", "&"}; 
  int len; 
  int i; 
  char *s1, *s2; 

  for (i = 0; i < 4; i++) { 
    flag[i] = 0; 
  } 

  for (*argc = 0, len = 0, s1 = s2 = buf; *s1 != '\n'; s1++) { 
    if (*s1 == '\0' && !len) break;	   
    if (isblank((int)*s1) && !len) { 
      s2++; 
    } else if (isblank((int)*s1) && len) { 
      *s1 = '\0'; 
      argv[*argc] = s2; 
      (*argc)++; 
      s2 = s1 + 1; 
      len = 0; 
    } else if ((*s1 == '\n' || *s1 == '\0') && len) { 
      argv[*argc] = s2; 
      (*argc)++; 
      break; 
    } else if (*s1 == '<' || *s1 == '>'  
 	       || *s1 == '|' || *s1 == '&') { 
      if (len) { 
 	argv[*argc] = s2; 
 	(*argc)++; 
      } 
      if (*s1 == '<') { 
 	if (!flag[0] ||  
 	    (flag[0] == 1 && !strcmp(argv[*argc - 1], "<"))) { 
 	  argv[*argc] = moji[0]; 
 	  flag[0]++; 
 	} else { 
 	  flag[3] = 1; 
 	  return; 
 	} 
      } else if (*s1 == '>') { 
 	if (!flag[0] || 
 	    (flag[0] == 1 && !strcmp(argv[*argc - 1], ">"))) { 
 	  argv[*argc] = moji[1]; 
 	  flag[0]++; 
 	} else { 
 	  flag[3] = 1; 
 	  return; 
 	} 
      } else if (*s1 == '|') { 
 	argv[*argc] = moji[2]; 
 	flag[1]++; 
      } else if (*s1 == '&') { 
 	if (!flag[2]) { 
 	  argv[*argc] = moji[3]; 
 	  flag[2] = 1; 
 	} else { 
 	  flag[3] = 1; 
 	  return; 
 	} 
      } 
      *s1 = '\0'; 
      (*argc)++; 
      s2 = s1 + 1; 
      len = 0; 
    } else { 
      len++; 
    } 
  } 

  if (flag[2] && strcmp(argv[*argc - 1], "&")) { 
    flag[3] = 1; 
    return; 
  } 

  if (*argc > 0) {  
    for (i = 0; i < 3; i++) { 
      if (!strcmp(argv[*argc - 1], moji[i])) { 
 	flag[3] = 1; 
 	break; 
      } 
    } 
  } 

} 

void changed(int argc, char *argv[]) { 
  if (argc == 1) argv[1] = getenv("HOME"); 
  if (chdir(argv[1]) != 0) { 
    perror("chdir"); 
  } 
} 

void redirect(int argc, char *argv[], int flag[]) { 
  int i; 
  int fd;
  
  for (i = argc - 1; i > 0; i--) {
    if (!strcmp(argv[i], ">")) {
      argv[i] = NULL;
      if (flag[0] == 2) {
	argv[i - 1] = NULL;
	printf("%d\n", i);
	if ((fd = open(argv[i + 1], 
		       O_WRONLY|O_CREAT|O_APPEND, 0644)) < 0) {
	  if (errno != EEXIST) {
	    perror("open");
	    exit(1);
	  }
	}
      }else {
	printf("%d\n", i);
	if ((fd = open(argv[argc - 1], 
		       O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
	  if (errno != EEXIST) {
	    perror("open");
	    exit(1);
	  }
	}
      }
      close(1);
      dup(fd);
      close(fd);
      break;
    } else if (!strcmp(argv[i], "<")) {
      argv[i] = NULL;
      if ((fd = open(argv[argc - 1], O_RDONLY, 0644)) < 0) {
	if (errno != EEXIST) {
	  perror("open");
	  exit(1);
	}
      }
      close(0);
      dup(fd);
      close(fd);
      break;
    }
  }
}


