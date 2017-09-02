/****************************************************************************
 * bfs                                                                      *
 * Copyright (C) 2015-2017 Tavian Barnes <tavianator@tavianator.com>        *
 *                                                                          *
 * Permission to use, copy, modify, and/or distribute this software for any *
 * purpose with or without fee is hereby granted.                           *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES *
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF         *
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR  *
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN    *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF  *
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.           *
 ****************************************************************************/

#ifndef BFS_H
#define BFS_H

#include "color.h"
#include "exec.h"
#include "printf.h"
#include "mtab.h"
#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#ifndef BFS_VERSION
#	define BFS_VERSION "1.1.1"
#endif

#ifndef BFS_HOMEPAGE
#	define BFS_HOMEPAGE "https://github.com/tavianator/bfs"
#endif

/**
 * A command line expression.
 */
struct expr;

/**
 * Ephemeral state for evaluating an expression.
 */
struct eval_state;

/**
 * Expression evaluation function.
 *
 * @param expr
 *         The current expression.
 * @param state
 *         The current evaluation state.
 * @return
 *         The result of the test.
 */
typedef bool eval_fn(const struct expr *expr, struct eval_state *state);

/**
 * Various debugging flags.
 */
enum debug_flags {
	/** Print cost estimates. */
	DEBUG_COST   = 1 << 0,
	/** Print executed command details. */
	DEBUG_EXEC   = 1 << 1,
	/** Print optimization details. */
	DEBUG_OPT    = 1 << 2,
	/** Print rate information. */
	DEBUG_RATES  = 1 << 3,
	/** Trace the filesystem traversal. */
	DEBUG_SEARCH = 1 << 4,
	/** Trace all stat() calls. */
	DEBUG_STAT   = 1 << 5,
	/** Print the parse tree. */
	DEBUG_TREE   = 1 << 6,
};

/**
 * A root path to explore.
 */
struct root {
	/** The root path itself. */
	const char *path;
	/** The next path in the list. */
	struct root *next;
};

/**
 * The parsed command line.
 */
struct cmdline {
	/** The unparsed command line arguments. */
	char **argv;

	/** The list of root paths. */
	struct root *roots;

	/** Color data. */
	struct colors *colors;
	/** Colored stdout. */
	CFILE *cout;
	/** Colored stderr. */
	CFILE *cerr;

	/** Table of mounted file systems. */
	struct bfs_mtab *mtab;

	/** -mindepth option. */
	int mindepth;
	/** -maxdepth option. */
	int maxdepth;

	/** bftw() flags. */
	enum bftw_flags flags;

	/** Optimization level. */
	int optlevel;
	/** Debugging flags. */
	enum debug_flags debug;
	/** Whether to only handle paths with xargs-safe characters. */
	bool xargs_safe;
	/** Whether to ignore deletions that race with bfs. */
	bool ignore_races;

	/** The command line expression. */
	struct expr *expr;

	/** The number of open files used by the expression tree. */
	int nopen_files;
};

/**
 * Possible types of numeric comparison.
 */
enum cmp_flag {
	/** Exactly n. */
	CMP_EXACT,
	/** Less than n. */
	CMP_LESS,
	/** Greater than n. */
	CMP_GREATER,
};

/**
 * Possible types of mode comparison.
 */
enum mode_cmp {
	/** Mode is an exact match (MODE). */
	MODE_EXACT,
	/** Mode has all these bits (-MODE). */
	MODE_ALL,
	/** Mode has any of these bits (/MODE). */
	MODE_ANY,
};

/**
 * Available struct stat time fields.
 */
enum time_field {
	/** Access time. */
	ATIME,
	/** Status change time. */
	CTIME,
	/** Modification time. */
	MTIME,
};

/**
 * Possible time units.
 */
enum time_unit {
	/** Minutes. */
	MINUTES,
	/** Days. */
	DAYS,
};

/**
 * Possible file size units.
 */
enum size_unit {
	/** 512-byte blocks. */
	SIZE_BLOCKS,
	/** Single bytes. */
	SIZE_BYTES,
	/** Two-byte words. */
	SIZE_WORDS,
	/** Kibibytes. */
	SIZE_KB,
	/** Mebibytes. */
	SIZE_MB,
	/** Gibibytes. */
	SIZE_GB,
	/** Tebibytes. */
	SIZE_TB,
	/** Pebibytes. */
	SIZE_PB,
};

struct expr {
	/** The function that evaluates this expression. */
	eval_fn *eval;

	/** The left hand side of the expression. */
	struct expr *lhs;
	/** The right hand side of the expression. */
	struct expr *rhs;

	/** Whether this expression has no side effects. */
	bool pure;
	/** Whether this expression always evaluates to true. */
	bool always_true;
	/** Whether this expression always evaluates to false. */
	bool always_false;

