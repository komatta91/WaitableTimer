#include <Windows.h>
#include <iostream>
#include <string>
#include <map>
#include <Protocol.h>

struct MyHandler
{
	MyHandler() : handle(NULL), count(0) {}
	HANDLE handle;
	int count;
};

namespace
{
	static CRITICAL_SECTION CriticalSection;
	static std::map<std::string, MyHandler> GlobalTimers;
	static const DWORD MAX_NUM_CLIENTS = 10;
	static DWORD numClient = 0;
}

DWORD WINAPI InstanceThread(LPVOID param);
bool sendResponse(HANDLE hPipe, std::string res, enum Response::Status status);
HANDLE AddTimer(const TimerOperation* request);
bool CancelTimer(const TimerOperation* request);
bool DeleteTimer(const TimerOperation* request);
bool DeleteTimer(const std::string& name);
HANDLE GetTimer(const TimerOperation* request);

int main(int argc, const char* argv[])
{
	if (!InitializeCriticalSectionAndSpinCount(&CriticalSection, 0x00000400))
	{
		return -1;
	}

	std::locale::global(std::locale(""));

	while (true)
	{
		const char* pipeName = "\\\\.\\pipe\\mynamedpipe";
		HANDLE hPipe = 0;
		HANDLE hThread = 0;
		DWORD  threadID = 0;

		std::cout << "G³ówny w¹tek oczekuje na klientów.\n";

		hPipe = CreateNamedPipeA(
			pipeName,                 // pipe name
			PIPE_ACCESS_DUPLEX,       // read/write access
			PIPE_TYPE_MESSAGE |       // message type pipe
			PIPE_READMODE_MESSAGE |   // message-read mode
			PIPE_WAIT,                // blocking mode
			PIPE_UNLIMITED_INSTANCES, // max. instances
			BUFSIZE,                  // output buffer size
			BUFSIZE,                  // input buffer size
			0,                        // client time-out
			NULL);                    // default security attribute

		if (hPipe == INVALID_HANDLE_VALUE)
		{
			std::cout << "Nie uda³o siê otworzyæ potoku. GLE = " << GetLastError() << " .\n";
			return 1;
		}

		bool connected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (connected)
		{
			if (numClient < MAX_NUM_CLIENTS)
			{
				numClient++;
				std::cout << "Uda³o siê po³¹czyæ z klientem, uruchamiam nowy w¹tek.\n";
				hThread = CreateThread(
					NULL,              // no security attribute
					0,                 // default stack size
					InstanceThread,    // thread proc
					(LPVOID)hPipe,     // thread parameter
					0,                 // not suspended
					&threadID);        // returns thread ID

				if (hThread == NULL)
				{
					std::cout << "Nie uda³o siê uruchomiæ w¹tku. GLE = " << GetLastError() << " .\n";

					return 1;
				}
				else CloseHandle(hThread);
			}
			else
			{
				std::cout << "Przekroczona maksymalna liczba klientów.\n";
				sendResponse(hPipe, "Serwer zajêty!", Response::ERR);
				CloseHandle(hPipe);
			}
		}
		else
		{
			CloseHandle(hPipe);
		}
	}
	DeleteCriticalSection(&CriticalSection);
	return 0;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam)
{
	std::string ok("OK");
	std::string error("ERROR");

	DWORD threadID = GetCurrentThreadId();

	HANDLE hHeap = GetProcessHeap();
	TimerOperation* request = (TimerOperation*)HeapAlloc(hHeap, 0, sizeof(TimerOperation));
	char* replay = (char*)HeapAlloc(hHeap, 0, BUFSIZE*sizeof(char));

	if (lpvParam == NULL)
	{
		std::cout << "B³¹d serwera!\n";
		if (replay != NULL) HeapFree(hHeap, 0, replay);
		if (request != NULL) HeapFree(hHeap, 0, request);
		return -1;
	}

	if (request == NULL)
	{
		std::cout << "B³¹d serwera!\n";
		if (replay != NULL) HeapFree(hHeap, 0, replay);
		return -1;
	}

	if (replay == NULL)
	{
		std::cout << "B³¹d serwera!\n";
		if (request != NULL) HeapFree(hHeap, 0, request);
		return -1;
	}

	std::cout << "W¹tek " << threadID << ": " << "uruchomiono w¹tek roboczy.\n";

	HANDLE hPipe = (HANDLE)lpvParam;
	DWORD bytesRead = 0;
	bool success;
	std::map<std::string, HANDLE> timers;

	sendResponse(hPipe, "OK", Response::OK);

	while (true)
	{
		success = ReadFile(
			hPipe,                  // handle to pipe
			request,                // buffer to receive data
			sizeof(TimerOperation), // size of buffer
			&bytesRead,             // number of bytes read
			NULL);                  // not overlapped I/O

		if (!success || bytesRead == 0)
		{
			if (GetLastError() == ERROR_BROKEN_PIPE)
			{
				std::cout << "W¹tek " << threadID << ": " << "klient roz³¹czy³ siê.\n";
			}
			else
			{
				std::cout << "W¹tek " << threadID << ": " << "b³¹d odczytu, GLE = " << GetLastError() << ".\n";
			}
			break;
		}

		std::cout << "W¹tek " << threadID << ": otrzyma³ dane\n";
		switch (request->Operation)
		{
		case (TimerOperation::CREATE) :
		{
			std::cout << "CREATE " << request->GetName() << " manualReset: " << (request->manualReset ? "TAK" : "NIE") << std::endl;
			if (timers[request->GetName()] == NULL)
			{
				HANDLE t = AddTimer(request);

				timers[request->GetName()] = t;
				if (t != NULL)
				{
					timers[request->GetName()] = t;
					std::string ans = "Timer " + request->GetName() + " zosta³ utworzony!";
					sendResponse(hPipe, ans, Response::OK);
				}
				else
				{
					std::string ans = "Timer " + request->GetName() + " nie mo¿e zostaæ utworzony!";
					sendResponse(hPipe, ans, Response::ERR);
				}
			}
			else
			{
				std::string ans = "Timer " + request->GetName() + " ju¿ istnieje!";
				sendResponse(hPipe, ans, Response::ERR);
			}

			break;
		}
		case (TimerOperation::CLOSE) :
		{
			std::cout << "CLOSE " << request->GetName() << std::endl;

			if (timers[request->GetName()] != NULL)
			{
				success = DeleteTimer(request);
				if (success)
				{
					timers[request->GetName()] = NULL;
					std::string ans = "Timer " + request->GetName() + " zosta³ usuniêty!";
					sendResponse(hPipe, ans, Response::OK);
				}
				else
				{
					std::string ans = "Timer " + request->GetName() + " nie zosta³ usuniêty!";
					sendResponse(hPipe, ans, Response::ERR);
				}
			}
			else
			{
				std::string ans = "Timer " + request->GetName() + " nie istnieje!";
				sendResponse(hPipe, ans, Response::ERR);
			}
			break;
		}
		case (TimerOperation::SET) :
		{
			std::cout << "SET " << request->GetName() << " duration: " << request->duration << " period: " << request->period << std::endl;
			if (timers[request->GetName()] != NULL)
			{
				LARGE_INTEGER duel;
				duel.QuadPart = request->duration * -10000LL;

				success = SetWaitableTimer(timers[request->GetName()], &duel, request->period, NULL, NULL, 0);
				if (success)
				{
					std::string ans = "Timer " + request->GetName() + " ustawiony!";
					sendResponse(hPipe, ans, Response::OK);
				}
				else
				{
					std::string ans = "Timer " + request->GetName() + " nie ustawiony!";
					sendResponse(hPipe, ans, Response::ERR);
				}
			}
			else
			{
				std::string ans = "Timer " + request->GetName() + " nie istnieje!";
				sendResponse(hPipe, ans, Response::ERR);
			}

			break;
		}
		case (TimerOperation::WAIT) :
		{
			std::cout << "WAIT " << request->GetName() << " duration: " << request->duration << std::endl;
			if (timers[request->GetName()] != NULL)
			{
				DWORD res = WaitForSingleObject(timers[request->GetName()], request->duration);

				if (WAIT_OBJECT_0 == res)
				{
					std::string ans = "Timer " + request->GetName() + " zadzwoni³!";
					sendResponse(hPipe, ans, Response::OK);
				}
				else if (WAIT_ABANDONED == res)
				{
					std::string ans = "Timer " + request->GetName() + " nie zadzwoni³ - WAIT_ABANDONED!";
					sendResponse(hPipe, ans, Response::ERR);
				}
				else if (res == WAIT_TIMEOUT)
				{
					std::string ans = "Timer " + request->GetName() + " nie zadzwoni³ - WAIT_TIMEOUT!";
					sendResponse(hPipe, ans, Response::ERR);
				}
				else if (res == WAIT_FAILED)
				{
					std::string ans = "Timer " + request->GetName() + " nie zadzwoni³ WAIT_FAILED!";
					sendResponse(hPipe, ans, Response::ERR);
				}
				else
				{
					std::string ans = "Timer " + request->GetName() + " nie zadzwoni³!";
					sendResponse(hPipe, ans, Response::ERR);
				}
			}
			else
			{
				std::string ans = "Timer " + request->GetName() + " nie istnieje!";
				sendResponse(hPipe, ans, Response::ERR);
			}

			break;
		}
		case (TimerOperation::GET) :
		{
			std::cout << "GET " << request->GetName() << std::endl;

			if (timers[request->GetName()] == NULL)
			{
				timers[request->GetName()] = GetTimer(request);

				if (timers[request->GetName()] != NULL)
				{
					std::string ans = "Timer " + request->GetName() + " pobrany!";
					sendResponse(hPipe, ans, Response::OK);
				}
				else
				{
					std::string ans = "Timer " + request->GetName() + " nie pobrany!";
					sendResponse(hPipe, ans, Response::ERR);
				}
			}
			else
			{
				std::string ans = "Timer " + request->GetName() + " ju¿ istnieje!";
				sendResponse(hPipe, ans, Response::ERR);
			}

			break;
		}
		case (TimerOperation::CANCEL) :
		{
			std::cout << "CANCEL " << request->GetName() << std::endl;
			if (timers[request->GetName()] != NULL)
			{
				success = CancelWaitableTimer(
					timers[request->GetName()]
					);
				if (success)
				{
					std::string ans = "Timer " + request->GetName() + " anulowany!";
					sendResponse(hPipe, ans, Response::OK);
				}
				else
				{
					std::string ans = "Timer " + request->GetName() + " nie anulowany!";
					sendResponse(hPipe, ans, Response::ERR);
				}
			}
			else
			{
				std::string ans = "Timer " + request->GetName() + " nie istnieje!";
				sendResponse(hPipe, ans, Response::ERR);
			}
			break;
		default:
		{
			std::string ans = "Nie rozpoznano polecenia!";
			sendResponse(hPipe, ans, Response::ERR);
		}
		}
		};
		std::cout << std::endl;
	}

	for each (auto var in timers)
	{
		DeleteTimer(var.first);
	}

	std::cout << "W¹tek " << threadID << ": " << "koniec pracy.\n";
	numClient--;
	return 0;
}

