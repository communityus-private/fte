#include "qcc.h"
#include "gui.h"

//common gui things

pbool fl_nondfltopts;
pbool fl_hexen2;
pbool fl_ftetarg;
pbool fl_compileonstart;
pbool fl_showall;
pbool fl_log;
pbool fl_extramargins;

char parameters[16384];
char progssrcname[256];
char progssrcdir[256];
char enginebinary[MAX_PATH];
char enginebasedir[MAX_PATH];
char enginecommandline[8192];

int qccpersisthunk = 1;
int Grep(char *filename, char *string)
{
	int foundcount = 0;
	char *last, *found, *linestart;
	int line = 1;
	int sz;
	char *buf;
	if (!filename)
		return foundcount;
	sz = QCC_RawFileSize(filename);
	if (sz <= 0)
		return foundcount;
	buf = malloc(sz+1);
	buf[sz] = 0;
	QCC_ReadFile(filename, buf, sz, NULL);

	linestart = last = found = buf;
	while ((found = strstr(found, string)))
	{
		while (last < found)
		{
			if (*last++ == '\n')
			{
				line++;
				linestart = last;
			}
		}

		while (*found && *found != '\n')
			found++;
		if (*found)
			*found++ = '\0';

		GUIprintf("%s:%i: %s\n", filename, line, linestart);
		line++;
		linestart = found;
		foundcount++;
	}
	free(buf);

	return foundcount;
}
void GoToDefinition(char *name)
{
	QCC_def_t *def;
	QCC_function_t *fnc;

	char *strip;	//trim whitespace (for convieniance).
	while (*name <= ' ' && *name)
		name++;
	for (strip = name + strlen(name)-1; *strip; strip++)
	{
		if (*strip <= ' ')
			*strip = '\0';
		else	//got some part of a word
			break;
	}

	if (!globalstable.numbuckets)
	{
		GUI_DialogPrint("Not found", "You need to compile first.");
		return;
	}


	def = QCC_PR_GetDef(NULL, name, NULL, false, 0, false);

	if (def)
	{
		//with functions, the def is the prototype.
		//we want the body, so zoom to the first statement of the function instead
		if (def->type->type == ev_function && def->constant && !def->arraysize)
		{
			int fnum = def->symboldata[def->ofs].function;
			if (fnum > 0 && fnum < numfunctions)
			{
				fnc = &functions[fnum];
				if (fnc->code>=0 && fnc->s_file)
				{
					EditFile(strings+fnc->s_file, statements[fnc->code].linenum-1, false);
					return;
				}
			}
		}
		if (!def->s_file)
			GUI_DialogPrint("Not found", "Global definition of var was not specified.");
		else
			EditFile(def->s_file+strings, def->s_line-1, false);
	}
	else
		GUI_DialogPrint("Not found", "Global instance of var was not found.");
}

