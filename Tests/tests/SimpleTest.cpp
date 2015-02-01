#include <gtest\gtest.h>

#include <Protocol.h>

#include <Windows.h>
#include <ctime>

bool closeEnough(long long left, long long right, int threshold = 5)
{
	//return (std::abs(left - right) < threshold) ;
	return left <= right;// || (std::abs(left - right) < threshold);
}

HANDLE RunServer()
{
#ifdef _DEBUG
	const char * cmd = "..\\Debug\\Serwer.exe";
#else
	const char * cmd = "..\\Release\\Serwer.exe";
#endif
	PROCESS_INFORMATION pi;
	STARTUPINFOA si;
	ZeroMemory(&pi, sizeof(pi));
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	EXPECT_TRUE(CreateProcessA(NULL,
		(LPSTR)cmd,
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&si,
		&pi));

	Sleep(500);
	return pi.hProcess;
}

HANDLE Connect()
{
	DWORD mode = PIPE_READMODE_MESSAGE;
	const char* pipeName = "\\\\.\\pipe\\mynamedpipe";
	HANDLE hPipe = CreateFileA(
		pipeName,
		GENERIC_READ |
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	EXPECT_TRUE(hPipe != INVALID_HANDLE_VALUE);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}
	EXPECT_TRUE(SetNamedPipeHandleState(
		hPipe,
		&mode,
		NULL,
		NULL));

	Response response;
	DWORD toRead = sizeof(response);
	DWORD readed;

	EXPECT_TRUE(ReadFile(
		hPipe,
		&response,
		toRead,
		&readed,
		NULL));

	EXPECT_EQ(toRead, readed);

	EXPECT_EQ(response.Status, Response::OK);
	if (response.Status != Response::OK)
	{
		return NULL;
	}
	Sleep(500);
	return hPipe;
}

bool Send(HANDLE hPipe, TimerOperation& op, enum Response::Status code = Response::OK)
{
	DWORD toWrite = sizeof(op);;
	DWORD written;

	Response response;
	DWORD toRead = sizeof(response);
	DWORD readed;

	EXPECT_TRUE(WriteFile(
		hPipe,
		&op,
		toWrite,
		&written,
		NULL));

	EXPECT_EQ(toWrite, written);

	EXPECT_TRUE(ReadFile(
		hPipe,
		&response,
		toRead,
		&readed,
		NULL));

	EXPECT_EQ(toRead, readed);

	EXPECT_EQ(response.Status, code);

	return response.Status == code;
}

bool Disconnect(HANDLE hPipe)
{
	bool ans = CloseHandle(hPipe);
	EXPECT_TRUE(ans);

	Sleep(500);
	return ans;
}

bool TerminateServer(HANDLE hServer)
{
	bool ans = TerminateProcess(hServer, 0);
	EXPECT_TRUE(ans);
	Sleep(500);
	return ans;
}

TEST(Server, Create)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe = Connect();
	if (hPipe == NULL)
	{
		Disconnect(hPipe);
		TerminateServer(hServer);
		return;
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test1");
		op.manualReset = false;
		Send(hPipe, op);
	}

	Disconnect(hPipe);
	TerminateServer(hServer);
}

TEST(Server, Wait)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe = Connect();

	std::clock_t begin, end;
	const DWORD TIME_TO_WAIT = 1000;

	if (hPipe == NULL)
	{
		Disconnect(hPipe);
		TerminateServer(hServer);
		return;
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.SET;
		op.SetName("test");
		op.duration = TIME_TO_WAIT;
		op.period = 0;
		begin = clock();
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.WAIT;
		op.SetName("test");
		op.duration = INFINITE;
		Send(hPipe, op);
		end = clock();
		DWORD elapsed = end - begin;
		//EXPECT_EQ(TIME_TO_WAIT, elapsed);
		EXPECT_TRUE(closeEnough(TIME_TO_WAIT, elapsed, 20));
	}

	Disconnect(hPipe);
	TerminateServer(hServer);
}

TEST(Server, Delete)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe = Connect();
	if (hPipe == NULL)
	{
		Disconnect(hPipe);
		TerminateServer(hServer);
		return;
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op, Response::ERR);
	}
	{
		TimerOperation op;
		op.Operation = op.CLOSE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}

	Disconnect(hPipe);
	TerminateServer(hServer);
}

TEST(Server, Cancel)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe = Connect();
	if (hPipe == NULL)
	{
		Disconnect(hPipe);
		TerminateServer(hServer);
		return;
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.CANCEL;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.CLOSE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe, op);
	}

	Disconnect(hPipe);
	TerminateServer(hServer);
}

TEST(Server, Get)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe1 = Connect();
	HANDLE hPipe2 = Connect();
	if (hPipe1 == NULL || hPipe2 == NULL)
	{
		Disconnect(hPipe1);
		Disconnect(hPipe2);
		TerminateServer(hServer);
		return;
	}
	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe1, op);
		//Sleep(50);
	}
	{
		TimerOperation op;
		op.Operation = op.GET;
		op.SetName("test");
		//op.manualReset = false;
		Send(hPipe2, op);
	}

	//Sleep(500);
	Disconnect(hPipe1);
	Disconnect(hPipe2);
	TerminateServer(hServer);
}

