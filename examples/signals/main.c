#include <stdio.h>
#include <unistd.h>
#include <uv.h>

/* SIGUSR1 */
void sigusr1_handler_1(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[1] SIGUSR1 received\n");
}

void sigusr1_handler_2(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[2] SIGUSR1 received\n");
}

void sigusr1_handler_3(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[3] SIGUSR1 received\n");
}

/* SIGUSR2 */
void sigusr2_handler_1(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[1] SIGUSR2 received\n");
}

void sigusr2_handler_2(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[2] SIGUSR2 received\n");
}

void sigusr2_handler_3(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[3] SIGUSR2 received\n");
}

/* SIGINT */
void sigint_handler_1(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[1] SIGINT received\n");
}

void sigint_handler_2(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[2] SIGINT received\n");
}

void sigint_handler_3(uv_signal_t *handle, int signum)
{
    /* UNSAFE: This handler uses non-async-signal-safe function printf() */
    printf("[3] SIGINT received\n");
}

int main()
{
    uv_loop_t loop;

    printf("PID %d\n", getpid());
    
    uv_loop_init(&loop);

    uv_signal_t sigusr1_1, sigusr1_2, sigusr1_3;
    uv_signal_t sigusr2_1, sigusr2_2, sigusr2_3;
    uv_signal_t sigint_1, sigint_2, sigint_3;
    
    /* SIGUSR1 */
    uv_signal_init(&loop, &sigusr1_1);
    uv_signal_start(&sigusr1_1, sigusr1_handler_1, SIGUSR1);

    uv_signal_init(&loop, &sigusr1_2);
    uv_signal_start(&sigusr1_2, sigusr1_handler_2, SIGUSR1);

    uv_signal_init(&loop, &sigusr1_3);
    uv_signal_start(&sigusr1_3, sigusr1_handler_3, SIGUSR1);

    /* SIGUSR2 */
    uv_signal_init(&loop, &sigusr2_1);
    uv_signal_start(&sigusr2_1, sigusr2_handler_1, SIGUSR2);

    uv_signal_init(&loop, &sigusr2_2);
    uv_signal_start(&sigusr2_2, sigusr2_handler_2, SIGUSR2);

    uv_signal_init(&loop, &sigusr2_3);
    uv_signal_start(&sigusr2_3, sigusr2_handler_3, SIGUSR2);

    /* SIGINT */
    uv_signal_init(&loop, &sigint_1);
    uv_signal_start(&sigint_1, sigint_handler_1, SIGINT);

    uv_signal_init(&loop, &sigint_2);
    uv_signal_start(&sigint_2, sigint_handler_2, SIGINT);

    uv_signal_init(&loop, &sigint_3);
    uv_signal_start(&sigint_3, sigint_handler_3, SIGINT);

    uv_run(&loop, UV_RUN_DEFAULT);
    
    return 0;
}
