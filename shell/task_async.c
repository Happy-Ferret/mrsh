#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell.h"

struct task_async {
	struct task task;
	struct task *async;
	bool started;
};

static void task_async_destroy(struct task *task) {
	struct task_async *ta = (struct task_async *)task;
	task_destroy(ta->async);
	free(ta);
}

static int fork_detached(void) {
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return -1;
	} else if (pid == 0) {
		pid_t child_pid = fork();
		if (child_pid < 0) {
			fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
			return -1;
		} else if (child_pid == 0) {
			return 0;
		} else {
			exit(EXIT_SUCCESS);
		}
	} else {
		if (waitpid(pid, NULL, 0) == -1) {
			fprintf(stderr, "failed to waitpid(): %s\n", strerror(errno));
			return -1;
		}

		return 1;
	}
}

static bool task_async_start(struct task *task, struct context *ctx) {
	struct task_async *ta = (struct task_async *)task;

	// Start a subshell
	int ret = fork_detached();
	if (ret < 0) {
		return false;
	} else if (ret == 0) {
		if (ctx->stdin_fileno >= 0) {
			dup2(ctx->stdin_fileno, STDIN_FILENO);
		}
		if (ctx->stdout_fileno >= 0) {
			dup2(ctx->stdout_fileno, STDOUT_FILENO);
		}

		int ret = task_run(ta->async, ctx);
		if (ret < 0) {
			exit(127);
		}

		exit(ret);
	} else {
		if (ctx->stdin_fileno >= 0) {
			close(ctx->stdin_fileno);
		}
		if (ctx->stdout_fileno >= 0) {
			close(ctx->stdout_fileno);
		}

		return true;
	}
}

static int task_async_poll(struct task *task, struct context *ctx) {
	struct task_async *ta = (struct task_async *)task;

	if (!ta->started) {
		if (!task_async_start(task, ctx)) {
			return TASK_STATUS_ERROR;
		}
		ta->started = true;
	}

	return 0;
}

static const struct task_interface task_async_impl = {
	.destroy = task_async_destroy,
	.poll = task_async_poll,
};

struct task *task_async_create(struct task *async) {
	struct task_async *ta = calloc(1, sizeof(struct task_async));
	task_init(&ta->task, &task_async_impl);
	ta->async = async;
	return &ta->task;
}