	/** Estimated cost. */
	double cost;
	/** Estimated probability of success. */
	double probability;
	/** Number of times this predicate was executed. */
	size_t evaluations;
	/** Number of times this predicate succeeded. */
	size_t successes;
	/** Total time spent running this predicate. */
	struct timespec elapsed;

	/** The number of command line arguments for this expression. */
	size_t argc;
	/** The command line arguments comprising this expression. */
	char **argv;

	/** The optional comparison flag. */
	enum cmp_flag cmp_flag;

	/** The mode comparison flag. */
	enum mode_cmp mode_cmp;
	/** Mode to use for files. */
	mode_t file_mode;
	/** Mode to use for directories (different due to X). */
	mode_t dir_mode;

	/** The optional reference time. */
	struct timespec reftime;
	/** The optional time field. */
	enum time_field time_field;
	/** The optional time unit. */
	enum time_unit time_unit;

	/** The optional size unit. */
	enum size_unit size_unit;

	/** Optional device number for a target file. */
	dev_t dev;
	/** Optional inode number for a target file. */
	ino_t ino;

	/** File to output to. */
	CFILE *cfile;

	/** Optional compiled regex. */
	regex_t *regex;

	/** Optional exec command. */
	struct bfs_exec *execbuf;

	/** Optional printf command. */
	struct bfs_printf *printf;

	/** Optional integer data for this expression. */
	long long idata;

	/** Optional string data for this expression. */
	const char *sdata;
};

/**
 * @return Whether expr is known to always quit.
 */
bool expr_never_returns(const struct expr *expr);

/**
 * @return The result of the comparison for this expression.
 */
bool expr_cmp(const struct expr *expr, long long n);

/**
 * Parse the command line.
 */
struct cmdline *parse_cmdline(int argc, char *argv[]);

/**
 * Dump the parsed command line.
 */
void dump_cmdline(const struct cmdline *cmdline, bool verbose);

/**
 * Evaluate the command line.
 */
int eval_cmdline(const struct cmdline *cmdline);

/**
 * Free the parsed command line.
 */
void free_cmdline(struct cmdline *cmdline);

// Predicate evaluation functions
bool eval_true(const struct expr *expr, struct eval_state *state);
bool eval_false(const struct expr *expr, struct eval_state *state);

bool eval_access(const struct expr *expr, struct eval_state *state);
bool eval_perm(const struct expr *expr, struct eval_state *state);

bool eval_acmtime(const struct expr *expr, struct eval_state *state);
bool eval_acnewer(const struct expr *expr, struct eval_state *state);
bool eval_used(const struct expr *expr, struct eval_state *state);

bool eval_gid(const struct expr *expr, struct eval_state *state);
bool eval_uid(const struct expr *expr, struct eval_state *state);
bool eval_nogroup(const struct expr *expr, struct eval_state *state);
bool eval_nouser(const struct expr *expr, struct eval_state *state);

bool eval_depth(const struct expr *expr, struct eval_state *state);
bool eval_empty(const struct expr *expr, struct eval_state *state);
bool eval_fstype(const struct expr *expr, struct eval_state *state);
bool eval_hidden(const struct expr *expr, struct eval_state *state);
bool eval_inum(const struct expr *expr, struct eval_state *state);
bool eval_links(const struct expr *expr, struct eval_state *state);
bool eval_samefile(const struct expr *expr, struct eval_state *state);
bool eval_size(const struct expr *expr, struct eval_state *state);
bool eval_sparse(const struct expr *expr, struct eval_state *state);
bool eval_type(const struct expr *expr, struct eval_state *state);
bool eval_xtype(const struct expr *expr, struct eval_state *state);

bool eval_lname(const struct expr *expr, struct eval_state *state);
bool eval_name(const struct expr *expr, struct eval_state *state);
bool eval_path(const struct expr *expr, struct eval_state *state);
bool eval_regex(const struct expr *expr, struct eval_state *state);

bool eval_delete(const struct expr *expr, struct eval_state *state);
bool eval_exec(const struct expr *expr, struct eval_state *state);
bool eval_exit(const struct expr *expr, struct eval_state *state);
bool eval_nohidden(const struct expr *expr, struct eval_state *state);
bool eval_fls(const struct expr *expr, struct eval_state *state);
bool eval_fprint(const struct expr *expr, struct eval_state *state);
bool eval_fprint0(const struct expr *expr, struct eval_state *state);
bool eval_fprintf(const struct expr *expr, struct eval_state *state);
bool eval_fprintx(const struct expr *expr, struct eval_state *state);
bool eval_prune(const struct expr *expr, struct eval_state *state);
bool eval_quit(const struct expr *expr, struct eval_state *state);

// Operator evaluation functions
bool eval_not(const struct expr *expr, struct eval_state *state);
bool eval_and(const struct expr *expr, struct eval_state *state);
bool eval_or(const struct expr *expr, struct eval_state *state);
bool eval_comma(const struct expr *expr, struct eval_state *state);

#endif // BFS_H