bool sendResponse(HANDLE hPipe, std::string res, enum Response::Status status)
{
	Response r;
	r.Status = status;
	r.SetName(res);

	DWORD cbToWrite = sizeof(r);;
	DWORD written;
	return WriteFile(
		hPipe,     // pipe handle
		&r,        // message
		cbToWrite, // message length
		&written,  // bytes written
		NULL);     // not overlapped
}

HANDLE AddTimer(const TimerOperation* request)
{
	EnterCriticalSection(&CriticalSection);
	HANDLE t = NULL;

	if (GlobalTimers[request->GetName()].handle == NULL)
	{
		t = CreateWaitableTimerA(
			NULL,
			request->manualReset,
			request->GetName().c_str()
			);
		MyHandler h;
		h.count++;
		h.handle = t;

		GlobalTimers[request->GetName()] = h;
	}
	LeaveCriticalSection(&CriticalSection);
	return t;
}

bool CancelTimer(const TimerOperation* request)
{
	EnterCriticalSection(&CriticalSection);
	bool success = false;
	if (GlobalTimers[request->GetName()].handle != NULL)
	{
		success = CancelWaitableTimer(
			GlobalTimers[request->GetName()].handle
			);
	}
	LeaveCriticalSection(&CriticalSection);
	return success;
}

bool DeleteTimer(const TimerOperation* request)
{
	return DeleteTimer(request->GetName());
}

bool DeleteTimer(const std::string& name)
{
	bool ans = false;
	EnterCriticalSection(&CriticalSection);
	if (GlobalTimers[name].handle != NULL)
	{
		GlobalTimers[name].count--;
		if (0 == GlobalTimers[name].count)
		{
			ans = CloseHandle(
				GlobalTimers[name].handle
				);
			GlobalTimers[name].handle = NULL;
		}
		ans = true;
	}
	LeaveCriticalSection(&CriticalSection);
	return ans;
}

HANDLE GetTimer(const TimerOperation* request)
{
	EnterCriticalSection(&CriticalSection);
	HANDLE t = NULL;
	if (GlobalTimers[request->GetName()].handle != NULL)
	{
		t = GlobalTimers[request->GetName()].handle;
		GlobalTimers[request->GetName()].count++;
	}
	LeaveCriticalSection(&CriticalSection);
	return t;
}