static void GUI_WriteConfigLine(FILE *file, char *part1, char *part2, char *part3, char *desc)
{
	int align = 0;
	if (part1)
	{
		if (strchr(part1, ' '))
			align += fprintf(file, "\"%s\" ", part1);
		else
			align += fprintf(file, "%s ", part1);
		for (; align < 14; align++)
			fputc(' ', file);
	}
	if (part2)
	{
		if (strchr(part2, ' '))
			align += fprintf(file, "\"%s\" ", part2);
		else
			align += fprintf(file, "%s ", part2);
		for (; align < 28; align++)
			fputc(' ', file);
	}
	if (part3)
	{
		if (strchr(part3, ' '))
			align += fprintf(file, "\"%s\" ", part3);
		else
			align += fprintf(file, "%s ", part3);
		for (; align < 40; align++)
			fputc(' ', file);
	}

	if (desc)
	{
		if (align > 40)
		{
			fputc('\n', file);
			align = 0;
		}
		for (; align < 40; align++)
			fputc(' ', file);
		fputs("# ", file);
		align -= 40;
		if (align < 0)
			align = 0;
		while(*desc)
		{
			if (*desc == '\n' || (*desc == ' ' && align > 60))
			{
				fputs("\n", file);
				for (align = 0; align < 40; align++)
					fputc(' ', file);
				fputs("# ", file);
				align = 0;
			}
			else
			{
				fputc(*desc, file);
				align++;
			}
			desc++;
		}
	}
	fputs("\n", file);
}
void GUI_SaveConfig(void)
{
	FILE *file = fopen("fteqcc.ini", "wt");
	int p;
	if (!file)
		return;
	for (p = 0; optimisations[p].enabled; p++)
	{
		if ((!(optimisations[p].flags&FLAG_SETINGUI)) == (!(optimisations[p].flags&FLAG_ASDEFAULT)))
			GUI_WriteConfigLine(file, "optimisation",	optimisations[p].abbrev,	"default",		optimisations[p].description);
		else
			GUI_WriteConfigLine(file, "optimisation",	optimisations[p].abbrev,	(optimisations[p].flags&FLAG_SETINGUI)?"on":"off",		optimisations[p].description);
	}

	for (p = 0; compiler_flag[p].enabled; p++)
	{
		if (!strncmp(compiler_flag[p].fullname, "Keyword: ", 9))
			GUI_WriteConfigLine(file, "keyword",	compiler_flag[p].abbrev,	(compiler_flag[p].flags&FLAG_SETINGUI)?"true":"false",	compiler_flag[p].description);
		else
			GUI_WriteConfigLine(file, "flag",		compiler_flag[p].abbrev,	(compiler_flag[p].flags&FLAG_SETINGUI)?"true":"false",	compiler_flag[p].description);
	}

	GUI_WriteConfigLine(file, "showall",			fl_showall?"on":"off",			NULL, "Show all keyword options in the gui");
	GUI_WriteConfigLine(file, "compileonstart",		fl_compileonstart?"on":"off",	NULL, "Recompile on GUI startup");
	GUI_WriteConfigLine(file, "log",				fl_log?"on":"off",				NULL, "Write out a compile log");

	GUI_WriteConfigLine(file, "enginebinary",		enginebinary,					NULL, "Location of the engine binary to run. Change this to something else to run a different engine, but not all support debugging.");
	GUI_WriteConfigLine(file, "basedir",			enginebasedir,					NULL, "The base directory of the game that contains your sub directory");
	GUI_WriteConfigLine(file, "engineargs",			enginecommandline,				NULL, "The engine commandline to use when debugging. You'll likely want to ensure this contains -window as well as the appropriate -game argument.");
	GUI_WriteConfigLine(file, "srcfile",			progssrcname,					NULL, "The progs.src file to load to find ordering of other qc files.");
	GUI_WriteConfigLine(file, "src",				progssrcdir,					NULL, "Additional subdir to read qc files from. Typically blank (ie: the working directory).");

	GUI_WriteConfigLine(file, "extramargins",		fl_extramargins?"on":"off",		NULL, "Enables line number and folding margins.");
	GUI_WriteConfigLine(file, "hexen2",				fl_hexen2?"on":"off",			NULL, "Enable the extra tweaks needed for compatibility with hexen2 engines.");
	GUI_WriteConfigLine(file, "extendedopcodes",	fl_ftetarg?"on":"off",			NULL, "Utilise an extended instruction set, providing support for pointers and faster arrays and other speedups.");

	GUI_WriteConfigLine(file, "parameters",			parameters,						NULL, "Other additional parameters that are not supported by the gui. Likely including -DFOO");

	fclose(file);
}
//grabs a token. modifies original string.
static char *GUI_ParseInPlace(char **state)
{
	char *str = *state, *end;
	while(*str == ' ' || *str == '\t')
		str++;
	if (*str == '\"')
	{
		char *fmt;
		str++;
		for (end = str, fmt = str; *end; )
		{
			if (*end == '\"')
				break;
			else if (*end == '\'' && end[1] == '\\')
				*fmt = '\\';
			else if (*end == '\'' && end[1] == '\"')
				*fmt = '\"';
			else if (*end == '\'' && end[1] == '\n')
				*fmt = '\n';
			else if (*end == '\'' && end[1] == '\r')
				*fmt = '\r';
			else if (*end == '\'' && end[1] == '\t')
				*fmt = '\t';
			else
			{
				*fmt++ = *end++;
				continue;
			}
			fmt+=1;
			end+=2;
		}
	}
	else
		for (end = str; *end&&*end!=' '&&*end !='\t' && *end != '#'; end++)
			;
	*end = 0;
	*state = end+1;
	return str;
}
static int GUI_ParseBooleanInPlace(char **state, int defaultval)
{
	char *token = GUI_ParseInPlace(state);
	if (!stricmp(token, "default"))
		return defaultval;
	else if (!stricmp(token, "on") || !stricmp(token, "true") || !stricmp(token, "yes"))
		return 1;
	else if (!stricmp(token, "off") || !stricmp(token, "false") || !stricmp(token, "no"))
		return 0;
	else
		return !!atoi(token);
}
void GUI_LoadConfig(void)
{
	char buf[2048];
	char *token, *str;
	FILE *file = fopen("fteqcc.ini", "rb");
	int p;
	if (!file)
		return;
	fl_nondfltopts = false;
	while (fgets(buf, sizeof(buf), file))
	{
		str = buf;
		token = GUI_ParseInPlace(&str);
		if (!stricmp(token, "optimisation") || !stricmp(token, "opt"))
		{
			char *item = GUI_ParseInPlace(&str);
			int value = GUI_ParseBooleanInPlace(&str, -1);
			for (p = 0; optimisations[p].enabled; p++)
				if (!stricmp(item, optimisations[p].abbrev))
				{
					if (value == -1)
						value = !!(optimisations[p].flags & FLAG_ASDEFAULT);
					else
						fl_nondfltopts = true;
					if (value)
						optimisations[p].flags |= FLAG_SETINGUI;
					else
						optimisations[p].flags &= ~FLAG_SETINGUI;
					break;
				}
			//don't worry if its not known	
		}
		else if (!stricmp(token, "flag") || !stricmp(token, "fl") || !stricmp(token, "keyword"))
		{
			char *item = GUI_ParseInPlace(&str);
			int value = GUI_ParseBooleanInPlace(&str, -1);
			for (p = 0; compiler_flag[p].enabled; p++)
				if (!stricmp(item, compiler_flag[p].abbrev))
				{
					if (value == -1)
						value = !!(compiler_flag[p].flags & FLAG_ASDEFAULT);
					if (value)
						compiler_flag[p].flags |= FLAG_SETINGUI;
					else
						compiler_flag[p].flags &= ~FLAG_SETINGUI;
					break;
				}
			//don't worry if its not known
		}
		else if (!stricmp(token, "enginebinary"))
			QC_strlcpy(enginebinary, GUI_ParseInPlace(&str), sizeof(enginebinary));
		else if (!stricmp(token, "basedir"))
			QC_strlcpy(enginebasedir, GUI_ParseInPlace(&str), sizeof(enginebasedir));
		else if (!stricmp(token, "engineargs"))
			QC_strlcpy(enginecommandline, GUI_ParseInPlace(&str), sizeof(enginecommandline));
		else if (!stricmp(token, "srcfile"))
			QC_strlcpy(progssrcname, GUI_ParseInPlace(&str), sizeof(progssrcname));
		else if (!stricmp(token, "src"))
			QC_strlcpy(progssrcdir, GUI_ParseInPlace(&str), sizeof(progssrcdir));
		else if (!stricmp(token, "parameters"))
			QC_strlcpy(parameters, GUI_ParseInPlace(&str), sizeof(parameters));

		else if (!stricmp(token, "log"))
			fl_log = GUI_ParseBooleanInPlace(&str, false);
		else if (!stricmp(token, "compileonstart"))
			fl_compileonstart = GUI_ParseBooleanInPlace(&str, false);
		else if (!stricmp(token, "showall"))
			fl_showall = GUI_ParseBooleanInPlace(&str, false);

		else if (!stricmp(token, "extramargins"))
			fl_extramargins = GUI_ParseBooleanInPlace(&str, false);
		else if (!stricmp(token, "hexen2"))
			fl_hexen2 = GUI_ParseBooleanInPlace(&str, false);
		else if (!stricmp(token, "extendedopcodes"))
			fl_ftetarg = GUI_ParseBooleanInPlace(&str, false);
		else if (*token)
		{
			puts("Unknown setting: "); puts(token); puts("\n");
		}
	}
	fclose(file);
}


