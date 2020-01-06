# YThreadpool
YThreadpool是封装了Windows线程池API的C++模板库。

**示例1** 使用线程池回调对象
```cpp
void example1()
{
    YThreadpoolSimpleCallback::TrySubmit(nullptr, [](PTP_CALLBACK_INSTANCE)
    {
        Sleep(500);
        fprintf_s(stdout, "[%I64d] YThreadpoolSimpleCallback, thread id : %d\r\n", GetTickCount64(), GetCurrentThreadId());
    });
    
    YThreadpoolTimer* timerObject = YThreadpoolTimer::Create(nullptr, [](long& counter, PTP_CALLBACK_INSTANCE, YThreadpoolTimer* timer)
    {
        long count = InterlockedIncrement(&counter);
        fprintf_s(stdout, "[%I64d] YThreadpoolTimer, count : %d, thread id : %d\r\n", GetTickCount64(), count, GetCurrentThreadId());
        if (count == 3)
        {
            timer->Close();
        }
    }, long(0));
    if (timerObject)
        timerObject->SetTimer(1000, 0);
}
```

**示例2** 优雅地退出回调函数

```cpp
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
```

**示例3** 使用清理组管理回调对象的生命周期；在单线程回调环境中访问共享数据

```cpp
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
    // 创建多个工作对象并提交
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
                // 在单线程回调环境中访问numbers
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

    // 主线程继续执行自己的任务，与线程池的工作线程并发运行
    Sleep(10);
    
    // 等待添加到清理组的回调对象执行完成，并释放它们
    fprintf_s(stdout, "[%I64d] CloseThreadpoolCleanupGroupMembers\r\n", GetTickCount64());
    CloseThreadpoolCleanupGroupMembers(cleanupGroup, FALSE, nullptr);
    /*
     * 上一次调用CloseThreadpoolCleanupGroupMembers时，可能有某些YThreadpoolWork对象尚未完成，
     * 它调用YThreadpoolSimpleCallback::TrySubmit创建回调对象并将之添加到清理组，
     * 现在等待清理组中的回调对象执行完成，并释放它们
    */
    fprintf_s(stdout, "[%I64d] CloseThreadpoolCleanupGroupMembers\r\n", GetTickCount64());
    CloseThreadpoolCleanupGroupMembers(cleanupGroup, FALSE, nullptr);
    CloseThreadpoolCleanupGroup(cleanupGroup);
    singleThreadedEnviron->Destroy();

    fprintf_s(stdout, "[%I64d] numbers.size : %d\r\n", GetTickCount64(), (int)numbers.size());
}
```

