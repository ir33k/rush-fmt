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

#define SIZE(array) (sizeof (array) / sizeof (array[0]))

static char root_path[4096]={0};	// Root path to project
static char project_names[46][4096];	// List of rush project names
static unsigned pni;			// Number of project names
static char project_paths[46][4096];	// List of project paths
static unsigned ppi;			// Number of project paths

static int project_indexof(char *name) {
	unsigned i;
	for (i=0; i<pni; i++) {
		if (strcmp(name, project_names[i]) == 0) {
			return i;
		}
	}
	return -1;	// Not found
}

/* Parse Rush configuration file defining project_names and project_paths. */
static void parse_cfg(char *path) {
	char buf[4096], *str;
	unsigned len;
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
		str = buf + pmatch[1].rm_so;
		len = pmatch[1].rm_eo - pmatch[1].rm_so;
		str[len] = 0;
		if (strcmp("packageName", str) == 0) {
			sprintf(project_names[pni++], "%.*s",
				(int)(pmatch[2].rm_eo - pmatch[2].rm_so),
				buf + pmatch[2].rm_so);
			assert(pni < SIZE(project_names));
			continue;
		}
		if (strcmp("projectFolder", str) == 0) {
			sprintf(project_paths[ppi++], "%.*s",
				(int)(pmatch[2].rm_eo - pmatch[2].rm_so),
				buf + pmatch[2].rm_so);
			assert(ppi < SIZE(project_paths));
			continue;
		}
	}
	if (fclose(fp)) {
		err(1, "parse_cfg fclose %s", path);
	}
	assert(pni == ppi);
}

int main(void) {
	char buf[4096], str[4][4096];
	int is_type_check=0, pi=-1, errors_count=0;
	regex_t reg_cfg, reg_cmd, reg_project, reg_err, reg_code;
	regmatch_t pmatch[6];
	//
	regcomp(&reg_cfg, "^Found configuration in (.*)\n$", REG_EXTENDED);
	regcomp(&reg_cmd, "^Starting (.*)\n$", REG_EXTENDED);
	regcomp(&reg_project, "^==\\[ (.*) \\]=+\\[ [0-9]+ of [0-9]+ \\]==\n$", REG_EXTENDED);
	regcomp(&reg_err, "^(.*\\.tsx?).([0-9]+).([0-9]+)..*error (.*)\n$", REG_EXTENDED);
	regcomp(&reg_code, "^Returned error code: ([0-9]+)\n$", REG_EXTENDED);
	//
	while (fgets(buf, sizeof buf, stdin)) {
		if (regexec(&reg_cfg, buf, SIZE(pmatch), pmatch, 0) == 0) {
			sprintf(str[0], "%.*s",
				(int)(pmatch[1].rm_eo - pmatch[1].rm_so),
				buf + pmatch[1].rm_so);
			sprintf(root_path, "%s", dirname(str[0]));
			parse_cfg(str[0]);
			printf("%s", buf);
			continue;
		}
		if (regexec(&reg_cmd, buf, SIZE(pmatch), pmatch, 0) == 0) {
			sprintf(str[0], "%.*s",
				(int)(pmatch[1].rm_eo - pmatch[1].rm_so),
				buf + pmatch[1].rm_so);
			is_type_check = strcmp("\"rush type-check\"", str[0]) == 0;
			printf("%s", buf);
			continue;
		}
		// Nothing more to do if this is not a rush type-check command.
		if (!is_type_check) {
			printf("%s", buf);
			continue;
		}
		if (regexec(&reg_project, buf, SIZE(pmatch), pmatch, 0) == 0) {
			sprintf(str[0], "%.*s",
				(int)(pmatch[1].rm_eo - pmatch[1].rm_so),
				buf + pmatch[1].rm_so);
			pi = project_indexof(str[0]);
			assert(pi != -1);
			printf("%s", buf);
			continue;
		}
		if (regexec(&reg_err, buf, SIZE(pmatch), pmatch, 0) == 0) {
			assert(pi != -1);
			sprintf(str[0], "%.*s",
				(int)(pmatch[1].rm_eo - pmatch[1].rm_so),
				buf + pmatch[1].rm_so);
			sprintf(str[1], "%.*s",
				(int)(pmatch[2].rm_eo - pmatch[2].rm_so),
				buf + pmatch[2].rm_so);
			sprintf(str[2], "%.*s",
				(int)(pmatch[3].rm_eo - pmatch[3].rm_so),
				buf + pmatch[3].rm_so);
			sprintf(str[3], "%.*s",
				(int)(pmatch[4].rm_eo - pmatch[4].rm_so),
				buf + pmatch[4].rm_so);
			printf("%s/%s/%s:%d:%d: error:\n\t%s\n",
			       root_path,
			       project_paths[pi],
			       str[0],		// File path
			       atoi(str[1]),	// Line
			       atoi(str[2]),	// Column
			       str[3]);		// Error msg
			errors_count++;
			continue;
		}
		printf("%s", buf);
	}
	return errors_count;
}
