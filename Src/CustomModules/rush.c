/*
 * rush.c - embed mruby
 *
 */

#include "rush.mdh"
#include "rush.pro"

#include "mruby.h"
#include "mruby/string.h"
#include "mruby/compile.h"

#define BUF_SIZE 256

static mrb_state *mrb;
static mrb_state *mrb_cxt;

static char*
read_stdin()
{
    fflush(stdin);

    char buffer[BUF_SIZE];
    size_t contentSize = 1; // includes NULL
    /* Preallocate space.  We could just allocate one char here, 
       but that wouldn't be efficient. */
    char *content = zalloc(sizeof(char) * BUF_SIZE);
    if(content == NULL)
    {
        perror("Failed to allocate content");
        return NULL;
    }
    content[0] = '\0'; // make null-terminated
    while(fgets(buffer, BUF_SIZE, stdin))
    {
        printf("%s\n", buffer);
        char *old = content;
        contentSize += strlen(buffer);
        content = zrealloc(content, contentSize);
        if(content == NULL)
        {
            perror("Failed to reallocate content");
            zfree(old, 0);
            return NULL;
        }
        strcat(content, buffer);
    }

    if(ferror(stdin))
    {
        zfree(content, 0);
        perror("Error reading from stdin.");
        return NULL;
    }
    printf("stdin = %s\n", content);
    return content;
}

static char*
concat_str_array(char **array)
{
    char **str = array;
    size_t input_size = 0;
    char *input;

    for (; *str; str++) {
        input_size += strlen(*str) + 1;
        printf(".");
    }

    input = (char *)zalloc(input_size);
    str = array;

    input_size = 0;
    for (; *str; str++) {
        strcpy(input + input_size, *str);
        input_size += strlen(*str) + 1;
        input[input_size - 1] = ' ';
    }
    input[input_size - 1] = '\0';
    return input;
}

static int
bin_zrush_reload()
{
    if (mrb_cxt) mrbc_context_free(mrb, mrb_cxt);
    if (mrb) mrb_close(mrb);
    mrb_cxt = mrbc_context_new(mrb);
    mrb = mrb_open();
    printf("Reloaded mruby.\n");
    return !(mrb);
}

static int
bin_zrush(char *nam, char **argv, Options ops, UNUSED(int func))
{
    char **args = argv;
    char *input;

    if (*argv) {
        input = concat_str_array(argv);
    } else {
        input = read_stdin();
    }

    printf("input = %s\n", input);

    mrb->exc = 0;
    if (OPT_ISSET(ops, 'v'))
        printf("Evaluating: `%s'\n", input);
    mrb_value result = mrb_load_string_cxt(mrb, input, mrb_cxt);

    if (input) zfree(input, 0);

    if (mrb->exc) {
        if (!OPT_ISSET(ops, 'q'))
            mrb_p(mrb, mrb_obj_value(mrb->exc));
        mrb->exc = 0;
        return 1;
    }

    if (!OPT_ISSET(ops, 'n'))
        mrb_p(mrb, result);

    errno = 0;
    if(errno != 0) {
        zwarnnam(nam, "%s: %e", *argv, errno);
        return 1;
    }
    return 0;
}

/* module paraphernalia */

static struct builtin bintab[] = {
    BUILTIN("mrbsh", 0, bin_zrush, 0, -1, 0, "qnv", NULL),
    BUILTIN("mrbreload", 0, bin_zrush_reload, 0, 0, 0, NULL, NULL),
};

static struct features module_features = {
    bintab, sizeof(bintab)/sizeof(*bintab),
    NULL, 0,
    NULL, 0,
    NULL, 0,
    0
};

/**/
int
setup_(Module m)
{
    mrb = mrb_open();
    mrb_cxt = mrbc_context_new(mrb);
    /*printf("Setup complete\n");*/
    return 0;
}

/**/
int
features_(Module m, char ***features)
{
    *features = featuresarray(m, &module_features);
    return 0;
}

/**/
int
enables_(Module m, int **enables)
{
    return handlefeatures(m, &module_features, enables);
}

/**/
int
boot_(UNUSED(Module m))
{
    return 0;
}

/**/
int
cleanup_(Module m)
{
    return setfeatureenables(m, &module_features, NULL);
}

/**/
int
finish_(UNUSED(Module m))
{
    if (mrb_cxt) mrbc_context_free(mrb, mrb_cxt);
    if (mrb) mrb_close(mrb);
    /*printf("Finish complete\n");*/
    return 0;
}

