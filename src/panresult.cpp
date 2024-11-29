#include "panresult.h"
#include <cstdio>
#include <cstdlib>

const struct ErrorMessage errorMessages[PAN__MAX] = {
	{PAN_OK, "exited without error"},
	{PAN_ERR_INSTRUCTION_READ, "no bytes could be read from input binary"},
	{PAN_ERR_DATA_MISSING_LOCATION,
   "data in a stack did not have a valid location (ie. was neither a reg, spm, "
   "or integer)"},
	{PAN_ERR_INVALID_FILE, "Specified file could not be opened"},
};

[[noreturn]] void panExit(PanResult exitCode, const char* userMessage) {
	printf("Program terminated. Reason: %s\n", errorMessages[exitCode].message);
	if (userMessage) {
		printf("User Message: %s\n", userMessage);
	}
	exit(exitCode);
}
