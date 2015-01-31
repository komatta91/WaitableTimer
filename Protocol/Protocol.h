#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <Windows.h>
#include <string>

static const unsigned int BUFSIZE = 512;

struct TimerOperation
{
	DWORD ThreadId;
	DWORD duration;
	LONG period;
	BOOL manualReset;

	enum Operation {
		CREATE,
		CLOSE,
		SET,
		WAIT,
		GET,
		CANCEL
	} Operation;

	explicit TimerOperation() : ThreadId(GetCurrentThreadId()), duration(0), period(0), manualReset(false) {};

	explicit TimerOperation(const DWORD threadId, const std::string& name, const enum Operation operation)
		: ThreadId(threadId), Operation(operation)
	{
		SetName(name);
	}

	explicit TimerOperation(const std::string& name, const enum Operation operation)
		: ThreadId(GetCurrentThreadId()), Operation(operation)
	{
		SetName(name);
	}

	std::string GetName() const {
		return std::string(Name);
	}

	void SetName(const std::string& name) {
		std::string Safe(name);
		Safe.resize(NAME_LENGTH - 1);
		//strncpy(Name, Safe.c_str(), NAME_LENGTH);
		strcpy_s(Name, Safe.length() + 1, Safe.c_str());
	}

private:
	static const DWORD NAME_LENGTH = 256;
	char Name[NAME_LENGTH];
};

struct Response
{
	enum Status
	{
		OK,
		ERR
	} Status;
	explicit Response() {}

	explicit Response(const std::string& name)
	{
		SetName(name);
	}

	std::string GetName() const {
		return std::string(Name);
	}

	void SetName(const std::string& name) {
		std::string Safe(name);
		Safe.resize(NAME_LENGTH - 1);
		strcpy_s(Name, Safe.length() + 1, Safe.c_str());
	}

private:
	static const DWORD NAME_LENGTH = 256;
	char Name[NAME_LENGTH];
};

#endif