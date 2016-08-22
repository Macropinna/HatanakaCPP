#pragma once

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string>
#include <stdexcept>


/**** Exit codes are defined here.   ****/
#define EXIT_WARNING 2

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif


#define CRX_VERSION1 "1.0"    /* CRINEX version for RINEX 2.x */
#define CRX_VERSION2 "3.0"    /* CRINEX version for RINEX 3.x */
#define MAXSAT    100         /* Maximum number of satellites observed at one epoch */
#define MAXTYPE   100         /* Maximum number of data types for a GNSS system */
#define MAXCLM   2048         /* Maximum columns in one line   (>MAXTYPE*19+3)  */
#define MAX_BUFF_SIZE 204800  /* Maximum size of output buffer (>MAXSAT*(MAXTYPE*19+4)+60 */
#define ARC_ORDER 3           /* order of difference to take    */


enum class HTNKRESULT {
	UNKNOWN = 0,
	INVALID_FILES_NAME = 1,
	STD_IN_OUT_ERROR = 2,
	ERROR_FILE_NAME = 4,
	ERROR_OPEN_FILE = 5,
	ERROR_READ_LINE = 6,
	ERROR_CLOCK_OFFSER = 7,
	ERROR_SAT_NUBMER = 8,
	ERROR_DATA_TYPES_NUMBER = 9,
	ERROR_ABNORMAL_DATA_FIELD = 10,
	ERROR_TRUNCATED_FILE = 11,
	ERROR_LINE_NULL_CHAR = 12,
	ERROR_LINE_DUPLICATE_SAT_IN_EPOCH = 13,
	ERROR_LINE_LENGTH_FILE_NAME = 14, 
	ERROR_INVALID_FILE_FORMAT = 15,
	ERROR_LINE_DATA_TYPE_NUMBER_EXCEED = 16,
	ERROR_LINE_BLANK_DATA = 20,
	ERROR_LINE_GNSS_NOT_DEFINE = 21,


	ERROR_OPEN_RW_FILES = 30,
	ERROR_HEADER = 31,
	ERROR_LINE = 32,

	ERROR_PROG_NAME_TOO_LONG = 33,
	ERROR_PROG_VERSION_TOO_LONG = 33,


	OK_EOF = 40,

	OK = 100,
};


/* define data structure for fields of clock offset and observation records */
/* Those data will be handled as integers after eliminating decimal points.  */
/* Since their size may exceeds range between LONG_MIN and LONG_MAX,         */
/* they will be read with being divided properly into upper and lower digits */
typedef struct clock_format {
	long u[ARC_ORDER + 1];      /* upper X digits */
	long l[ARC_ORDER + 1];      /* lower 8 digits (can be 9-10 digits for deltas)*/
} clock_format;

typedef struct data_format {
	long u[ARC_ORDER + 1];      /* upper X digits */
	long l[ARC_ORDER + 1];      /* lower 5 digits (can be 6-7 digits for deltas) */
	int order;
} data_format;

class CHatanaka
{
public:
	CHatanaka();
	~CHatanaka();
	HTNKRESULT compressFile(const std::string& fileName);
	HTNKRESULT compressFile(const std::string& inputFileName, const std::string& outputFileName);
	void setProgrammName(std::string programmName_);
	void setProgrammVersion(std::string programmVersion_);
	void setAuthenticMode(bool mode);
	bool getAuthenticMode();
	void closeFiles();
private:
	/* declaration of functions */	
	bool open_rw_files(std::string fileName, std::string output = "");
	void close_rw_files();
	bool header(void);
	HTNKRESULT  get_next_epoch(char *p_line);
	void skip_to_next(char *p_line);
	void initialize_all(char *oldline, int *nsat_old, int count);
	HTNKRESULT put_event_data(char *p_line);
	HTNKRESULT read_clock(char *line, int shift_cl);
	void process_clock(void);
	HTNKRESULT set_sat_table(char *p_new, char *p_old, int nsat_old, int *sattbl);
	HTNKRESULT  read_more_sat(int n, char *p);
	void data(int *sattbl);
	char *strdiff(char *s1, char *s2, char *ds);
	HTNKRESULT  ggetline(data_format *py1, char *flag, char *sat_id, int *ntype_rec);
	void read_value(char *p, long *pu, long *pl);
	void take_diff(data_format *py1, data_format *py0);
	void putdiff(long dddu, long dddl);
	void put_clock(long du, long dl, int clk_order);
	HTNKRESULT  read_chk_line(char *line);
	HTNKRESULT error_exit(HTNKRESULT error_no, char *string);
	HTNKRESULT error_exit_old(HTNKRESULT error_no, char *string);

	void flush_buff();
	void clear_buff();

private:
	bool authenticMode = false;
	std::string programmName = "RNX2CRX";
	std::string programmVersion = "ver.4.0.7";
	/* define variables */
	long ep_count = 0;
	long ep_reset = 0;
	long nl_count = 0;
	int rinex_version;          /* =2 or 3 */
	int nsat, ntype, ntype_gnss[UCHAR_MAX], ntype_record[MAXSAT], clk_order = -1;
	int exit_status = EXIT_SUCCESS;
	int skip_strange_epoch = 0; /* default : stop with error */

								/*
								clock_format clk1,clk0 = {0,0,0,0,0,0,0,0};
								*/
	clock_format clk1, clk0 = { { 0,0,0,0 },{ 0,0,0,0 } };
	data_format dy0[MAXSAT][MAXTYPE], dy1[MAXSAT][MAXTYPE];
	char flag0[MAXSAT][MAXTYPE * 2], flag[MAXSAT][MAXTYPE * 2];
	char out_buff[MAX_BUFF_SIZE] = { 'x','\0' };  /**** a character is put as a stopper to avoid memory overflow ****/
	char *top_buff = &out_buff[1], *p_buff;        /**** therefore, actual buffer start from the second character ****/
	size_t C1 = sizeof("");               /* size of one character */
	size_t C2 = sizeof(" ");              /* size of 2-character string */
	size_t C3 = sizeof("  ");             /* size of 3-character string */
	size_t C14 = sizeof("             ");  /* size of 14-character string */

	char oldline[MAXCLM] = { '&','\0' };
	int nsat_old = 0;

};

