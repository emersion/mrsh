#include "shell/task.h"
#include <fnmatch.h>
#include <stdlib.h>

struct task_case_item {
	struct task *body;
	struct mrsh_array *patterns; // struct mrsh_word *
	size_t index;
	struct task *word;
};

struct task_case_clause {
	struct task task;

	struct {
		struct mrsh_word *word;
	} ast;
	struct {
		struct task *word;
	} tasks;

	char *word;
	struct task_case_item *selected;
	struct mrsh_array cases; // struct task_case_item *
	size_t index;
};

static void task_case_clause_destroy(struct task *task) {
	struct task_case_clause *tcc = (struct task_case_clause *)task;
	for (size_t i = 0; i < tcc->cases.len; ++i) {
		struct task_case_item *tci = tcc->cases.data[i];
		task_destroy(tci->body);
		free(tci);
	}
	mrsh_array_finish(&tcc->cases);
	task_destroy(tcc->tasks.word);
	mrsh_word_destroy(tcc->ast.word);
	free(tcc->word);
	free(tcc);
}

static int task_case_clause_poll(struct task *task, struct context *ctx) {
	struct task_case_clause *tcc = (struct task_case_clause *)task;
	if (tcc->tasks.word) {
		int word_status = task_poll(tcc->tasks.word, ctx);
		if (word_status < 0) {
			return word_status;
		}
		tcc->word = mrsh_word_str(tcc->ast.word);
		task_destroy(tcc->tasks.word);
		tcc->tasks.word = NULL;
	}

	while (!tcc->selected && tcc->index < tcc->cases.len) {
		struct task_case_item *tci =
			(struct task_case_item *)tcc->cases.data[tcc->index];
		if (tci->word) {
			int word_status = task_poll(tci->word, ctx);
			if (word_status < 0) {
				return word_status;
			}
			struct mrsh_word_string *word = (struct mrsh_word_string *)
				tci->patterns->data[tci->index - 1];
			task_destroy(tci->word);
			tci->word = NULL;
			if (fnmatch(word->str, tcc->word, 0) == 0) {
				tcc->selected = tci;
				break;
			}
			if (tci->index == tci->patterns->len) {
				++tcc->index;
			}
		} else {
			struct mrsh_word **word_ptr =
				(struct mrsh_word **)&tci->patterns->data[tci->index++];
			tci->word = task_word_create(word_ptr, TILDE_EXPANSION_NAME);
		}
	}

	if (tcc->selected) {
		return task_poll(tcc->selected->body, ctx);
	}

	return 0;
}

static const struct task_interface task_case_clause_impl = {
	.destroy = task_case_clause_destroy,
	.poll = task_case_clause_poll,
};

struct task *task_case_clause_create(
		const struct mrsh_word *word, const struct mrsh_array *cases) {
	struct task_case_clause *task = calloc(1, sizeof(struct task_case_clause));
	task_init(&task->task, &task_case_clause_impl);
	if (!mrsh_array_reserve(&task->cases, cases->len)) {
		free(task);
		return NULL;
	}
	for (size_t i = 0; i < cases->len; ++i) {
		struct mrsh_case_item *mci = cases->data[i];
		struct task_case_item *tci = calloc(1, sizeof(struct task_case_item));
		mrsh_array_add(&task->cases, tci);
		tci->patterns = &mci->patterns;
		tci->body = task_for_command_list_array(&mci->body);
	}
	task->ast.word = mrsh_word_copy(word);
	task->tasks.word = task_word_create(&task->ast.word, TILDE_EXPANSION_NAME);
	return (struct task *)task;
}
