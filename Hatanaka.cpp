#include "stdafx.h"
#include "Hatanaka.h"


CHatanaka::CHatanaka()
{
}


CHatanaka::~CHatanaka()
{
}


HTNKRESULT CHatanaka::compressFile(const std::string& fileName)
{
	return compressFile(fileName, std::string());
}

HTNKRESULT CHatanaka::compressFile(const std::string& inputFileName, const std::string& outputFileName)
{

	if (programmName.size() > 16)
		return HTNKRESULT::ERROR_PROG_NAME_TOO_LONG;

	if (programmVersion.size() > 16)
		return HTNKRESULT::ERROR_PROG_VERSION_TOO_LONG;
	try {
		
		if (!open_rw_files(inputFileName, outputFileName))
			return HTNKRESULT::ERROR_OPEN_RW_FILES;

		/////////
		char newline[MAXCLM];
		char dummy[2] = { '\0','\0' };
		char *p, *p_event, *p_nsat, *p_satlst, *p_satold, *p_clock;
		int sattbl[MAXSAT], i, j, shift_clk;

		for (i = 0; i < UCHAR_MAX; i++)
			ntype_gnss[i] = -1;  /** -1 unless GNSS type is defined **/

		if (!header())
			return HTNKRESULT::ERROR_HEADER;

		if (rinex_version == 2) {
			p_event = &newline[28];  /** pointer to event flag **/
			p_nsat = &newline[29];  /** pointer to n_sat **/
			p_satlst = &newline[32];  /** pointer to satellite list **/
			p_satold = &oldline[32];  /** pointer to n_sat **/
			p_clock = &newline[68];  /** pointer to clock offset data **/
			shift_clk = 1;
		}
		else {
			p_event = &newline[31];
			p_nsat = &newline[32];
			p_satlst = &newline[41];
			p_satold = &oldline[41];
			p_clock = &newline[41];
			shift_clk = 4;
		}

		for (clear_buff();; flush_buff()) {
		SKIP:
			HTNKRESULT res = get_next_epoch(newline);
			
			//todo разобраться как быть с концом файла
			if (res == HTNKRESULT::OK_EOF)
				return HTNKRESULT::OK;

			/*** if event flag > 1, then (1)output event data  */
			/*** (2)initialize all data arcs, and continue to next epoch ***/
			if (atoi(strncpy(dummy, p_event, C1)) > 1)
			{
				HTNKRESULT res = put_event_data(newline);
				if (res != HTNKRESULT::OK)
					return res;
				initialize_all(oldline, &nsat_old, 0);
				continue;
			}

			if (strchr(newline, '\0') > p_clock) {
				if (authenticMode)
				{
					HTNKRESULT res = read_clock(p_clock, shift_clk);        /**** read clock offset ****/
					if (res != HTNKRESULT::OK)
						return res;
				}
				else
					clk_order = -1;
			}
			else {
				clk_order = -1;                       /*** reset data arc for clock offset ***/
			}

			nsat = atoi(p_nsat);
			if (nsat > MAXSAT)
				return HTNKRESULT::ERROR_SAT_NUBMER;

			if (nsat > 12 && rinex_version == 2)
				read_more_sat(nsat, p_satlst);  /*** read continuation lines ***/

			if (ep_reset > 0 && ++ep_count > ep_reset)
				initialize_all(oldline, &nsat_old, 1);

			/**** get observation ****/
			for (i = 0, p = p_satlst; i < nsat; i++, p += 3)
			{
				if (ggetline(dy1[i], flag[i], p, &ntype_record[i]) != HTNKRESULT::OK)
				{
					clear_buff();
					exit_status = EXIT_WARNING;
					goto SKIP;
				}
			}
			*p = '\0';    /*** terminate satellite list ***/

			if (set_sat_table(p_satlst, p_satold, nsat_old, sattbl) != HTNKRESULT::OK)
			{
				clear_buff();
				exit_status = EXIT_WARNING;
				continue;
			}

			/***********************************************************/
			/**** print change of the line & clock offset difference ****/
			/**** and data difference                               ****/
			/***********************************************************/
			p_buff = strdiff(oldline, newline, p_buff);
			if (clk_order > -1) {
				if (clk_order > 0)
					process_clock();            /**** process clock offset ****/

				put_clock(clk1.u[clk_order], clk1.l[clk_order], clk_order);
			}
			else {
				*p_buff++ = '\n';
			}
			data(sattbl); *p_buff = '\0';
			/**************************************/
			/**** save current epoch to buffer ****/
			/**************************************/
			nsat_old = nsat;
			sprintf(oldline, "%s", newline);
			clk0 = clk1;
			for (i = 0; i < nsat; i++) {
				strcpy(flag0[i], flag[i]);
				for (j = 0; j < ntype_record[i]; j++)
					dy0[i][j] = dy1[i][j];
			}
		}
	}
	catch (std::invalid_argument& e)
	{
		close_rw_files();
	}

	close_rw_files();
	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
void CHatanaka::closeFiles()
{
	close_rw_files();
}
/*---------------------------------------------------------------------*/
void CHatanaka::setProgrammName(std::string programmName_)
{
	programmName = programmName_;
}
/*---------------------------------------------------------------------*/
void CHatanaka::setProgrammVersion(std::string programmVersion_)
{
	programmVersion = programmVersion_;
}
/*---------------------------------------------------------------------*/
void CHatanaka::setAuthenticMode(bool mode)
{
	authenticMode = mode;
}
/*---------------------------------------------------------------------*/
bool CHatanaka::getAuthenticMode()
{
	return authenticMode;
}
/*---------------------------------------------------------------------*/
bool CHatanaka::open_rw_files(std::string inFilename, std::string outFilename)
{
	char *p, infile[MAXCLM], outfile[MAXCLM];
	FILE *ifp;
	
	strncpy(infile, inFilename.data(), C1*MAXCLM);

	if (strlen(infile) == MAXCLM)
		return false;

	/***********************/
	/*** open input file ***/
	/***********************/
	if (outFilename.empty()) //если указаны оба файла, проверки можно опустить 
	{
		p = strrchr(infile, '.');

		if (p == NULL 
			|| *(p + 4) != '\0'
			|| (toupper(*(p + 3)) != 'O'
			&& strcmp(p + 1, "RNX") != 0
			&& strcmp(p + 1, "rnx") != 0)
			) return false;

	}

	if ((ifp = fopen(infile, "r")) == NULL)
		return false;

	/************************/
	/*** open output file ***/
	/************************/

	if (outFilename.empty())
	{
		strcpy(outfile, infile);
		p = strrchr(outfile, '.');
		//изменение имени файла
		if (*(p + 3) == 'o')
			*(p + 3) = 'd';
		else if (*(p + 3) == 'O')
			*(p + 3) = 'D';
		else if (strcmp(p + 1, "rnx") == 0)
			strcpy((p + 1), "crx");
		else if (strcmp(p + 1, "RNX") == 0)
			strcpy((p + 1), "CRX");
	}
	else
	{
		strncpy(outfile, outFilename.data(), C1*MAXCLM);
		if (strlen(outfile) == MAXCLM)
			return false;
	}
	
	freopen(outfile, "w", stdout);
	fclose(ifp);
	freopen(infile, "r", stdin);

	return true;
}
/*---------------------------------------------------------------------*/
void CHatanaka::close_rw_files()
{
	fclose(stdout);
	fclose(stdin);
}
/*---------------------------------------------------------------------*/
bool CHatanaka::header(void) {
	char line[MAXCLM], line2[41], timestring[20];
	time_t tc = time(NULL);
	struct tm *tp;

	if ((tp = gmtime(&tc)) == NULL)
		tp = localtime(&tc);

	strftime(timestring, C1 * 20, "%d-%b-%y %H:%M", tp);

	/*** Check RINEX VERSION / TYPE ***/
	read_chk_line(line);
	if (strncmp(&line[60], "RINEX VERSION / TYPE", C1 * 20) != 0
		|| strncmp(&line[20], "O", C1) != 0)
		return false;

	rinex_version = atoi(line);

	if (rinex_version == 2)
		printf("%-20.20s", CRX_VERSION1);
	else if (rinex_version == 3)
		printf("%-20.20s", CRX_VERSION2);
	else
		return false;
	
	printf("%-40.40s%-20.20s\n", "COMPACT RINEX FORMAT", "CRINEX VERS   / TYPE");

	sprintf(line2, "%s %s", programmName.c_str(), programmVersion.c_str());

	printf("%-40.40s%-20.20sCRINEX PROG / DATE\n", line2, timestring);
	printf("%s\n", line);

	do {
		read_chk_line(line);
		printf("%s\n", line);
		if (strncmp(&line[60], "# / TYPES OF OBSERV", C1 * 19) == 0 && line[5] != ' ') 
		{ /** for RINEX2 **/
			ntype = atoi(line);                                       
		}
		else if (strncmp(&line[60], "SYS / # / OBS TYPES", C1 * 19) == 0) 
		{ /** for RINEX3 **/
			if (line[0] != ' ') 
				ntype_gnss[(unsigned int)line[0]] = atoi(&line[3]);

			if (ntype_gnss[(unsigned int)line[0]] > MAXTYPE) 
				return false;
		}
	} while (strncmp(&line[60], "END OF HEADER", C1 * 13) != 0);

	return true;
}
/*---------------------------------------------------------------------*/
HTNKRESULT  CHatanaka::get_next_epoch(char *p_line) {
	/**** find next epoch line.                                                          ****/
	/**** If the line seems to be abnormal, print warning message			             ****/
	/**** and skip until next epoch is found                                             ****/
	/**** return value  0 : end of the file                                              ****/
	/****               1 : normal end                                                   ****/
	/****               2 : trouble in the line                                          ****/
	char *p;

	nl_count++;
	if (fgets(p_line, MAXCLM, stdin) == NULL) 
		return HTNKRESULT::OK_EOF;  /*** EOF: exit program successfully ***/

	if ((p = strchr(p_line, '\n')) == NULL) {
		if (*p_line == '\032') 
			return HTNKRESULT::OK_EOF;              /** DOS EOF **/
		if (*p_line != '\0' || feof(stdin) == 0) {
			if (!skip_strange_epoch) 
				return error_exit(HTNKRESULT::ERROR_LINE_NULL_CHAR, p_line);
			skip_to_next(p_line);
			return HTNKRESULT::ERROR_LINE;
		}
		fprintf(stderr, "WARNING: null characters are detected at the end of file --> neglected.\n");
		exit_status = EXIT_WARNING;
		return HTNKRESULT::OK_EOF;
	}
	if (*(p - 1) == '\r')p--;                      /*** check DOS CR/LF ***/

	if (rinex_version == 2) {
		while (*--p == ' ' && p > p_line) {}; *++p = '\0';   /*** chop blank ***/
		if (strlen(p_line) < 29 || *p_line != ' '
			|| *(p_line + 27) != ' ' || !isdigit(*(p_line + 28))
			|| (*(p_line + 29) != ' ' && *(p_line + 29) != '\0')) {
			/**** ------- something strange is found in the epoch line ****/
			if (!skip_strange_epoch) 
				return error_exit(HTNKRESULT::ERROR_READ_LINE, p_line);
			if (*(p_line + 18) != '.') clear_buff();
			skip_to_next(p_line);
			return HTNKRESULT::ERROR_LINE;
		}
	}
	else {
		if (*p_line != '>') {
			if (!skip_strange_epoch) 
				return error_exit(HTNKRESULT::ERROR_READ_LINE, p_line);
			clear_buff();
			skip_to_next(p_line);
			return HTNKRESULT::ERROR_LINE;
		}
		while (p < (p_line + 41)) *p++ = ' '; /*** pad blank ***/
		*p = '\0';
	}
	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
void CHatanaka::skip_to_next(char *p_line) {
	fprintf(stderr, " WARNING at line %ld: strange format. skip to next epoch.\n", nl_count);
	exit_status = EXIT_WARNING;

	if (rinex_version == 2) {
		do {                               /**** try to find next epoch line ****/
			read_chk_line(p_line);
		} while (strlen(p_line) < 29 || *p_line != ' ' || *(p_line + 3) != ' '
			|| *(p_line + 6) != ' ' || *(p_line + 9) != ' '
			|| *(p_line + 12) != ' ' || *(p_line + 15) != ' '
			|| *(p_line + 26) != ' ' || *(p_line + 27) != ' '
			|| !isdigit(*(p_line + 28)) || !isspace(*(p_line + 29))
			|| (strlen(p_line) > 68 && *(p_line + 70) != '.'));
	}
	else {    /*** for RINEX3 ***/
		do {
			read_chk_line(p_line);
		} while (*p_line != '>');
	}
	initialize_all(oldline, &nsat_old, 0);             /**** initialize all data ***/
}
/*---------------------------------------------------------------------*/
void CHatanaka::initialize_all(char *oldline, int *nsat_old, int count) {
	strcpy(oldline, "&");        /**** initialize the epoch data arc ****/
	clk_order = -1;             /**** initialize the clock data arc ****/
	*nsat_old = 0;              /**** initialize the all satellite arcs ****/
	ep_count = count;
}
/*---------------------------------------------------------------------*/
HTNKRESULT CHatanaka::put_event_data(char *p_line) {
	/**** This routine is called when event flag >1 is set.  ****/
	/****      read # of event information lines and output  ****/
	int i, n;
	char *p;

	if (rinex_version == 2) {
		if (*(p_line + 26) == '.') 
			return error_exit(HTNKRESULT::ERROR_READ_LINE, p_line);
		printf("&%s\n", (p_line + 1));
		if (strlen(p_line) > 29) {
			n = atoi((p_line + 29));     /** n: number of lines to follow **/
			for (i = 0; i < n; i++) {
				read_chk_line(p_line);
				printf("%s\n", p_line);
				if (strncmp((p_line + 60), "# / TYPES OF OBSERV", C1 * 19) == 0 && *(p_line + 5) != ' ') {
					*flag[0] = '\0';
					ntype = atoi(p_line);
					if (ntype > MAXTYPE) 
						return error_exit(HTNKRESULT::ERROR_LINE_DATA_TYPE_NUMBER_EXCEED, p_line);
				}
			}
		}
	}
	else {
		if (strlen(p_line) < 35 || *(p_line + 29) == '.') 
			return error_exit(HTNKRESULT::ERROR_READ_LINE, p_line);
		/* chop blanks that were padded in get_next_epoch */
		p = strchr(p_line + 35, '\0'); while (*--p == ' ') {}; *++p = '\0';
		printf("%s\n", p_line);
		n = atoi((p_line + 32));         /** n: number of lines to follow **/
		for (i = 0; i < n; i++) {
			read_chk_line(p_line);
			printf("%s\n", p_line);
			if (strncmp((p_line + 60), "SYS / # / OBS TYPES", C1 * 19) == 0 && *p_line != ' ') {
				*flag[0] = '\0';
				ntype_gnss[(unsigned int)*p_line] = atoi((p_line + 3));
				if (ntype_gnss[(unsigned int)*p_line] > MAXTYPE) 
					return error_exit(HTNKRESULT::ERROR_LINE_DATA_TYPE_NUMBER_EXCEED, p_line);
			}
		}
	}

	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
HTNKRESULT CHatanaka::read_clock(char *p_clock, int shift_clk) {
	/****  read the clock offset value ****/
	/**  *p_clock : pointer to beginning of clock data **/
	char *p_dot;      /** pointer for decimal point **/
	p_dot = p_clock + 2;
	if (*p_dot != '.')
		return error_exit(HTNKRESULT::ERROR_CLOCK_OFFSER, p_clock);

	strncpy(p_dot, p_dot + 1, C1*shift_clk);  /**** shift digits because of too  ****/
	*(p_dot + shift_clk) = '.';             /**** many digits for fractional part ****/
	sscanf(p_clock, "%ld.%ld", &clk1.u[0], &clk1.l[0]);
	if (*p_clock == '-' || *(p_clock + 1) == '-') clk1.l[0] = -clk1.l[0];
	if (clk_order < ARC_ORDER) clk_order++;
	*p_clock = '\0';

	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
void CHatanaka::process_clock(void) {
	int i;
	for (i = 0; i < clk_order; i++) {
		clk1.u[i + 1] = clk1.u[i] - clk0.u[i];
		clk1.l[i + 1] = clk1.l[i] - clk0.l[i];
	}
}
/*---------------------------------------------------------------------*/
HTNKRESULT CHatanaka::set_sat_table(char *p_new, char *p_old, int nsat_old, int *sattbl) {
	/**** sattbl : order of the satellites in the previous epoch   ****/
	/**** if *sattbl is set to  -1, the data arc for the satellite ****/
	/**** will be initialized                                      ****/
	int i, j;
	char *ps;

	for (i = 0; i < nsat; i++, p_new += 3) {
		*sattbl = -1;
		ps = p_old;
		for (j = 0; j < nsat_old; j++, ps += 3) {
			if (strncmp(p_new, ps, C3) == 0) {
				*sattbl = j;
				break;
			}
		}
		/*** check double entry ***/
		for (j = i + 1, ps = p_new + 3; j < nsat; j++, ps += 3) {
			if (strncmp(p_new, ps, C3) == 0) {
				if (!skip_strange_epoch) 
					return error_exit(HTNKRESULT::ERROR_LINE_DUPLICATE_SAT_IN_EPOCH, p_new);
				fprintf(stderr, "WARNING:Duplicated satellite in one epoch at line %ld. ... skip\n", nl_count);
				return HTNKRESULT::ERROR_LINE_DUPLICATE_SAT_IN_EPOCH;
			}
		}
		sattbl++;
	}
	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
HTNKRESULT CHatanaka::read_more_sat(int n, char *p) {
	/**** read continuation line of satellite list (for RINEX2) ****/
	char line[MAXCLM];

	do {
		p += 36;
		HTNKRESULT res = read_chk_line(line);
		if (res != HTNKRESULT::OK) 
			return res;
		/**** append satellite table ****/
		if (line[2] == ' ') {
			sprintf(p, "%s", &line[32]);
		}
		else {                        /*** for the files before clarification of format ***/
			sprintf(p, "%s", &line[0]); /*** by W.Gurtner (IGS mail #1577)                ***/
		}
		n -= 12;
	} while (n > 12);
	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
void CHatanaka::data(int *sattbl) {
	/********************************************************************/
	/*  Function : output the 3rd order difference of data              */
	/*       u : upper X digits of the data                             */
	/*       l : lower 5 digits of the data                             */
	/*            ( y = u*100 + l/1000 )                                */
	/*   py->u : upper digits of the 3rd order difference of the data   */
	/*   py->l : lower digits of the 3rd order difference of the data   */
	/********************************************************************/
	data_format *py1;
	int  i, j, *i0;
	char *p;

	for (i = 0, i0 = sattbl; i < nsat; i++, i0++) {
		for (j = 0, py1 = dy1[i]; j < ntype_record[i]; j++, py1++) {
			if (py1->order >= 0) {       /*** if the numerical data field is non-blank ***/
				if (*i0 < 0 || dy0[*i0][j].order == -1) {
					/**** initialize the data arc ****/
					py1->order = 0; p_buff += sprintf(p_buff, "%d&", ARC_ORDER);
				}
				else {
					take_diff(py1, &(dy0[*i0][j]));
					if (labs(py1->u[py1->order]) > 100000) {
						/**** initialization of the arc for large cycle slip  ****/
						py1->order = 0; p_buff += sprintf(p_buff, "%d&", ARC_ORDER);
					}
				}
				putdiff(py1->u[py1->order], py1->l[py1->order]);
			}
			else if (*i0 >= 0 && rinex_version == 2) {
				/**** CRINEX1 (RINEX2) initialize flags for blank field, not put '&' ****/
				flag0[*i0][j * 2] = flag0[*i0][j * 2 + 1] = ' ';
			}
			if (j < ntype_record[i] - 1) *p_buff++ = ' ';   /** ' ' :field separator **/
		}
		*(p_buff++) = ' ';  /* write field separator */
		if (*i0 < 0) {             /* if new satellite initialize all LLI & SN flags */
			if (rinex_version == 2) {
				p_buff = strdiff("", flag[i], p_buff);
			}
			else {          /*  replace space with '&' for CRINEX3(RINEX3)  */
				for (p = flag[i]; *p != '\0'; p++) *p_buff++ = (*p == ' ') ? '&' : *p;
				*p_buff++ = '\n'; *p_buff = '\0';
			}
		}
		else {
			p_buff = strdiff(flag0[*i0], flag[i], p_buff);
		}
	}
}
/*---------------------------------------------------------------------*/
char* CHatanaka::strdiff(char *s1, char *s2, char *ds) {
	/********************************************************************/
	/**   copy only the difference of string s2 from string s1         **/
	/**   '&' is marked when some character changed to a space         **/
	/**   trailing blank is eliminated and '/n' is added               **/
	/********************************************************************/
	for (; *s1 != '\0' && *s2 != '\0'; s2++) {
		if (*s2 == *(s1++))
			*ds++ = ' ';
		else if (*s2 == ' ')
			*ds++ = '&';
		else
			*ds++ = *s2;
	}
	strcpy(ds, s1);
	for (; *ds; ds++) { if (*ds != ' ') *ds = '&'; }
	while (*s2) *ds++ = *s2++;

	for (ds--; *ds == ' '; ds--);    /*** find pointer of last non-space character ***/
	*++ds = '\n'; *++ds = '\0';       /*** chop spaces at the end of the line ***/
	return ds;
}
/*---------------------------------------------------------------------*/
HTNKRESULT CHatanaka::ggetline(data_format *py1, char *flag, char *sat_id, int *ntype_rec) {
	/**** read data line for one satellite and       ****/
	/**** set data difference and flags to variables ****/
	char line[MAXCLM], *p, *pmax, *p_1st_rec;
	int i, j, nfield, max_field;
	HTNKRESULT res = read_chk_line(line);
	if (res != HTNKRESULT::OK) 
		return res;

	if (rinex_version == 2) {             /** for RINEX2 **/
		max_field = 5;                             /** maximum data types in one line **/
		*ntype_rec = ntype;                        /** # of data types for the satellite **/
		p_1st_rec = line;                          /** pointer to the start of the first record **/
	}
	else {                                /** for RINEX3 **/
		strncpy(sat_id, line, C3);                   /** put satellite ID to the list of satellites **/
		max_field = *ntype_rec = ntype_gnss[(unsigned int)line[0]];  /*** # of data type for the GNSS system ***/
		if (max_field < 0) 
			return error_exit(HTNKRESULT::ERROR_LINE_GNSS_NOT_DEFINE, line);
		p_1st_rec = line + 3;
	}
	for (i = 0; i < *ntype_rec; i += max_field) {                 /* for each line */
		nfield = (*ntype_rec - i < max_field ? *ntype_rec - i : max_field); /*** expected # of data fields in the line ***/
		pmax = p_1st_rec + 16 * nfield;

		/*** Cut or pad spaces. Detect error if there is any character after *pmax ***/
		p = strchr(line, '\0');
		if (p < pmax) {
			while (p < pmax) *p++ = ' '; *p = '\0';
		}
		else {
			for (*p = ' '; p > pmax && *p == ' '; p--) {};
			if (p > pmax) {
				if (!skip_strange_epoch) 
					return error_exit(HTNKRESULT::ERROR_DATA_TYPES_NUMBER, line);
				fprintf(stderr, "WARNING: mismatch of number of the data types at line %ld. ... skip\n", nl_count);
				return HTNKRESULT::ERROR_DATA_TYPES_NUMBER;
			}
		}

		/*** parse the line (read value into py1) ***/
		for (j = 0, p = p_1st_rec; j < nfield; j++, p += 16, py1++) {
			if (*(p + 10) == '.') {
				*flag++ = *(p + 14);
				*flag++ = *(p + 15);
				*(p + 14) = '\0';
				read_value(p, &(py1->u[0]), &(py1->l[0]));
				py1->order = 0;
			}
			else if (strncmp(p, "              ", C14) == 0) {
				if (rinex_version == 2 && strncmp((p + 14), "  ", C2) != 0) 
					return error_exit(HTNKRESULT::ERROR_LINE_BLANK_DATA, line);
				*flag++ = *(p + 14);
				*flag++ = *(p + 15);
				py1->order = -1;
			}
			else {
				if (!skip_strange_epoch) 
					return error_exit(HTNKRESULT::ERROR_ABNORMAL_DATA_FIELD, p);
				fprintf(stderr, "WARNING: abnormal data field at line %ld....skip\n", nl_count);
				return HTNKRESULT::ERROR_ABNORMAL_DATA_FIELD;
			}
		}
		if (i + max_field < *ntype_rec) {
			HTNKRESULT res = read_chk_line(line);
			if (res != HTNKRESULT::OK)
				return res;  /* read continuation line */
		}
	}
	*flag = '\0';

	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
void CHatanaka::read_value(char *p, long *pu, long *pl) {
	/**** divide the data into lower 5 digits and upper digits     ****/
	/**** input p :  pointer to one record (14 characters + '\0')  ****/
	/**** output  *pu, *pl: upper and lower digits the data        ****/

	char *p7, *p8, *p9;
	p7 = p + 7;
	p8 = p7 + 1;
	p9 = p8 + 1;

	*(p9 + 1) = *p9;            /* shift two digits: ex. 123.456 -> 1223456,  -.345 ->   -345 */
	*p9 = *p8;                /*                       -12.345 -> -112345, -1.234 -> --1234 */
	*pl = atol(p9);           /*                         0.123 ->  . 0123, -0.123 -> --0123 */

	if (*p7 == ' ') {
		*pu = 0;
	}
	else if (*p7 == '-') {
		*pu = 0;
		*pl = -*pl;
	}
	else {
		*p8 = '.';
		*pu = atol(p);
		if (*pu < 0) *pl = -*pl;
	}
}
/*---------------------------------------------------------------------*/
void CHatanaka::take_diff(data_format *py1, data_format *py0) {
	int k;

	py1->order = py0->order;
	if (py1->order < ARC_ORDER) (py1->order)++;
	if (py1->order > 0) {
		for (k = 0; k < py1->order; k++) {
			py1->u[k + 1] = py1->u[k] - py0->u[k];
			py1->l[k + 1] = py1->l[k] - py0->l[k];
		}
	}
}
/*---------------------------------------------------------------------*/
void CHatanaka::putdiff(long dddu, long dddl) {

	dddu += dddl / 100000; dddl %= 100000;
	if (dddu < 0 && dddl>0) {
		dddu++; dddl -= 100000;
	}
	else if (dddu > 0 && dddl < 0) {
		dddu--; dddl += 100000;
	}

	if (dddu == 0) {
		p_buff += sprintf(p_buff, "%ld", dddl);
	}
	else {
		p_buff += sprintf(p_buff, "%ld%5.5ld", dddu, labs(dddl));
	}
}
/*---------------------------------------------------------------------*/
void CHatanaka::put_clock(long du, long dl, int c_order) {
	/***********************************/
	/****  output clock diff. data  ****/
	/***********************************/
	du += dl / 100000000; dl %= 100000000;
	if (du < 0 && dl>0) {
		du++; dl -= 100000000;
	}
	else if (du > 0 && dl < 0) {
		du--; dl += 100000000;
	}
	if (c_order == 0) p_buff += sprintf(p_buff, "%d&", ARC_ORDER);
	if (du == 0) {
		p_buff += sprintf(p_buff, "%ld\n", dl);
	}
	else {
		p_buff += sprintf(p_buff, "%ld%8.8ld\n", du, labs(dl));
	}
}
/*---------------------------------------------------------------------*/
HTNKRESULT  CHatanaka::read_chk_line(char *line) {
	char *p;
	/***************************************/
	/* Read and check one line.            */
	/* The end of the line should be '\n'. */
	/***************************************/
	nl_count++;
	if (fgets(line, MAXCLM, stdin) == NULL) 
		return error_exit(HTNKRESULT::ERROR_TRUNCATED_FILE, line);
	if ((p = strchr(line, '\n')) == NULL) {
		if (fgetc(stdin) == EOF) {
			return error_exit(HTNKRESULT::ERROR_TRUNCATED_FILE, line);
		}
		else {
			if (!skip_strange_epoch) 
				return error_exit(HTNKRESULT::ERROR_LINE_NULL_CHAR, line);
			fprintf(stderr, "WARNING: null character is found or the line is too long (>%d) at line %ld.\n", MAXCLM, nl_count);
			return HTNKRESULT::ERROR_LINE_NULL_CHAR;
		}
	}
	if (*(p - 1) == '\r')p--;   /*** check DOS CR/LF ***/
	while (*--p == ' ' && p > line) {}; *++p = '\0';   /** Chop blank **/

	return HTNKRESULT::OK;
}
/*---------------------------------------------------------------------*/
HTNKRESULT CHatanaka::error_exit(HTNKRESULT error_no, char *string)
{
	if (authenticMode)
		error_exit_old(error_no, string);
	
	return error_no;
}
/*---------------------------------------------------------------------*/
HTNKRESULT CHatanaka::error_exit_old(HTNKRESULT error_no, char *string) {
	if (error_no == HTNKRESULT::INVALID_FILES_NAME) {
		fprintf(stderr, "Usage: %s input file [-o output file] [-f] [-e # of epochs] [-s] [-h]\n", string);
		fprintf(stderr, "    output file name can be omitted if input file name is *.[yy]o\n");
	}
	else if (error_no == HTNKRESULT::STD_IN_OUT_ERROR) {
		fprintf(stderr, "Usage: %s [file] [-] [-f] [-e # of epochs] [-s] [-h]\n", string);
		fprintf(stderr, "    stdin and stdout are used if input file name is not given.\n");
	}

	switch (error_no)
	{
	case HTNKRESULT::INVALID_FILES_NAME:
	case HTNKRESULT::STD_IN_OUT_ERROR:
			fprintf(stderr, "    -       : output to stdout\n");
			fprintf(stderr, "    -f      : force overwrite of output file\n");
			fprintf(stderr, "    -e #    : initialize the compression operation at every # epochs\n");
			fprintf(stderr, "              When some part of the Compact RINEX file is lost, the data\n");
			fprintf(stderr, "              can not be recovered thereafter until all the data arc are\n");
			fprintf(stderr, "              initialized for differential operation. This option may be used to\n");
			fprintf(stderr, "              increase chances to recover parts of data by using an option of\n");
			fprintf(stderr, "              CRX2RNX(ver. 4.0 or after) with cost of increase of file size.\n");
			fprintf(stderr, "    -s      : warn and skip strange epochs (default: stop with error status)\n");
			fprintf(stderr, "    -h      : display this message\n\n");
			fprintf(stderr, "    exit code = %d (success)\n", EXIT_SUCCESS);
			fprintf(stderr, "              = %d (error)\n", EXIT_FAILURE);
			fprintf(stderr, "              = %d (warning)\n", EXIT_WARNING);
			fprintf(stderr, "    [version : %s]\n", programmVersion.c_str());
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_FILE_NAME:
			fprintf(stderr, "ERROR : invalid file name  %s\n", string);
			fprintf(stderr, "The extension of the input file name should be [.xxo] or [.rnx].\n");
			fprintf(stderr, "To convert the files whose name is not fit to the above conventions,\n");
			fprintf(stderr, "use of this program as a filter is also possible. \n");
			fprintf(stderr, "    for example)  cat file.in | %s - > file.out\n", programmName.c_str());
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_OPEN_FILE:
			fprintf(stderr, "ERROR : can't open %s\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_READ_LINE:
			fprintf(stderr, "ERROR when reading line %ld.\n", nl_count);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_CLOCK_OFFSER:
			fprintf(stderr, "ERROR at line %ld: invalid format for clock offset.\n", nl_count);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_SAT_NUBMER:
			fprintf(stderr, "ERROR at line %ld : number of satellites exceed the maximum(%d).\n", nl_count, MAXSAT);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_DATA_TYPES_NUMBER:
			fprintf(stderr, "ERROR at line %ld : mismatch of number of the data types.\n", nl_count);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_ABNORMAL_DATA_FIELD:
			fprintf(stderr, "ERROR at line %ld : abnormal data field.\n", nl_count);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_TRUNCATED_FILE:
			fprintf(stderr, "ERROR : The RINEX file seems to be truncated in the middle.\n");
			fprintf(stderr, "        The conversion is interrupted after reading line %ld :\n", nl_count);
			fprintf(stderr, "        start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_LINE_NULL_CHAR:
			fprintf(stderr, "ERROR at line %ld. : null character is found or the line is too long (>%d).\n", nl_count, MAXCLM);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_LINE_DUPLICATE_SAT_IN_EPOCH:
			fprintf(stderr, "ERROR at line %ld. : Duplicated satellite in one epoch.\n", nl_count);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_LINE_LENGTH_FILE_NAME:
			fprintf(stderr, "ERROR at line %ld. : Length of file name exceed MAXCLM(%d).\n", nl_count, MAXCLM);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_INVALID_FILE_FORMAT:
			fprintf(stderr, "The first line is :\n%s\n\n", string);
			fprintf(stderr, "ERROR : The file format is not valid. This program is applicable\n");
			fprintf(stderr, "        only to RINEX Version 2/3 Observation file.\n");
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_LINE_DATA_TYPE_NUMBER_EXCEED:
			fprintf(stderr, "ERROR at line %ld. : Number of data types exceed MAXTYPE(%d).\n", nl_count, MAXTYPE);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_LINE_BLANK_DATA:
			fprintf(stderr, "ERROR at line %ld. : data is blank but there is flag.\n", nl_count);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	case HTNKRESULT::ERROR_LINE_GNSS_NOT_DEFINE:
			fprintf(stderr, "ERROR at line %ld. : GNSS type is not defined in the header.\n", nl_count);
			fprintf(stderr, "     start>%s<end\n", string);
			throw std::invalid_argument("Error."); //exit(EXIT_FAILURE);
			break;
	default:
		return HTNKRESULT::UNKNOWN;
	}
}

void CHatanaka::flush_buff()
{
	printf("%s", top_buff);
	*(p_buff = top_buff) = '\0';
}

void CHatanaka::clear_buff()
{
	*(p_buff = top_buff) = '\0';
}
