#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/ptrace.h>

#define Error(msg) do { perror(msg); exit(0); } while(0)
#define PTRACE_E(req, pid, addr, data) \
    do { \
        if(ptrace(req, pid, addr, data) < 0) { \
            perror(#req); \
            exit(0); \
        } \
    } while(0)
#define BUF_SIZE 16

struct args_struct {
    char **margs; // master args
    char **sargs; // slave args
    int mcount;   // master args count
    int scount;   // slave args count
};

int parse_args(int argc, char **argv, struct args_struct *args) {
    int m_max, s_max, m_cur, s_cur;
    char m_or_s = 0;
    int i;

    m_max = 3;
    s_max = 2;
    m_cur = s_cur = 0;
    args->margs = malloc(m_max*sizeof(char*));
    args->sargs = malloc(s_max*sizeof(char*));

    if(args->margs == NULL) {
        return -1;
    } 
    if(args->sargs == NULL) {
        free(args->margs);
        return -1;
    }

    for(i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "-m")) {
            m_or_s = 'm';
            continue;
        } else if(!strcmp(argv[i], "-s")) {
            m_or_s = 's';
            continue;
        } else if(m_or_s == 'm') {
            if(m_cur == m_max-2) { // m_max-2 because we need one cell for pid and one for NULL
                m_max += 2;
                args->margs = realloc(args->margs, m_max);
                if(args->margs == NULL) goto error;
            }
            args->margs[m_cur++] = argv[i];
        } else if(m_or_s == 's') {
            if(s_cur == s_max-1) { // s_max-1 because we need one cell for NULL
                s_max += 2;
                args->sargs = realloc(args->sargs, s_max);
                if(args->sargs == NULL) goto error;
            }
            args->sargs[s_cur++] = argv[i];
        }
    }

    args->margs[m_cur+1] = NULL;
    args->sargs[s_cur] = NULL;
    args->mcount = m_cur+2;
    args->scount = s_cur+1;

    return 0;
error:
    if(args->margs) free(args->margs);
    if(args->sargs) free(args->sargs);

    return -1;
}

int tracer(char **argv, int fd) {
    pid_t pid;
    int status;
    struct user_regs_struct regs;

    pid = fork();
    if(pid < 0) {
        Error("fork");
    } else if(pid == 0) {
        if(execvp(argv[0], argv) < 0)
            Error("execvp");
    }

    PTRACE_E(PTRACE_ATTACH, pid, NULL, NULL);

    while(wait(&status) && !WIFEXITED(status)) {
        PTRACE_E(PTRACE_GETREGS, pid, NULL, &regs);

        if(regs.orig_eax == 26 && regs.ebx == PTRACE_TRACEME) {
            regs.eax = 0;

            PTRACE_E(PTRACE_SETREGS, pid, NULL, &regs);
            break;
        }

        PTRACE_E(PTRACE_SYSCALL, pid, NULL, NULL);
    }

    ptrace(PTRACE_DETACH, pid, NULL, SIGSTOP);
    write(fd, &pid, sizeof(pid));
    return 0;
}

int main(int argc, char **argv) {
    pid_t tpid, cpid;
    int pipefd[2];
    struct args_struct args;
    char buf[BUF_SIZE];

    if(argc < 5) {
        fprintf(stderr, "Usage: %s -m <tracer> [args] -s <traced> [args]\n", argv[0]);
        exit(0);
    }

    if(parse_args(argc, argv, &args) < 0)
        Error("malloc");

    if(pipe(pipefd) < 0)
        Error("pipe");

    tpid = fork();
    if(tpid < 0) {
        Error("fork");
    } else if(tpid == 0) {
        close(pipefd[0]);
        return tracer(args.sargs, pipefd[1]);
    }
    close(pipefd[1]);
    if(waitid(P_PID, tpid, NULL, WEXITED) < 0)
        Error("waitid");

    read(pipefd[0], &cpid, sizeof(cpid));

    snprintf(buf, BUF_SIZE, "%d", cpid);
    args.margs[args.mcount-2] = buf;
    if(execvp(args.margs[0], args.margs) < 0)
        Error("execvp");
}
