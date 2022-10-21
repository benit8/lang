#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "std.h"
#include "vm.h"

static const char* g_short_options = "d";
static const struct option g_long_options[] = {
	{"debug", no_argument, NULL, 'd'},
	{NULL, 0, NULL, 0}
};

static array_t* make_argv(vm_t* vm)
{
	array_t* args = new_array(vm);
	for (char** arg = vm->arguments; *arg != NULL; ++arg) {
		value_t val = VALUE_OBJECT(new_string(vm, *arg));
		buffer_push(&args->values, &val);
	}
	return args;
}

static table_t* make_env(vm_t* vm)
{
	table_t* env = new_table(vm);
	for (char** e = vm->environment; *e != NULL; ++e) {
		value_t key = VALUE_OBJECT(new_string(vm, strtok(*e, "=")));
		value_t value = VALUE_OBJECT(new_string(vm, strtok(NULL, "")));
		table_set(env, key, value);
	}
	return env;
}

static char* read_file(const char* filename)
{
	int fd = open(filename, O_RDONLY);
	struct stat s;
	fstat(fd, &s);

	char* contents = malloc(s.st_size + 1);
	read(fd, contents, s.st_size);
	close(fd);

	contents[s.st_size] = '\0';
	return contents;
}

static void do_file(vm_t* vm)
{
	char* source = read_file(vm->arguments[0]);
	if (!source) {
		perror(vm->arguments[0]);
		return;
	}

	value_t res = vm_compile(vm, source, vm->arguments[0]);
	free(source);

	if (vm->debug) value_dump(res);
	if (res == VALUE_NULL) return;

	vm_interpret(vm, res, 0);

	value_t main = vm_pop(vm);
	vm->stack.size = 0;

	if (AS_FUNCTION(main)->arity >= 1) {
		vm_push(vm, VALUE_OBJECT(make_argv(vm)));

		if (AS_FUNCTION(main)->arity >= 2) {
			vm_push(vm, VALUE_OBJECT(make_env(vm)));
		}
	}

	vm_interpret(vm, main, AS_FUNCTION(main)->arity);
}

static bool parse_options(vm_t *vm, int argc, char **argv)
{
	int opt = -1;
	while ((opt = getopt_long(argc, argv, g_short_options, g_long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			vm->debug = true;
			break;
		default:
			fprintf(stderr, "Unknown option %c (%d)\n", opt, opt);
			break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "usage: %s [-d|--debug] <entry-point> -- [arguments...]\n", argv[0]);
		return false;
	}

	vm->arguments = argv + optind;
	return true;
}

static void error(const char* msg)
{
	fprintf(stderr, "lang error: %s\n", msg);
}

int main(int argc, char** argv, char** environment)
{
	vm_t *vm = vm_open(environment, &error);
	vm_std_all(vm);

	if (!parse_options(vm, argc, argv)) {
		vm_destroy(vm);
		return EXIT_FAILURE;
	}

	do_file(vm);

	vm_destroy(vm);
	return EXIT_SUCCESS;
}