//this function takes the windows specified commandline and strips out all the options menu items.
void GUI_ParseCommandLine(char *args)
{
	int paramlen=0;
	int l, p;
	char *next;

	//find the first argument
	while (*args == ' ' || *args == '\t')
		args++;
	for (next = args; *next&&*next!=' '&&*next !='\t'; next++)
		;

	if (*args != '-')
	{
		pbool qt = *args == '\"';
		l = 0;
		if (qt)
			args++;
		while ((*args != ' ' || qt) && *args)
		{
			if (qt && *args == '\"')
			{
				args++;
				break;
			}
			progssrcname[l++] = *args++;
		}
		progssrcname[l] = 0;

		next = args;

		args = strrchr(progssrcname, '\\');
		while(args && strchr(args, '/'))
			args = strchr(args, '/');
		if (args)
		{
			memcpy(progssrcdir, progssrcname, args-progssrcname);
			progssrcdir[args-progssrcname] = 0;
			args++;
			memmove(progssrcname, args, strlen(args)+1);

			SetCurrentDirectoryA(progssrcdir);
			*progssrcdir = 0;
		}
		args = next;
	}

	GUI_LoadConfig();

	while(*args)
	{
		while (*args == ' ' || *args == '\t')
			args++;
		for (next = args; *next&&*next!=' '&&*next !='\t'; next++)
			;

		strncpy(parameters+paramlen, args, next-args);
		parameters[paramlen+next-args] = '\0';
		l = strlen(parameters+paramlen)+1;

		if (!strnicmp(parameters+paramlen, "-O", 2) || !strnicmp(parameters+paramlen, "/O", 2))
		{	//strip out all -O
			fl_nondfltopts = true;
			if (parameters[paramlen+2])
			{
				if (parameters[paramlen+2] >= '0' && parameters[paramlen+2] <= '3')
				{
					p = parameters[paramlen+2]-'0';
					for (l = 0; optimisations[l].enabled; l++)
					{
						if (optimisations[l].optimisationlevel<=p)
							optimisations[l].flags |= FLAG_SETINGUI;
						else
							optimisations[l].flags &= ~FLAG_SETINGUI;
					}
				}
				else if (!strncmp(parameters+paramlen+2, "no-", 3))
				{
					if (parameters[paramlen+5])
					{
						for (p = 0; optimisations[p].enabled; p++)
							if ((*optimisations[p].abbrev && !strcmp(parameters+paramlen+5, optimisations[p].abbrev)) || !strcmp(parameters+paramlen+5, optimisations[p].fullname))
							{
								optimisations[p].flags &= ~FLAG_SETINGUI;
								break;
							}

						if (!optimisations[p].enabled)
						{
							parameters[paramlen+next-args] = ' ';
							paramlen += l;
						}
					}
				}
				else
				{
					for (p = 0; optimisations[p].enabled; p++)
						if ((*optimisations[p].abbrev && !strcmp(parameters+paramlen+2, optimisations[p].abbrev)) || !strcmp(parameters+paramlen+2, optimisations[p].fullname))
						{
							optimisations[p].flags |= FLAG_SETINGUI;
							break;
						}

					if (!optimisations[p].enabled)
					{
						parameters[paramlen+next-args] = ' ';
						paramlen += l;
					}
				}
			}
		}
		else if (!strnicmp(parameters+paramlen, "-F", 2) || !strnicmp(parameters+paramlen, "/F", 2) || !strnicmp(parameters+paramlen, "-K", 2) || !strnicmp(parameters+paramlen, "/K", 2))
		{
			if (parameters[paramlen+2])
			{
				if (!strncmp(parameters+paramlen+2, "no-", 3))
				{
					if (parameters[paramlen+5])
					{
						for (p = 0; compiler_flag[p].enabled; p++)
							if ((*compiler_flag[p].abbrev && !strcmp(parameters+paramlen+5, compiler_flag[p].abbrev)) || !strcmp(parameters+paramlen+5, compiler_flag[p].fullname))
							{
								compiler_flag[p].flags &= ~FLAG_SETINGUI;
								break;
							}

						if (!compiler_flag[p].enabled)
						{
							parameters[paramlen+next-args] = ' ';
							paramlen += l;
						}
					}
				}
				else
				{
					for (p = 0; compiler_flag[p].enabled; p++)
						if ((*compiler_flag[p].abbrev && !strcmp(parameters+paramlen+2, compiler_flag[p].abbrev)) || !strcmp(parameters+paramlen+2, compiler_flag[p].fullname))
						{
							compiler_flag[p].flags |= FLAG_SETINGUI;
							break;
						}

					if (!compiler_flag[p].enabled)
					{
						parameters[paramlen+next-args] = ' ';
						paramlen += l;
					}
				}
			}
		}

/*
		else if (!strnicmp(parameters+paramlen, "-Fno-kce", 8) || !strnicmp(parameters+paramlen, "/Fno-kce", 8))	//keywords stuph
		{
			fl_nokeywords_coexist = true;
		}
		else if (!strnicmp(parameters+paramlen, "-Fkce", 5) || !strnicmp(parameters+paramlen, "/Fkce", 5))
		{
			fl_nokeywords_coexist = false;
		}
		else if (!strnicmp(parameters+paramlen, "-Facc", 5) || !strnicmp(parameters+paramlen, "/Facc", 5))
		{
			fl_acc = true;
		}
		else if (!strnicmp(parameters+paramlen, "-autoproto", 10) || !strnicmp(parameters+paramlen, "/autoproto", 10))
		{
			fl_autoprototype = true;
		}
*/
		else if (!strnicmp(parameters+paramlen, "-showall", 8) || !strnicmp(parameters+paramlen, "/showall", 8))
		{
			fl_showall = true;
		}
		else if (!strnicmp(parameters+paramlen, "-ac", 3) || !strnicmp(parameters+paramlen, "/ac", 3))
		{
			fl_compileonstart = true;
		}
		else if (!strnicmp(parameters+paramlen, "-log", 4) || !strnicmp(parameters+paramlen, "/log", 4))
		{
			fl_log = true;
		}
		else if (!strnicmp(parameters+paramlen, "-engine", 7) || !strnicmp(parameters+paramlen, "/engine", 7))
		{
			while (*next == ' ')
				next++;
			
			l = 0;
			while (*next != ' ' && *next)
				enginebinary[l++] = *next++;
			enginebinary[l] = 0;
		}
		else if (!strnicmp(parameters+paramlen, "-basedir", 8) || !strnicmp(parameters+paramlen, "/basedir", 8))
		{
			while (*next == ' ')
				next++;
			
			l = 0;
			while (*next != ' ' && *next)
				enginebasedir[l++] = *next++;
			enginebasedir[l] = 0;
		}
		//strcpy(enginecommandline, "-window +map start -nohome");
		else if (!strnicmp(parameters+paramlen, "-srcfile", 8) || !strnicmp(parameters+paramlen, "/srcfile", 8))
		{
			while (*next == ' ')
				next++;
			
			l = 0;
			while (*next != ' ' && *next)
				progssrcname[l++] = *next++;
			progssrcname[l] = 0;
		}
		else if (!strnicmp(parameters+paramlen, "-src ", 5) || !strnicmp(parameters+paramlen, "/src ", 5))
		{
			while (*next == ' ')
				next++;
			
			l = 0;
			while (*next != ' ' && *next)
				progssrcdir[l++] = *next++;
			progssrcdir[l] = 0;
		}
		else if (!strnicmp(parameters+paramlen, "-T", 2) || !strnicmp(parameters+paramlen, "/T", 2))	//the target
		{
			if (!strnicmp(parameters+paramlen+2, "h2", 2))
			{
				fl_hexen2 = true;
			}
			else
			{
				fl_hexen2 = false;
				parameters[paramlen+next-args] = ' ';
				paramlen += l;
			}
		}
		/*
		else if (isfirst && *args != '-' && *args != '/')
		{
			pbool qt = *args == '\"';
			l = 0;
			if (qt)
				args++;
			while (*args != ' ' && *args)
			{
				if (qt && *args == '\"')
				{
					args++;
					break;
				}
				progssrcname[l++] = *args++;
			}
			progssrcname[l] = 0;

			args = strrchr(progssrcname, '\\');
			while(args && strchr(args, '/'))
				args = strchr(args, '/');
			if (args)
			{
				memcpy(progssrcdir, progssrcname, args-progssrcname);
				progssrcdir[args-progssrcname] = 0;
				args++;
				memmove(progssrcname, args, strlen(args)+1);

				SetCurrentDirectoryA(progssrcdir);
				*progssrcdir = 0;
			}
		}
		*/
		else
		{
			parameters[paramlen+next-args] = ' ';
			paramlen += l;
		}

		args=next;
	}
	if (paramlen)
		parameters[paramlen-1] = '\0';
	else
		*parameters = '\0';
}

