#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static int run_cmd_redirect_stdout(const char *const argv[], const char *stdout_path) {
    pid_t pid = fork();
    if (pid < 0) die("fork");

    if (pid == 0) {
        if (stdout_path) {
            int fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) _exit(127);
            if (dup2(fd, STDOUT_FILENO) < 0) _exit(127);
            close(fd);
        }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) die("waitpid");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static const char *pick_compiler_path(void) {
    const char *env = getenv("BASM_CC");
    if (env && env[0]) return env;

    // Prefer repo-local compiler if it exists.
    if (file_exists("./build/basm")) return "./build/basm";

    // Fallback to installed compiler name.
    return "basm-cc";
}

int main(int argc, char **argv) {
    // Behavior:
    //   - If user passes "-o ..." => forward to compiler (asm generation mode)
    //   - Else => compile to temp asm, assemble+link, then run

    if (argc < 2) {
        fprintf(stderr, "usage: basm <input.b> [-o <output.asm>]\n");
        return 1;
    }

    const char *compiler = pick_compiler_path();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            // Forward: basm-cc <args>
            char **cc_argv = calloc((size_t)argc + 1, sizeof(char *));
            if (!cc_argv) die("calloc");
            cc_argv[0] = (char *)compiler;
            for (int j = 1; j < argc; j++) cc_argv[j] = argv[j];
            cc_argv[argc] = NULL;
            execvp(cc_argv[0], cc_argv);
            die("execvp(basm-cc)");
        }
    }

    const char *input = argv[1];

    char asm_tmpl[] = "/tmp/basm-XXXXXX.asm";
    int asm_fd = mkstemps(asm_tmpl, 4);
    if (asm_fd < 0) die("mkstemps");
    close(asm_fd);

    char obj_path[512];
    char bin_path[512];
    snprintf(obj_path, sizeof(obj_path), "%.*s.o", (int)strlen(asm_tmpl) - 4, asm_tmpl);
    snprintf(bin_path, sizeof(bin_path), "%.*s", (int)strlen(asm_tmpl) - 4, asm_tmpl);

    // 1) basm-cc input -o temp.asm  (suppress stdout because compiler may also print asm)
    const char *cc_args[] = {compiler, input, "-o", asm_tmpl, NULL};
    int rc = run_cmd_redirect_stdout(cc_args, "/dev/null");
    if (rc != 0) return rc;

    // 2) nasm -f elf64 temp.asm -o temp.o
    const char *nasm_args[] = {"nasm", "-f", "elf64", asm_tmpl, "-o", obj_path, NULL};
    rc = run_cmd_redirect_stdout(nasm_args, NULL);
    if (rc != 0) return rc;

    // 3) ld temp.o -o temp.bin
    const char *ld_args[] = {"ld", obj_path, "-o", bin_path, NULL};
    rc = run_cmd_redirect_stdout(ld_args, NULL);
    if (rc != 0) return rc;

    // 4) run temp.bin (as a child, so we can cleanup)
    const char *run_args[] = {bin_path, NULL};
    rc = run_cmd_redirect_stdout(run_args, NULL);

    unlink(asm_tmpl);
    unlink(obj_path);
    unlink(bin_path);

    return rc;
}
