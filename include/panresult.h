#pragma once

enum PanResult {
	PAN_OK,
	PAN_ERR_INSTRUCTION_READ,
	PAN_ERR_DATA_MISSING_LOCATION,
	PAN_ERR_INVALID_FILE,
	PAN__MAX,
};

struct ErrorMessage {
	PanResult result;
	const char* message;
};

extern const ErrorMessage errorMessages[PAN__MAX];

[[noreturn]] void panExit(PanResult, const char* userMessage = nullptr);
