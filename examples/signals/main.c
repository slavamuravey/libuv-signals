#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uv.h>

uv_loop_t* create_loop()
{
    uv_loop_t *loop = malloc(sizeof(uv_loop_t));
    if (loop) {
      uv_loop_init(loop);
    }
    return loop;
}

void signal_handler_usr1(uv_signal_t *handle, int signum)
{
    printf("Signal received: %d\n", signum);
    uv_signal_stop(handle);
}

void signal_handler_int1(uv_signal_t *handle, int signum)
{
    printf("SIGINT 1 Signal received: %d\n", signum);
}

void signal_handler_int2(uv_signal_t *handle, int signum)
{
    printf("SIGINT 2 Signal received: %d\n", signum);
}

int main()
{
    printf("PID %d\n", getpid());

    uv_loop_t *loop1 = create_loop();

    uv_signal_t sig1, sig2, sig3, sig4;
    uv_signal_init(loop1, &sig1);
    uv_signal_start(&sig1, signal_handler_usr1, SIGUSR1);

    uv_signal_init(loop1, &sig2);
    uv_signal_start(&sig2, signal_handler_usr1, SIGUSR1);

    uv_signal_init(loop1, &sig3);
    uv_signal_start(&sig3, signal_handler_int1, SIGINT);
    uv_unref((uv_handle_t *)&sig3);

    uv_signal_init(loop1, &sig4);
    uv_signal_start(&sig4, signal_handler_int2, SIGINT);
    uv_unref((uv_handle_t *)&sig4);

    uv_run(loop1, UV_RUN_DEFAULT);
    
    return 0;
}