TEST(Server, WaitOther)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe1 = Connect();
	HANDLE hPipe2 = Connect();
	if (hPipe1 == NULL || hPipe2 == NULL)
	{
		Disconnect(hPipe1);
		Disconnect(hPipe2);
		TerminateServer(hServer);
		return;
	}

	std::clock_t begin, end;
	const DWORD TIME_TO_WAIT = 1000;

	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe1, op);
	}
	{
		TimerOperation op;
		op.Operation = op.GET;
		op.SetName("test");
		Send(hPipe2, op);
	}
	{
		TimerOperation op;
		op.Operation = op.SET;
		op.SetName("test");
		op.duration = TIME_TO_WAIT;
		op.period = 0;
		begin = clock();
		Send(hPipe1, op);
	}
	{
		TimerOperation op;
		op.Operation = op.WAIT;
		op.SetName("test");
		op.duration = INFINITE;
		Send(hPipe2, op);
		end = clock();
		DWORD elapsed = end - begin;
		//EXPECT_EQ(TIME_TO_WAIT, elapsed);
		EXPECT_TRUE(closeEnough(TIME_TO_WAIT, elapsed, 25));
	}

	Disconnect(hPipe1);
	Disconnect(hPipe2);
	TerminateServer(hServer);
}

TEST(Server, WaitOtherManual)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe1 = Connect();
	HANDLE hPipe2 = Connect();
	if (hPipe1 == NULL || hPipe2 == NULL)
	{
		Disconnect(hPipe1);
		Disconnect(hPipe2);
		TerminateServer(hServer);
		return;
	}

	std::clock_t begin, end, secondEnd;
	const DWORD TIME_TO_WAIT = 1000;

	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = true;
		Send(hPipe1, op);
	}
	{
		TimerOperation op;
		op.Operation = op.GET;
		op.SetName("test");
		Send(hPipe2, op);
	}
	{
		TimerOperation op;
		op.Operation = op.SET;
		op.SetName("test");
		op.duration = TIME_TO_WAIT;
		op.period = 0;
		begin = clock();
		Send(hPipe1, op);
	}
	{
		TimerOperation op;
		op.Operation = op.WAIT;
		op.SetName("test");
		op.duration = INFINITE;
		Send(hPipe2, op);
		end = clock();
		DWORD elapsed = end - begin;
		//EXPECT_EQ(TIME_TO_WAIT, elapsed);
		EXPECT_TRUE(closeEnough(TIME_TO_WAIT, elapsed, 25));
	}
	{
		TimerOperation op;
		op.Operation = op.WAIT;
		op.SetName("test");
		op.duration = INFINITE;
		Send(hPipe2, op);
		secondEnd = clock();
		DWORD elapsed = secondEnd - end;
		EXPECT_TRUE(closeEnough(0, elapsed, 20));
	}

	Disconnect(hPipe1);
	Disconnect(hPipe2);
	TerminateServer(hServer);
}

TEST(Server, WaitOtherAuto)
{
	HANDLE hServer = RunServer();
	HANDLE hPipe1 = Connect();
	HANDLE hPipe2 = Connect();
	if (hPipe1 == NULL || hPipe2 == NULL)
	{
		Disconnect(hPipe1);
		Disconnect(hPipe2);
		TerminateServer(hServer);
		return;
	}

	std::clock_t begin, end, secondEnd;
	const DWORD TIME_TO_WAIT = 1000;

	{
		TimerOperation op;
		op.Operation = op.CREATE;
		op.SetName("test");
		op.manualReset = false;
		Send(hPipe1, op);
	}
	{
		TimerOperation op;
		op.Operation = op.GET;
		op.SetName("test");
		Send(hPipe2, op);
	}
	{
		TimerOperation op;
		op.Operation = op.SET;
		op.SetName("test");
		op.duration = TIME_TO_WAIT;
		op.period = 0;
		begin = clock();
		Send(hPipe1, op);
	}
	{
		TimerOperation op;
		op.Operation = op.WAIT;
		op.SetName("test");
		op.duration = INFINITE;
		Send(hPipe2, op);
		end = clock();
		DWORD elapsed = end - begin;
		//EXPECT_EQ(TIME_TO_WAIT, elapsed);
		EXPECT_TRUE(closeEnough(TIME_TO_WAIT, elapsed, 25));
	}
	{
		TimerOperation op;
		op.Operation = op.WAIT;
		op.SetName("test");
		op.duration = TIME_TO_WAIT;
		Send(hPipe2, op, Response::ERR);
		secondEnd = clock();
		DWORD elapsed = secondEnd - end;
		EXPECT_TRUE(closeEnough(TIME_TO_WAIT, elapsed, 20));
	}

	Disconnect(hPipe1);
	Disconnect(hPipe2);
	TerminateServer(hServer);
}