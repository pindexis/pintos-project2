#include <stdbool.h>
typedef int pid_t;

void ipc_init(void);
int ipc_pipe_read(char *pipe_name, int ticket);
void ipc_pipe_write(char *pipe_name, int ticket, int msg);
