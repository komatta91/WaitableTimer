#include <Windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <Protocol.h>

int main(int argc, const char* argv[])
{
	std::locale::global(std::locale(""));
	HANDLE hPipe;
	//const char* lpvMessage = TEXT("Default message from client.");
	//char  chBuf[BUFSIZE];

	//DWORD  cbRead, cbToWrite, cbWritten, dwMode;

	const char* pipeName = "\\\\.\\pipe\\mynamedpipe";
	while (true)
	{
		hPipe = CreateFileA(
			pipeName,   // pipe name
			GENERIC_READ |  // read and write access
			GENERIC_WRITE,
			0,              // no sharing
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe
			0,              // default attributes
			NULL);          // no template file

		// Break if the pipe handle is valid.

		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		// Exit if an error other than ERROR_PIPE_BUSY occurs.

		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			//_tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
			std::cout << "Nie mo¿na otworzyæ potoku. GLE = " << GetLastError() << ".\n";
			return -1;
		}

		// All pipe instances are busy, so wait for 20 seconds.

		if (!WaitNamedPipeA(pipeName, 20000))
		{
			//printf("Could not open pipe: 20 second wait timed out.");
			std::cout << "Nie mo¿na otworzyæ potoku: timeout 20 sekund.\n";
			return -1;
		}
	}

	DWORD mode = PIPE_READMODE_MESSAGE;

	bool success = SetNamedPipeHandleState(
		hPipe,    // pipe handle
		&mode,  // new pipe mode
		NULL,     // don't set maximum bytes
		NULL);    // don't set maximum time
	if (!success)
	{
		std::cout << "B³¹d SetNamedPipeHandleState. GLE = " << GetLastError() << ".\n";
		return -1;
	}

	do
	{
		// Read from the pipe.

		Response r;
		DWORD read;

		success = ReadFile(
			hPipe,    // pipe handle
			&r,    // buffer to receive reply
			sizeof(r),  // size of buffer
			&read,  // number of bytes read
			NULL);    // not overlapped

		std::cout << "Odebrano odpowedŸ : " << (r.Status == r.OK ? "OK " : "B£¥D ") << r.GetName() << ".\n\n";

		if (r.Status != r.OK)
		{
			return -1;
		}
		if (!success && GetLastError() != ERROR_MORE_DATA)
			break;
	} while (!success);  // repeat loop if ERROR_MORE_DATA

	bool test = true;
	do
	{
		if (GetAsyncKeyState(VK_ESCAPE))
		{
			test = false;
		}
		std::cout << "polecenie: ";
		TimerOperation op;
		DWORD toWrite = sizeof(op);
		DWORD written;
		std::string operation;
		std::string name;

		std::string temp;
		std::getline(std::cin, temp);
		std::stringstream ss(temp);
		ss >> operation;

		bool test = false;
		if (operation == "create")
		{
			ss >> name;

			op.SetName(name);
			op.Operation = TimerOperation::CREATE;
			std::string reset;
			ss >> reset;
			op.manualReset = reset == "tak";

			if (name.size() != 0)
			{
				test = true;
			}
		}
		if (operation == "close")
		{
			ss >> name;
			op.SetName(name);
			op.Operation = op.CLOSE;

			test = true;
		}
		if (operation == "set")
		{
			ss >> name;
			op.SetName(name);
			op.Operation = op.SET;

			DWORD du;
			DWORD per;
			if (!ss.eof())
			{
				ss >> du;
			}
			else
			{
				std::cout << "Niew³aœciwa sk³adnia!\n\n";
				continue;
			}
			if (!ss.eof())
			{
				ss >> per;
			}
			else
			{
				per = 0;
			}

			op.duration = du;
			op.period = per;
			test = true;
		}
		if (operation == "wait")
		{
			ss >> name;
			op.SetName(name);
			op.Operation = op.WAIT;

			DWORD du;
			if (!ss.eof())
			{
				ss >> du;
			}
			else
			{
				du = INFINITE;
			}

			op.duration = du;
			test = true;
		}
		if (operation == "get")
		{
			ss >> name;
			op.SetName(name);
			op.Operation = op.GET;

			test = true;
		}
		if (operation == "cancel")
		{
			ss >> name;
			op.SetName(name);
			op.Operation = op.CANCEL;

			test = true;
		}
		if (!test)
		{
			std::cout << "Nie rozpoznano polecenia!\n\n";
			continue;
		}

		success = WriteFile(
			hPipe,                  // pipe handle
			&op,             // message
			toWrite,              // message length
			&written,             // bytes written
			NULL);                  // not overlapped

		if (!success)
		{
			std::cout << "Nie uda³o siê wys³¹æ wiadomoœci. GLE = " << GetLastError() << ".\n\n";
			return -1;
		}

		std::cout << "Wys³ano wiadomoœæ.\n";

		do
		{
			// Read from the pipe.

			Response r;
			DWORD read;

			success = ReadFile(
				hPipe,    // pipe handle
				&r,    // buffer to receive reply
				sizeof(r),  // size of buffer
				&read,  // number of bytes read
				NULL);    // not overlapped

			if (!success && GetLastError() != ERROR_MORE_DATA)
				break;
			std::cout << "Odebrano odpowedŸ : " << (r.Status == r.OK ? "OK " : "B£¥D ") << r.GetName() << ".\n\n";
		} while (!success);  // repeat loop if ERROR_MORE_DATA

		if (!success)
		{
			return -1;
		}
	} while (test);

	CloseHandle(hPipe);
	return 0;
}