void GUI_SetDefaultOpts(void)
{
	int i;
	for (i = 0; compiler_flag[i].enabled; i++)	//enabled is a pointer
	{
		if (compiler_flag[i].flags & FLAG_ASDEFAULT)
			compiler_flag[i].flags |= FLAG_SETINGUI;
		else
			compiler_flag[i].flags &= ~FLAG_SETINGUI;
	}
	for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
	{
		if (optimisations[i].flags & FLAG_ASDEFAULT)
			optimisations[i].flags |= FLAG_SETINGUI;
		else
			optimisations[i].flags &= ~FLAG_SETINGUI;
	}
}

void GUI_RevealOptions(void)
{
	int i;
	for (i = 0; compiler_flag[i].enabled; i++)	//enabled is a pointer
	{
		if (fl_showall && compiler_flag[i].flags & FLAG_HIDDENINGUI)
			compiler_flag[i].flags &= ~FLAG_HIDDENINGUI;
	}
	for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
	{
		if (fl_showall && optimisations[i].flags & FLAG_HIDDENINGUI)
			optimisations[i].flags &= ~FLAG_HIDDENINGUI;

		if (optimisations[i].flags & FLAG_HIDDENINGUI)	//hidden optimisations are disabled as default
			optimisations[i].optimisationlevel = 4;
	}
}




