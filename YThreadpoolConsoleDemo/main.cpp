
#include <Windows.h>
#include <iostream>
#include <vector>
#include <ctime>
using namespace std;
#include "..\YThreadpool\include\YThreadpool.h"

// ʾ��1��ʹ���̳߳ػص�����
void example1()
{
	YThreadpoolSimpleCallback::TrySubmit(nullptr, [](PTP_CALLBACK_INSTANCE)
	{
		Sleep(500);
		fprintf_s(stdout, "[%I64d] YThreadpoolSimpleCallback, thread id : %d\r\n", GetTickCount64(), GetCurrentThreadId());
	});

	const long CountOfWorkInstances = 3;
	YThreadpoolWork* workObject = YThreadpoolWork::Create(nullptr, [=](long& counter, PTP_CALLBACK_INSTANCE, YThreadpoolWork* work)
	{
		long index = InterlockedIncrement(&counter);
		Sleep(index * 500);
		fprintf_s(stdout, "[%I64d] YThreadpoolWork, index : %d, thread id : %d\r\n", GetTickCount64(), index, GetCurrentThreadId());
		if (index == CountOfWorkInstances)
			work->Close();
	}, (long)0);
	if (workObject)
	{
		for (int i = 0; i < CountOfWorkInstances; ++i)
			workObject->Submit();
	}

	HANDLE hEvent = nullptr;
	YThreadpoolWait* waitObject = YThreadpoolWait::Create(nullptr, [=](PTP_CALLBACK_INSTANCE, YThreadpoolWait* wait, TP_WAIT_RESULT waitResult)
	{
		wait->Close();
		CloseHandle(hEvent);
		fprintf_s(stdout, "[%I64d] YThreadpoolWait, WaitResult : %d, thread id : %d\r\n", GetTickCount64(), waitResult, GetCurrentThreadId());
	});
	if (waitObject)
	{
		hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		waitObject->SetWait(hEvent, INFINITE);
	}

	YThreadpoolTimer* timerObject = YThreadpoolTimer::Create(nullptr, [=](long& counter, PTP_CALLBACK_INSTANCE, YThreadpoolTimer* timer)
	{
		long count = InterlockedIncrement(&counter);
		fprintf_s(stdout, "[%I64d] YThreadpoolTimer, count : %d, thread id : %d\r\n", GetTickCount64(), count, GetCurrentThreadId());
		if (count == 3)
		{
			timer->Close();
			SetEvent(hEvent);
		}
		Sleep(1009);
	}, long(0));
	if (timerObject)
		timerObject->SetTimer(1000, 0);
		
	for (int i = 0; i < 6; ++i)
	{
		Sleep(1000);
		fprintf_s(stdout, "[%I64d] Main thread, i : %d, thread id : %d\r\n", GetTickCount64(), i, GetCurrentThreadId());
	}
}

// ʾ��2�����ŵ��˳��ص�����
void example2()
{
	YThreadpoolWork* workObject = YThreadpoolWork::Create(nullptr, [=](PTP_CALLBACK_INSTANCE, YThreadpoolWork* work){
		for (int i = 0; i < INT_MAX; ++i)
		{
			if (work->ShouldExit())
			{
				fprintf_s(stdout, "Exit\r\n");
				break;
			}
			fprintf_s(stdout, "i : %d\r\n", i);
			Sleep(1000);
		}
	});
	if (workObject)
	{
		workObject->Submit();
		Sleep(2100);
		workObject->Exit();
		workObject->Wait(FALSE);
		workObject->Close();
	}
}

// ʾ��3��ʹ�����������ص�������������ڣ��ڵ��̻߳ص������з��ʹ�������
void example3()
{
	auto cleanupGroup = CreateThreadpoolCleanupGroup();
	auto cleanupGroupCallback = [](YThreadpoolCallback* /*CallbackObject*/, PVOID /*CleanupContext*/)
	{
		fprintf_s(stdout, "Callback object is cancelled.\r\n");
	};
	YTPConcurrenceEnviron concurrenceEnviron;
	concurrenceEnviron.SetCleanupGroup<cleanupGroupCallback>(cleanupGroup);
	YTPSingleThreadedEnviron* singleThreadedEnviron = YTPSingleThreadedEnviron::Create();
	singleThreadedEnviron->SetCleanupGroupWith(concurrenceEnviron);

	vector<int> numbers;
	const int CountOfWorkObjects = 2;
	const int CountOfInstances = 3;
	const int N = 1000 * 100;
	// ����������������ύ
	for (int indexOfObject = 1; indexOfObject <= CountOfWorkObjects; ++indexOfObject)
	{
		auto func = [=, &numbers](long& counter, PTP_CALLBACK_INSTANCE, YThreadpoolWork*)
		{
			long indexOfInstance = InterlockedIncrement(&counter);
			fprintf_s(stdout, "------>  index of object : %d, index of instance : %d\r\n", indexOfObject, indexOfInstance);
			srand((unsigned int)time(nullptr));
			for (int i = 0; i < N; ++i)
			{
				int number = rand() * 100 + indexOfObject * 10 + indexOfInstance;
				// �ڵ��̻߳ص������з���numbers
				YThreadpoolSimpleCallback::TrySubmit(singleThreadedEnviron, [=, &numbers](PTP_CALLBACK_INSTANCE)
				{
					numbers.push_back(number);
				});
			}
			fprintf_s(stdout, "<------  index of object : %d, index of instance : %d\r\n", indexOfObject, indexOfInstance);
		};
		YThreadpoolWork* work = YThreadpoolWork::Create(&concurrenceEnviron, func, (long)0);
		if (work)
		{
			for (int i = 0; i < CountOfInstances; ++i)
				work->Submit();
		}
	}

	// ���̼߳���ִ���Լ����������̳߳صĹ����̲߳�������
	Sleep(10);

	// �ȴ���ӵ�������Ļص�����ִ����ɣ����ͷ�����
	fprintf_s(stdout, "[%I64d] CloseThreadpoolCleanupGroupMembers\r\n", GetTickCount64());
	CloseThreadpoolCleanupGroupMembers(cleanupGroup, FALSE, nullptr);
	/*
	 * ��һ�ε���CloseThreadpoolCleanupGroupMembersʱ��������ĳЩYThreadpoolWork������δ��ɣ�
	 * ������YThreadpoolSimpleCallback::TrySubmit�����ص����󲢽�֮��ӵ������飬
	 * ���ڵȴ��������еĻص�����ִ����ɣ����ͷ�����
	*/
	fprintf_s(stdout, "[%I64d] CloseThreadpoolCleanupGroupMembers\r\n", GetTickCount64());
	CloseThreadpoolCleanupGroupMembers(cleanupGroup, FALSE, nullptr);
	CloseThreadpoolCleanupGroup(cleanupGroup);
	singleThreadedEnviron->Destroy();

	fprintf_s(stdout, "[%I64d] numbers.size : %d\r\n", GetTickCount64(), (int)numbers.size());
}

int main()
{
	void (*examples[])(void) = {example1, example2, example3, nullptr};
	for (int i = 0; examples[i]; ++i)
	{
		fprintf_s(stdout, "example%d : \r\n", i + 1);
		examples[i]();
		cout << endl;
	}
}

