/* Format Rush [1] Typescript type check [2] output to Emacs [3] compatible format.

[1] https://rushjs.io/
[2] https://www.typescriptlang.org/docs/handbook/compiler-options.html
[3] https://www.gnu.org/software/emacs/

Compile and run:
	$ cc -o rush-fmt main.c
	$ cd ~/path_to_rush_project
	$ rush tsc-command | rush-fmt
*/

#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SIZE(array) (sizeof (array) / sizeof (array[0]))

static char project_names[46][4096];	// List of rush project names
static char project_paths[46][4096];	// List of project paths
static unsigned pni, ppi;		// Number of project names and paths

static int project_indexof(char *name) {
	unsigned i;
	for (i=0; i<pni; i++) {
		if (!strcmp(name, project_names[i])) return i;
	}
	return -1;	// Not found
}

static void get_captures(char dst[][4096], char *src, regmatch_t pmatch[], int n) {
	while (n--) {
		sprintf(dst[n], "%.*s",
			(int)(pmatch[n+1].rm_eo - pmatch[n+1].rm_so),
			src + pmatch[n+1].rm_so);
	}
}

/* Parse Rush configuration file defining project_names and project_paths. */
static void parse_cfg(char *path) {
	char buf[4096], str[2][4096];
	FILE *fp;
	regex_t reg;
	regmatch_t pmatch[4];
	regcomp(&reg, "^ *\"(.*)\": *\"(.*)\",?\n$", REG_EXTENDED);
	if (!(fp = fopen(path, "r"))) {
		err(1, "parse_cfg fopen %s", path);
	}
	pni = 0;
	ppi = 0;
	while (fgets(buf, sizeof buf, fp)) {
		if (regexec(&reg, buf, SIZE(pmatch), pmatch, 0)) {
			continue;
		}
		get_captures(str, buf, pmatch, 2);
		if (!strcmp("packageName", str[0])) {
			strcpy(project_names[pni++], str[1]);
			assert(pni < SIZE(project_names));
		}
		if (!strcmp("projectFolder", str[0])) {
			strcpy(project_paths[ppi++], str[1]);
			assert(ppi < SIZE(project_paths));
		}
	}
	if (fclose(fp)) {
		err(1, "parse_cfg fclose %s", path);
	}
	assert(pni == ppi);
}

int main(void) {
	char buf[4096], cwd[4096], root_path[4096]={0}, str[4][4096];
	int pi=-1, errors_count=0;
	regex_t reg_cfg, reg_proj, reg_err;
	regmatch_t pmatch[6];
	//
	regcomp(&reg_cfg, "^Found configuration in (.*)\n$", REG_EXTENDED);
	regcomp(&reg_proj, "^==\\[ (.*) \\]=+\\[ [0-9]+ of [0-9]+ \\]==\n$", REG_EXTENDED);
	regcomp(&reg_err, "^(.*\\.tsx?).([0-9]+).([0-9]+)..*error (.*)\n$", REG_EXTENDED);
	//
	getcwd(cwd, sizeof cwd);
	while (fgets(buf, sizeof buf, stdin)) {
		if (!regexec(&reg_cfg, buf, SIZE(pmatch), pmatch, 0)) {
			get_captures(str, buf, pmatch, 1);
			strcpy(root_path, dirname(str[0]));
			parse_cfg(str[0]);
			pi = -1;	
		}
		if (!regexec(&reg_proj, buf, SIZE(pmatch), pmatch, 0)) {
			get_captures(str, buf, pmatch, 1);
			pi = project_indexof(str[0]);
		}
		if (!regexec(&reg_err, buf, SIZE(pmatch), pmatch, 0)) {
			get_captures(str, buf, pmatch, 4);
			printf("%s/%s%s%s:%d:%d: error:\n\t%s\n",
			       // NOTE(irek): When PI project index is unknown
			       // then use CWD as file path prefix.
			       pi == -1 ? cwd : root_path,
			       pi == -1 ? "" : project_paths[pi],
			       pi == -1 ? "" : "/",
			       str[0],		// File path
			       atoi(str[1]),	// Line
			       atoi(str[2]),	// Column
			       str[3]);		// Error msg
			errors_count++;
			continue;	// Skip default printer
		}
		printf("%s", buf);
	}
	return errors_count;
}