int GUI_BuildParms(char *args, char **argv, pbool quick)
{
	static char param[2048];
	int paramlen = 0;
	int argc;
	char *next;
	int i;
	int targ;
	char *targs[] = {"", "-Th2", "-Tfte", "-Tfteh2"};


	argc = 1;
	argv[0] = "fteqcc";

	if (quick)
	{
		strcpy(param+paramlen, "-Tparse");
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}

	while(*args)
	{
		while (*args <= ' '&& *args)
			args++;

		for (next = args; *next>' '; next++)
			;
		strncpy(param+paramlen, args, next-args);
		param[paramlen+next-args] = '\0';
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;

		args=next;
	}

	targ = 0;
	targ |= fl_hexen2?1:0;
	targ |= fl_ftetarg?2:0;
	if (*targs[targ])
	{
		strcpy(param+paramlen, targs[targ]);
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}

	if (fl_nondfltopts)
	{
		for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
		{
			if (optimisations[i].flags & FLAG_SETINGUI)
				sprintf(param+paramlen, "-O%s", optimisations[i].abbrev);
			else
				sprintf(param+paramlen, "-Ono-%s", optimisations[i].abbrev);
			argv[argc++] = param+paramlen;
			paramlen += strlen(param+paramlen)+1;
		}
	}

	for (i = 0; compiler_flag[i].enabled; i++)	//enabled is a pointer
	{
		if (compiler_flag[i].flags & FLAG_SETINGUI)
			sprintf(param+paramlen, "-F%s", compiler_flag[i].abbrev);
		else
			sprintf(param+paramlen, "-Fno-%s", compiler_flag[i].abbrev);
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}


/*	while(*args)
	{
		while (*args <= ' '&& *args)
			args++;

		for (next = args; *next>' '; next++)
			;
		strncpy(param+paramlen, args, next-args);
		param[paramlen+next-args] = '\0';
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
		args=next;
	}*/

	if (*progssrcname)
	{
		argv[argc++] = "-srcfile";
		argv[argc++] = progssrcname;
	}
	if (*progssrcdir)
	{
		argv[argc++] = "-src";
		argv[argc++] = progssrcdir;
	}

	return argc;
}
